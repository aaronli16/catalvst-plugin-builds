#include "PluginEditor.h"
#include "BinaryData.h"
#include <unordered_map>

// Sanitize a parameter name to be a valid JUCE Identifier (alphanumeric + underscore only).
// "Decay Rate" → "Decay_Rate", "Dry/Wet Mix" → "Dry_Wet_Mix"
// Must match the JS-side sanitization exactly.
static juce::String sanitizeForRelay (const juce::String& name)
{
    juce::String result;
    for (int i = 0; i < name.length(); i++)
    {
        auto c = name[i];
        if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '_')
            result += c;
        else if (c == ' ' || c == '/' || c == '-' || c == '(' || c == ')')
            result += '_';
        // skip other special chars
    }
    return result;
}

// MIME type lookup
static const char* getMimeForExtension (const juce::String& extension)
{
    static const std::unordered_map<juce::String, const char*> mimeMap = {
        { "html", "text/html" },
        { "htm",  "text/html" },
        { "css",  "text/css" },
        { "js",   "text/javascript" },
        { "json", "application/json" },
        { "png",  "image/png" },
        { "jpg",  "image/jpeg" },
        { "svg",  "image/svg+xml" },
        { "woff2","font/woff2" }
    };

    if (const auto it = mimeMap.find (extension.toLowerCase()); it != mimeMap.end())
        return it->second;

    return "application/octet-stream";
}

// Inline JS bridge injected before </body>. Polyfills the JUCE Backend event bus
// (ES modules fail silently in JUCE WebView on macOS, so we can't import index.js).
static const char* dataParamBridgeJS = R"JS(
<script>
(function() {
    if (!window.__JUCE__) return;

    // Backend polyfill — normally created by check_native_interop.js (via ES module),
    // but modules fail in JUCE WebView on macOS.
    if (!window.__JUCE__.backend) {
        var _listeners = {};
        window.__JUCE__.backend = {
            addEventListener: function(eventId, fn) {
                if (!_listeners[eventId]) _listeners[eventId] = [];
                _listeners[eventId].push(fn);
            },
            emitEvent: function(eventId, object) {
                window.__JUCE__.postMessage(JSON.stringify({ eventId: eventId, payload: object }));
            },
            emitByBackend: function(eventId, objectOrString) {
                var obj = typeof objectOrString === 'string' ? JSON.parse(objectOrString) : objectOrString;
                var fns = _listeners[eventId];
                if (fns) {
                    for (var i = 0; i < fns.length; i++) {
                        try { fns[i](obj); } catch(e) { console.error(e); }
                    }
                }
            }
        };
    }

    var backend = window.__JUCE__.backend;
    var knobs = document.querySelectorAll('[data-param]');

    knobs.forEach(function(el) {
        var rawName = el.getAttribute('data-param');
        var safeName = rawName.replace(/[^a-zA-Z0-9_]/g, '_');
        var eventId = '__juce__slider' + safeName;
        var start = 0, end = 1;

        try {
            backend.addEventListener(eventId, function(event) {
                if (event.eventType === 'valueChanged' && event.value !== undefined) {
                    var range = end - start;
                    var norm = range > 0 ? (event.value - start) / range : 0;
                    el.value = Math.max(0, Math.min(1, norm));
                } else if (event.eventType === 'propertiesChanged' && event.properties) {
                    start = event.properties.start !== undefined ? event.properties.start : 0;
                    end = event.properties.end !== undefined ? event.properties.end : 1;
                }
            });

            backend.emitEvent(eventId, { eventType: 'requestInitialUpdate' });

            el.addEventListener('input', function() {
                var norm = parseFloat(el.value);
                backend.emitEvent(eventId, { eventType: 'valueChanged', value: start + norm * (end - start) });
            });
            el.addEventListener('mousedown', function() {
                backend.emitEvent(eventId, { eventType: 'sliderDragStarted' });
            });
            el.addEventListener('mouseup', function() {
                backend.emitEvent(eventId, { eventType: 'sliderDragEnded' });
            });
        } catch (e) {
            console.error('[CatalvstBridge]', safeName, e);
        }
    });
})();
</script>
)JS";

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), processorRef (p)
{
    // Step 1: Create WebSliderRelay for each Faust parameter.
    // Relay names MUST be sanitized — JUCE Identifier only allows alphanumeric + underscore.
    const auto& labels = processorRef.getParamLabels();

    for (const auto& label : labels)
        sliderRelays.push_back (std::make_unique<juce::WebSliderRelay> (sanitizeForRelay (label)));

    // Step 2: Build WebBrowserComponent options — chain all relays via withOptionsFrom.
    // Matches the proven pattern from working plugins (Half Beat Echo).
    auto options = juce::WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withResourceProvider (
            [this] (const auto& url) { return getResource (url); },
            juce::URL { "http://localhost" }.getOrigin());

    for (auto& relay : sliderRelays)
        options = options.withOptionsFrom (*relay);

    // Step 3: Create the WebBrowserComponent with assembled options
    webBrowser = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*webBrowser);

    // Step 4: Create attachments (bidirectional sync: JUCE param ↔ relay)
    auto& params = processorRef.getParameters();
    int relayIdx = 0;

    for (auto* param : params)
    {
        if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            if (relayIdx < static_cast<int> (sliderRelays.size()))
            {
                sliderAttachments.push_back (
                    std::make_unique<juce::WebSliderParameterAttachment> (
                        *rangedParam, *sliderRelays[static_cast<size_t> (relayIdx)], nullptr));
                relayIdx++;
            }
        }
    }

    // Step 5: Navigate to the embedded HTML
    webBrowser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setSize (800, 540);
    setResizable (false, false);
}

PluginEditor::~PluginEditor() {}

void PluginEditor::resized()
{
    if (webBrowser)
        webBrowser->setBounds (getLocalBounds());
}

// Resource provider — serves files to the WebView.
// JUCE bridge files use HARDCODED paths (proven pattern from working plugins).
// User files (HTML/CSS) use dynamic BinaryData lookup.
std::optional<PluginEditor::Resource>
    PluginEditor::getResource (const juce::String& url) const
{
    auto makeResource = [] (const char* data, int size, const char* mime)
        -> std::optional<Resource>
    {
        if (data == nullptr || size == 0) return std::nullopt;
        return Resource {
            std::vector<std::byte> (
                reinterpret_cast<const std::byte*> (data),
                reinterpret_cast<const std::byte*> (data) + size),
            juce::String (mime)
        };
    };

    // Hardcoded JUCE bridge files (same pattern as working Half Beat Echo plugin)
    if (url == "/js/juce/index.js")
        return makeResource (BinaryData::index_js, BinaryData::index_jsSize, "text/javascript");

    if (url == "/js/juce/check_native_interop.js")
        return makeResource (BinaryData::check_native_interop_js, BinaryData::check_native_interop_jsSize, "text/javascript");

    // Main HTML — inject the data-param bridge shim before </body>
    if (url == "/" || url == "/index.html")
    {
        auto html = juce::String::fromUTF8 (BinaryData::index_html, BinaryData::index_htmlSize);

        if (html.contains ("</body>"))
            html = html.replace ("</body>", juce::String (dataParamBridgeJS) + "\n</body>");
        else
            html += dataParamBridgeJS;

        auto utf8 = html.toUTF8();
        return Resource {
            std::vector<std::byte> (
                reinterpret_cast<const std::byte*> (utf8.getAddress()),
                reinterpret_cast<const std::byte*> (utf8.getAddress()) + utf8.sizeInBytes() - 1),
            juce::String ("text/html")
        };
    }

    // CSS files
    if (url.endsWithIgnoreCase (".css"))
    {
        // Convert filename to BinaryData identifier: "styles.css" → "styles_css"
        auto filename = url.fromLastOccurrenceOf ("/", false, false);
        auto identifier = filename.replace (".", "_").replace ("-", "");

        int dataSize = 0;
        if (auto* data = BinaryData::getNamedResource (identifier.toRawUTF8(), dataSize))
            return makeResource (data, dataSize, "text/css");
    }

    return std::nullopt;
}

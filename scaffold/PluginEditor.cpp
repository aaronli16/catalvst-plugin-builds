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

// JS shim injected into the HTML.
// Uses the PROVEN pattern from working plugins: import getSliderState from JUCE bridge module.
// Sanitizes data-param names the same way C++ sanitizes relay names.
static const char* dataParamBridgeJS = R"JS(
<script type="module">
import { getSliderState } from './js/juce/index.js';

function sanitizeName(name) {
    return name.replace(/[^a-zA-Z0-9_]/g, '_');
}

console.log('[CatalvstBridge] Wiring', document.querySelectorAll('[data-param]').length, 'params');

document.querySelectorAll('[data-param]').forEach(function(el) {
    var rawName = el.getAttribute('data-param');
    var safeName = sanitizeName(rawName);

    try {
        var state = getSliderState(safeName);
        if (!state) {
            console.warn('[CatalvstBridge] No relay found for:', safeName);
            return;
        }

        // UI knob → DAW parameter
        el.addEventListener('input', function() {
            state.setNormalisedValue(parseFloat(el.value));
        });

        // DAW automation → UI knob
        state.valueChangedEvent.addListener(function() {
            el.value = state.getNormalisedValue();
        });

        // Set initial value from DAW parameter
        el.value = state.getNormalisedValue();
        console.log('[CatalvstBridge] Wired:', safeName);
    } catch (e) {
        console.error('[CatalvstBridge] Failed to wire', safeName, e);
    }
});
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

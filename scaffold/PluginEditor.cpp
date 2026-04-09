#include "PluginEditor.h"
#include "BinaryData.h"
#include <unordered_map>

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

// JS shim injected into the HTML — wires [data-param] elements to JUCE WebSliderRelay.
// Makes the same HTML work in browser (Faust bridge.ts) and DAW (JUCE relay).
static const char* dataParamBridgeJS = R"JS(
<script type="module">
import * as Juce from "./js/juce/index.js";

function wireParams() {
    document.querySelectorAll('[data-param]').forEach(function(el) {
        var name = el.getAttribute('data-param');
        var state = Juce.getSliderState(name);
        if (!state) {
            console.warn('[CatalvstBridge] No relay found for data-param="' + name + '"');
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
    });
}

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', wireParams);
} else {
    wireParams();
}
</script>
)JS";

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), processorRef (p)
{
    // Step 1: Create WebSliderRelay for each Faust parameter
    // Relay name = Faust param label (matches data-param in HTML and JS getSliderState call)
    const auto& labels = processorRef.getParamLabels();

    for (const auto& label : labels)
        sliderRelays.push_back (std::make_unique<juce::WebSliderRelay> (label));

    // Step 2: Build WebBrowserComponent options — chain all relays via withOptionsFrom
    auto options = juce::WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()
        .withResourceProvider (
            [this] (const auto& url) { return getResource (url); });

    for (auto& relay : sliderRelays)
        options = options.withOptionsFrom (*relay);

    // Step 3: Create the WebBrowserComponent with assembled options
    webBrowser = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*webBrowser);

    // Step 4: Create attachments (bidirectional sync: JUCE param <-> relay)
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

// Resource provider — serves BinaryData files to the WebView.
// BinaryData is generated at compile time by juce_add_binary_data (CMakeLists.txt).
std::optional<PluginEditor::Resource>
    PluginEditor::getResource (const juce::String& url) const
{
    auto path = url;
    if (path == "/" || path.isEmpty())
        path = "/index.html";

    // Strip leading slash
    auto filename = path.fromFirstOccurrenceOf ("/", false, false);

    // Search BinaryData for a matching file
    for (int i = 0; i < BinaryData::namedResourceListSize; i++)
    {
        // BinaryData original filenames are stored in namedResourceList
        juce::String resourceName (BinaryData::namedResourceList[i]);

        // Get the original filename for this resource
        int dataSize = 0;
        const char* data = BinaryData::getNamedResource (
            BinaryData::namedResourceList[i], dataSize);

        if (data == nullptr || dataSize == 0)
            continue;

        // Match by original filename (BinaryData stores the original names)
        juce::String originalFile (BinaryData::originalFilenames[i]);

        // Compare: strip path prefix to match just the filename
        if (originalFile.endsWith (filename) || originalFile == filename)
        {
            auto extension = filename.fromLastOccurrenceOf (".", false, false);
            auto mimeType = juce::String (getMimeForExtension (extension));

            // For HTML files, inject the data-param bridge JS shim before </body>
            if (mimeType == "text/html")
            {
                auto html = juce::String::fromUTF8 (data, dataSize);

                if (html.contains ("</body>"))
                    html = html.replace ("</body>", juce::String (dataParamBridgeJS) + "\n</body>");
                else
                    html += dataParamBridgeJS;

                auto utf8 = html.toUTF8();
                return Resource {
                    std::vector<std::byte> (
                        reinterpret_cast<const std::byte*> (utf8.getAddress()),
                        reinterpret_cast<const std::byte*> (utf8.getAddress()) + utf8.sizeInBytes() - 1),
                    mimeType
                };
            }

            return Resource {
                std::vector<std::byte> (
                    reinterpret_cast<const std::byte*> (data),
                    reinterpret_cast<const std::byte*> (data) + dataSize),
                mimeType
            };
        }
    }

    return std::nullopt;
}

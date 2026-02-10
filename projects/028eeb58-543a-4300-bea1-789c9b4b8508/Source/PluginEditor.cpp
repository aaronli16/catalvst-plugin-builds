#include "PluginEditor.h"
#include "BinaryData.h"

// ==============================================================================
// Constructor
// ==============================================================================

OneKnobSmoothReverbAudioProcessorEditor::OneKnobSmoothReverbAudioProcessorEditor(OneKnobSmoothReverbAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // --------------------------------------------------------------------------
    // STEP 1: CREATE RELAYS (before WebView!)
    // --------------------------------------------------------------------------
    wetRelay = std::make_unique<juce::WebSliderRelay>("WET");

    // --------------------------------------------------------------------------
    // STEP 2: CREATE WEBVIEW (with relay options)
    // --------------------------------------------------------------------------
    webView = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withResourceProvider([this](const auto& url) {
                return getResource(url);
            })
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withOptionsFrom(*wetRelay)
    );

    // --------------------------------------------------------------------------
    // STEP 3: CREATE PARAMETER ATTACHMENTS (after WebView!)
    // --------------------------------------------------------------------------
    wetAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("WET"),
        *wetRelay,
        nullptr
    );

    // --------------------------------------------------------------------------
    // WEBVIEW SETUP
    // --------------------------------------------------------------------------
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    addAndMakeVisible(*webView);

    // --------------------------------------------------------------------------
    // WINDOW SIZE
    // --------------------------------------------------------------------------
    setSize(400, 500);
    setResizable(false, false);
}

// ==============================================================================
// Destructor
// ==============================================================================

OneKnobSmoothReverbAudioProcessorEditor::~OneKnobSmoothReverbAudioProcessorEditor()
{
}

// ==============================================================================
// Paint & Resize
// ==============================================================================

void OneKnobSmoothReverbAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void OneKnobSmoothReverbAudioProcessorEditor::resized()
{
    webView->setBounds(getLocalBounds());
}

// ==============================================================================
// Resource Provider
// ==============================================================================

std::optional<juce::WebBrowserComponent::Resource>
OneKnobSmoothReverbAudioProcessorEditor::getResource(const juce::String& url)
{
    auto makeVector = [](const char* data, int size) {
        return std::vector<std::byte>(
            reinterpret_cast<const std::byte*>(data),
            reinterpret_cast<const std::byte*>(data) + size
        );
    };

    if (url == "/" || url == "/index.html") {
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::index_html, BinaryData::index_htmlSize),
            juce::String("text/html")
        };
    }

    if (url == "/js/juce/index.js") {
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::index_js, BinaryData::index_jsSize),
            juce::String("text/javascript")
        };
    }

    if (url == "/js/juce/check_native_interop.js") {
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::check_native_interop_js, BinaryData::check_native_interop_jsSize),
            juce::String("text/javascript")
        };
    }

    return std::nullopt;
}

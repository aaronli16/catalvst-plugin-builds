#include "PluginEditor.h"
#include "BinaryData.h"

// ==============================================================================
// Constructor
// ==============================================================================

SimpleHalfbeatDelayAudioProcessorEditor::SimpleHalfbeatDelayAudioProcessorEditor(SimpleHalfbeatDelayAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // --------------------------------------------------------------------------
    // STEP 1: CREATE RELAYS (before WebView!)
    // --------------------------------------------------------------------------
    mixRelay = std::make_unique<juce::WebSliderRelay>("MIX");
    feedbackRelay = std::make_unique<juce::WebSliderRelay>("FEEDBACK");

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
            .withOptionsFrom(*mixRelay)
            .withOptionsFrom(*feedbackRelay)
    );

    // --------------------------------------------------------------------------
    // STEP 3: CREATE PARAMETER ATTACHMENTS (after WebView!)
    // --------------------------------------------------------------------------
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("MIX"),
        *mixRelay,
        nullptr
    );

    feedbackAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("FEEDBACK"),
        *feedbackRelay,
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
    setSize(400, 300);
    setResizable(false, false);
}

// ==============================================================================
// Destructor
// ==============================================================================

SimpleHalfbeatDelayAudioProcessorEditor::~SimpleHalfbeatDelayAudioProcessorEditor()
{
}

// ==============================================================================
// Paint & Resize
// ==============================================================================

void SimpleHalfbeatDelayAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void SimpleHalfbeatDelayAudioProcessorEditor::resized()
{
    webView->setBounds(getLocalBounds());
}

// ==============================================================================
// Resource Provider
// ==============================================================================

std::optional<juce::WebBrowserComponent::Resource>
SimpleHalfbeatDelayAudioProcessorEditor::getResource(const juce::String& url)
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

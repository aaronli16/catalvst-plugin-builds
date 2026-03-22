#include "PluginEditor.h"
#include "BinaryData.h"

// ==============================================================================
// Constructor - CRITICAL: Initialize in correct order
// ==============================================================================

FlutterAudioProcessorEditor::FlutterAudioProcessorEditor(FlutterAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // ==========================================================================
    // INITIALIZATION SEQUENCE (CRITICAL ORDER)
    // ==========================================================================
    //
    // 1. Create relays FIRST (before WebView construction)
    // 2. Create WebView with relay options
    // 3. Create parameter attachments LAST (after WebView construction)
    //
    // ==========================================================================

    // --------------------------------------------------------------------------
    // STEP 1: CREATE RELAYS (before WebView!)
    // --------------------------------------------------------------------------
    timeRelay     = std::make_unique<juce::WebSliderRelay>("TIME");
    feedbackRelay = std::make_unique<juce::WebSliderRelay>("FEEDBACK");
    flutterRelay  = std::make_unique<juce::WebSliderRelay>("FLUTTER");
    tapeAgeRelay  = std::make_unique<juce::WebSliderRelay>("TAPE_AGE");
    toneRelay     = std::make_unique<juce::WebSliderRelay>("TONE");
    mixRelay      = std::make_unique<juce::WebSliderRelay>("MIX");

    // --------------------------------------------------------------------------
    // STEP 2: CREATE WEBVIEW (with relay options)
    // --------------------------------------------------------------------------
    webView = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
            // REQUIRED: Enable JUCE frontend library
            .withNativeIntegrationEnabled()

            // REQUIRED: Resource provider for embedded files
            .withResourceProvider([this](const auto& url) {
                return getResource(url);
            })

            // OPTIONAL: FL Studio fix (prevents blank screen on focus loss)
            .withKeepPageLoadedWhenBrowserIsHidden()

            .withOptionsFrom(*timeRelay)
            .withOptionsFrom(*feedbackRelay)
            .withOptionsFrom(*flutterRelay)
            .withOptionsFrom(*tapeAgeRelay)
            .withOptionsFrom(*toneRelay)
            .withOptionsFrom(*mixRelay)
    );

    // --------------------------------------------------------------------------
    // STEP 3: CREATE PARAMETER ATTACHMENTS (after WebView!)
    // --------------------------------------------------------------------------
    timeAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("TIME"), *timeRelay, nullptr);
    feedbackAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("FEEDBACK"), *feedbackRelay, nullptr);
    flutterAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("FLUTTER"), *flutterRelay, nullptr);
    tapeAgeAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("TAPE_AGE"), *tapeAgeRelay, nullptr);
    toneAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("TONE"), *toneRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("MIX"), *mixRelay, nullptr);

    // --------------------------------------------------------------------------
    // WEBVIEW SETUP
    // --------------------------------------------------------------------------
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    addAndMakeVisible(*webView);

    // --------------------------------------------------------------------------
    // WINDOW SIZE
    // --------------------------------------------------------------------------
    // ===== PLUGIN WINDOW SIZE (auto-set from dimension tier) =====
    setSize(800, 540);
    setResizable(false, false);
}

// ==============================================================================
// Destructor
// ==============================================================================

FlutterAudioProcessorEditor::~FlutterAudioProcessorEditor()
{
    // Members automatically destroyed in reverse order:
    // 1. Attachments (stop calling evaluateJavascript)
    // 2. webView (safe, attachments are gone)
    // 3. Relays (safe, nothing using them)
}

// ==============================================================================
// AudioProcessorEditor Overrides
// ==============================================================================

void FlutterAudioProcessorEditor::paint(juce::Graphics& g)
{
    // WebView fills the entire editor, no custom painting needed
    juce::ignoreUnused(g);
}

void FlutterAudioProcessorEditor::resized()
{
    // Make WebView fill the entire editor bounds
    webView->setBounds(getLocalBounds());
}

// ==============================================================================
// Resource Provider - Serves embedded HTML/CSS/JS to WebView
// ==============================================================================

std::optional<juce::WebBrowserComponent::Resource>
FlutterAudioProcessorEditor::getResource(const juce::String& url)
{
    // Helper lambda to convert raw binary data to vector<byte>
    auto makeVector = [](const char* data, int size) {
        return std::vector<std::byte>(
            reinterpret_cast<const std::byte*>(data),
            reinterpret_cast<const std::byte*>(data) + size
        );
    };

    // --------------------------------------------------------------------------
    // Serve index.html (main UI)
    // --------------------------------------------------------------------------
    if (url == "/" || url == "/index.html") {
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::index_html, BinaryData::index_htmlSize),
            juce::String("text/html")
        };
    }

    // --------------------------------------------------------------------------
    // JUCE Frontend Library (required for parameter binding)
    // --------------------------------------------------------------------------
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

    // ===== TODO: ADD ADDITIONAL RESOURCES HERE =====
    //
    // If you have additional CSS or JS files:
    //   if (url == "/styles.css") {
    //       return juce::WebBrowserComponent::Resource {
    //           makeVector(BinaryData::styles_css, BinaryData::styles_cssSize),
    //           juce::String("text/css")
    //       };
    //   }
    //
    // ================================================

    // 404 - Resource not found
    return std::nullopt;
}

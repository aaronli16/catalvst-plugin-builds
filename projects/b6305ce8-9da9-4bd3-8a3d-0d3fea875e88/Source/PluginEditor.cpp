#include "PluginEditor.h"
#include "BinaryData.h"

// ==============================================================================
// Constructor - CRITICAL: Initialize in correct order
// ==============================================================================

CrunchAudioProcessorEditor::CrunchAudioProcessorEditor(CrunchAudioProcessor& p)
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
    bitsRelay = std::make_unique<juce::WebSliderRelay>("BITS");
    crushRelay = std::make_unique<juce::WebSliderRelay>("CRUSH");
    mixRelay = std::make_unique<juce::WebSliderRelay>("MIX");

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

            .withOptionsFrom(*bitsRelay)
            .withOptionsFrom(*crushRelay)
            .withOptionsFrom(*mixRelay)
    );

    // --------------------------------------------------------------------------
    // STEP 3: CREATE PARAMETER ATTACHMENTS (after WebView!)
    // --------------------------------------------------------------------------
    bitsAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("BITS"), *bitsRelay, nullptr);
    crushAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("CRUSH"), *crushRelay, nullptr);
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
    setSize(600, 400);
    setResizable(false, false);
}

// ==============================================================================
// Destructor
// ==============================================================================

CrunchAudioProcessorEditor::~CrunchAudioProcessorEditor()
{
    // Members automatically destroyed in reverse order:
    // 1. Attachments (stop calling evaluateJavascript)
    // 2. webView (safe, attachments are gone)
    // 3. Relays (safe, nothing using them)
}

// ==============================================================================
// AudioProcessorEditor Overrides
// ==============================================================================

void CrunchAudioProcessorEditor::paint(juce::Graphics& g)
{
    // WebView fills the entire editor, no custom painting needed
    juce::ignoreUnused(g);
}

void CrunchAudioProcessorEditor::resized()
{
    // Make WebView fill the entire editor bounds
    webView->setBounds(getLocalBounds());
}

// ==============================================================================
// Resource Provider - Serves embedded HTML/CSS/JS to WebView
// ==============================================================================

std::optional<juce::WebBrowserComponent::Resource>
CrunchAudioProcessorEditor::getResource(const juce::String& url)
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

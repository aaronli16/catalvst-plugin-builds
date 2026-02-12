#include "PluginEditor.h"
#include "BinaryData.h"

// ==============================================================================
// Constructor
// ==============================================================================

AmbientSmearEffectAudioProcessorEditor::AmbientSmearEffectAudioProcessorEditor(AmbientSmearEffectAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // STEP 1: Create relays FIRST
    intensityRelay = std::make_unique<juce::WebSliderRelay>("INTENSITY");

    // STEP 2: Create WebView with relay options
    webView = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withResourceProvider([this](const auto& url) {
                return getResource(url);
            })
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withOptionsFrom(*intensityRelay)
    );

    // STEP 3: Create parameter attachments LAST
    intensityAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("INTENSITY"),
        *intensityRelay,
        nullptr
    );

    // WebView setup
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    addAndMakeVisible(*webView);

    // Window size
    setSize(400, 500);
    setResizable(false, false);
}

// ==============================================================================
// Destructor
// ==============================================================================

AmbientSmearEffectAudioProcessorEditor::~AmbientSmearEffectAudioProcessorEditor()
{
}

// ==============================================================================
// Paint & Resize
// ==============================================================================

void AmbientSmearEffectAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void AmbientSmearEffectAudioProcessorEditor::resized()
{
    webView->setBounds(getLocalBounds());
}

// ==============================================================================
// Resource Provider
// ==============================================================================

std::optional<juce::WebBrowserComponent::Resource>
AmbientSmearEffectAudioProcessorEditor::getResource(const juce::String& url)
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

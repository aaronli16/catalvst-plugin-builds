#include "PluginEditor.h"
#include "BinaryData.h"

OneKnobReverbAudioProcessorEditor::OneKnobReverbAudioProcessorEditor(OneKnobReverbAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // STEP 1: Create relays FIRST
    wetRelay = std::make_unique<juce::WebSliderRelay>("WET");

    // STEP 2: Create WebView with relay options
    webView = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withResourceProvider([this](const auto& url) {
                return getResource(url);
            })
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withOptionsFrom(*wetRelay)
    );

    // STEP 3: Create attachments LAST
    wetAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("WET"),
        *wetRelay,
        nullptr
    );

    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    addAndMakeVisible(*webView);

    setSize(600, 400);
    setResizable(false, false);
}

OneKnobReverbAudioProcessorEditor::~OneKnobReverbAudioProcessorEditor()
{
}

void OneKnobReverbAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void OneKnobReverbAudioProcessorEditor::resized()
{
    webView->setBounds(getLocalBounds());
}

std::optional<juce::WebBrowserComponent::Resource>
OneKnobReverbAudioProcessorEditor::getResource(const juce::String& url)
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

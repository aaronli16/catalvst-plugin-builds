#include "PluginProcessor.h"
#include "PluginEditor.h"

// Build JUCE parameter layout by enumerating Faust params via APIUI.
// Called once from constructor initializer list.
juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    // Temporary DSP + APIUI just for parameter enumeration
    FaustDSP tempDSP;
    tempDSP.init (44100);

    APIUI apiUI;
    tempDSP.buildUserInterface (&apiUI);

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < apiUI.getParamsCount(); i++)
    {
        juce::String address (apiUI.getParamAddress (i));
        juce::String label (apiUI.getParamLabel (i));
        float minVal = static_cast<float> (apiUI.getParamMin (i));
        float maxVal = static_cast<float> (apiUI.getParamMax (i));
        float defVal = static_cast<float> (apiUI.getParamInit (i));
        float step   = static_cast<float> (apiUI.getParamStep (i));

        if (step <= 0.0f) step = 0.001f;

        // Use Faust label as APVTS parameter ID (matches MapUI key for parameterChanged)
        // This also becomes the WebSliderRelay name that JS references
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { label, 1 },
            label,
            juce::NormalisableRange<float> (minVal, maxVal, step),
            defVal
        ));

        paramLabels.add (label);
        paramAddresses.add (address);
    }

    return layout;
}

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // Create the real Faust DSP instance
    fDSP = std::make_unique<FaustDSP>();
    fDSP->init (44100);

    // Build MapUI for runtime param control
    fMapUI = std::make_unique<MapUI>();
    fDSP->buildUserInterface (fMapUI.get());

    // Listen to all parameter changes so we can forward to Faust
    for (const auto& label : paramLabels)
        apvts.addParameterListener (label, this);
}

PluginProcessor::~PluginProcessor()
{
    for (const auto& label : paramLabels)
        apvts.removeParameterListener (label, this);
}

void PluginProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    fDSP->init (static_cast<int> (sampleRate));

    // Rebuild MapUI after init (resets internal state)
    fMapUI = std::make_unique<MapUI>();
    fDSP->buildUserInterface (fMapUI.get());

    // Push current parameter values to Faust (restores state after sample rate change)
    for (int i = 0; i < paramLabels.size(); i++)
    {
        auto* rawParam = apvts.getRawParameterValue (paramLabels[i]);
        if (rawParam != nullptr)
            fMapUI->setParamValue (paramLabels[i].toRawUTF8(), rawParam->load());
    }
}

void PluginProcessor::releaseResources() {}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    int numSamples = buffer.getNumSamples();
    int faustOutputs = fDSP->getNumOutputs();

    if (faustOutputs >= 2)
    {
        // Stereo: process in-place (compiled with --in-place flag)
        float* channels[2] = {
            buffer.getWritePointer (0),
            buffer.getWritePointer (1)
        };
        fDSP->compute (numSamples, channels, channels);
    }
    else if (faustOutputs == 1)
    {
        // Mono: process channel 0, duplicate to channel 1
        float* mono[1] = { buffer.getWritePointer (0) };
        fDSP->compute (numSamples, mono, mono);
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    }
}

// Forward JUCE parameter changes to Faust DSP via MapUI
void PluginProcessor::parameterChanged (const juce::String& parameterId, float newValue)
{
    fMapUI->setParamValue (parameterId.toRawUTF8(), newValue);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}

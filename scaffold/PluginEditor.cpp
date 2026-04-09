#include "PluginProcessor.h"
#include "PluginEditor.h"

// Static helper: enumerate Faust params via APIUI and build JUCE parameter layout
juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout (FaustDSP& dsp)
{
    APIUI apiUI;
    dsp.buildUserInterface (&apiUI);

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < apiUI.getParamsCount(); i++)
    {
        auto address = juce::String (apiUI.getParamAddress (i));
        auto label   = juce::String (apiUI.getParamLabel (i));
        float minVal = static_cast<float> (apiUI.getParamMin (i));
        float maxVal = static_cast<float> (apiUI.getParamMax (i));
        float defVal = static_cast<float> (apiUI.getParamInit (i));
        float step   = static_cast<float> (apiUI.getParamStep (i));

        // Use address as parameter ID (unique even for duplicate labels like "Diffusion 1")
        // Use label as display name (shown in DAW automation lanes)
        if (step <= 0.0f) step = 0.001f;

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { address, 1 },
            label,
            juce::NormalisableRange<float> (minVal, maxVal, step),
            defVal
        ));
    }

    return layout;
}

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      fDSP (std::make_unique<FaustDSP>()),
      apvts (*this, nullptr, "Parameters", createParameterLayout (*fDSP))
{
    // Init Faust DSP at default sample rate (re-inited in prepareToPlay with actual rate)
    fDSP->init (44100);

    // Build MapUI for runtime param control in processBlock
    fMapUI = std::make_unique<MapUI>();
    fDSP->buildUserInterface (fMapUI.get());

    // Build APIUI to read param addresses for mapping
    fParamUI = std::make_unique<APIUI>();
    fDSP->buildUserInterface (fParamUI.get());

    // Cache param address → JUCE atomic pointer mappings for fast processBlock sync
    for (int i = 0; i < fParamUI->getParamsCount(); i++)
    {
        auto address = juce::String (fParamUI->getParamAddress (i));
        auto* rawParam = apvts.getRawParameterValue (address);
        if (rawParam != nullptr)
            paramMappings.push_back ({ address, rawParam });
    }
}

PluginProcessor::~PluginProcessor() {}

void PluginProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    // Re-init Faust DSP at the host's actual sample rate
    fDSP->init (static_cast<int> (sampleRate));

    // Rebuild MapUI (init resets internal state)
    fMapUI = std::make_unique<MapUI>();
    fDSP->buildUserInterface (fMapUI.get());
}

void PluginProcessor::releaseResources() {}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Sync JUCE parameter values → Faust
    for (auto& mapping : paramMappings)
        fMapUI->setParamValue (mapping.faustAddress.toRawUTF8(), mapping.juceParam->load());

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    int numSamples = buffer.getNumSamples();
    int faustInputs = fDSP->getNumInputs();
    int faustOutputs = fDSP->getNumOutputs();

    if (faustOutputs >= 2)
    {
        // Stereo Faust DSP — process in-place (--in-place flag allows shared buffers)
        float* channels[2] = {
            buffer.getWritePointer (0),
            buffer.getWritePointer (1)
        };

        fDSP->compute (numSamples, channels, channels);
    }
    else if (faustOutputs == 1)
    {
        // Mono Faust DSP — process channel 0, copy to channel 1
        float* monoOut[1] = { buffer.getWritePointer (0) };
        float* monoIn[1]  = { buffer.getWritePointer (0) };

        fDSP->compute (numSamples, monoIn, monoOut);

        // Duplicate mono to stereo
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    }
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

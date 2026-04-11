#include "PluginProcessor.h"
#include "PluginEditor.h"

// Sanitize a string to be a valid juce::Identifier (alphanumeric + underscore only).
// Must match the sanitization in PluginEditor.cpp and the JS shim exactly.
static juce::String sanitizeId (const juce::String& name)
{
    juce::String result;
    for (int i = 0; i < name.length(); i++)
    {
        auto c = name[i];
        if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '_')
            result += c;
        else if (c == ' ' || c == '/' || c == '-' || c == '(' || c == ')')
            result += '_';
    }
    return result;
}

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

        // Sanitize label for use as APVTS parameter ID.
        // juce::Identifier only allows alphanumeric + underscore — raw Faust labels
        // like "Decay Rate" or "Dry/Wet Mix" have spaces/slashes that silently break
        // APVTS registration, preventing WebSliderParameterAttachment from working.
        juce::String paramId = sanitizeId (label);

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { paramId, 1 },
            label,  // display name keeps the original label
            juce::NormalisableRange<float> (minVal, maxVal, step),
            defVal
        ));

        paramLabels.add (label);
        paramAddresses.add (address);
        paramIds.add (paramId);
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
    for (const auto& id : paramIds)
        apvts.addParameterListener (id, this);
}

PluginProcessor::~PluginProcessor()
{
    for (const auto& id : paramIds)
        apvts.removeParameterListener (id, this);
}

void PluginProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    fDSP->init (static_cast<int> (sampleRate));

    // Rebuild MapUI after init (resets internal state)
    fMapUI = std::make_unique<MapUI>();
    fDSP->buildUserInterface (fMapUI.get());

    // Push current parameter values to Faust (restores state after sample rate change)
    for (int i = 0; i < paramIds.size(); i++)
    {
        auto* rawParam = apvts.getRawParameterValue (paramIds[i]);
        if (rawParam != nullptr && i < paramAddresses.size())
            fMapUI->setParamValue (paramAddresses[i].toRawUTF8(), rawParam->load());
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

// Forward JUCE parameter changes to Faust DSP via MapUI.
// parameterId is the sanitized ID (e.g., "Decay_Rate"). We look up the
// corresponding Faust address (e.g., "/Dattorro/Decay_Rate") for MapUI.
void PluginProcessor::parameterChanged (const juce::String& parameterId, float newValue)
{
    // Find the Faust address for this parameter ID
    int idx = paramIds.indexOf (parameterId);
    if (idx >= 0)
        fMapUI->setParamValue (paramAddresses[idx].toRawUTF8(), newValue);
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

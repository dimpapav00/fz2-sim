#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout FZ2AudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto knob = [&p] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                                  juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), def));
    };
    auto db = [&p] (const char* id, const char* name, float lo, float hi, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                                  juce::NormalisableRange<float> (lo, hi, 0.1f), def));
    };
    db   ("input",  "Input",  -24.0f, 24.0f, 0.0f);
    knob ("gain",   "Gain",   0.60f);
    knob ("level",  "Level",  0.50f);
    knob ("bass",   "Bass",   0.50f);
    knob ("treble", "Treble", 0.50f);
    knob ("octave", "Octave", 0.40f);
    db   ("output", "Output", -24.0f, 12.0f, 0.0f);
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "on", 1 }, "Effect On", true));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Mode",
                                                               juce::StringArray { "Fuzz I", "Fuzz II", "Boost", "Fuzz I + II" }, 0));
    return { p.begin(), p.end() };
}

FZ2AudioProcessor::FZ2AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pIn = apvts.getRawParameterValue ("input");
    pGain = apvts.getRawParameterValue ("gain");
    pLevel = apvts.getRawParameterValue ("level");
    pBass = apvts.getRawParameterValue ("bass");
    pTreble = apvts.getRawParameterValue ("treble");
    pOct = apvts.getRawParameterValue ("octave");
    pOut = apvts.getRawParameterValue ("output");
    pOn = apvts.getRawParameterValue ("on");
    pMode = apvts.getRawParameterValue ("mode");
}

void FZ2AudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
}

bool FZ2AudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void FZ2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    fz2::Params p;
    p.inputDb  = pIn->load();
    p.gain     = pGain->load();
    p.level    = pLevel->load();
    p.bass     = pBass->load();
    p.treble   = pTreble->load();
    p.octave   = pOct->load();
    p.outputDb = pOut->load();
    p.on       = pOn->load() > 0.5f;
    p.mode     = std::clamp ((int) std::lround (pMode->load()), 0, 3);

    engine.process (buffer, p);
}

juce::AudioProcessorEditor* FZ2AudioProcessor::createEditor() { return new FZ2Editor (*this); }

void FZ2AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void FZ2AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FZ2AudioProcessor(); }

#include "PluginProcessor.h"
#include "PluginEditor.h"

TishinaProcessor::TishinaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TISHINA", params::createLayout())
{
    cache.init (apvts);
    setLatencySamples (kv::Suppressor::fftSizeFor (44100.0, cache.quality()));
}

TishinaProcessor::~TishinaProcessor() { cancelPendingUpdate(); }

bool TishinaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void TishinaProcessor::rebuild (double sampleRate, int quality)
{
    cache.fill (sp);
    eng.setParams (sp);
    eng.prepare (sampleRate, quality);
    preparedQuality.store (quality);
    setLatencySamples (eng.latencySamples());
    bypassDelay.setSize (2, std::max (1, eng.latencySamples()));
    bypassDelay.clear();
    bypassPos = 0;
}

void TishinaProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateUsed.store (sampleRate);
    rebuild (sampleRate, cache.quality());
    prepared = true;
}

void TishinaProcessor::applyQualityChange()
{
    const int q = cache.quality();
    if (! prepared || q == preparedQuality.load())
        return;
    // A new FFT size means new buffers and a new latency: stop the audio callback while swapping.
    suspendProcessing (true);
    rebuild (sampleRateUsed.load(), q);
    suspendProcessing (false);
}

void TishinaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
    if (buffer.getNumChannels() < 2 || n == 0 || ! prepared)
        return;

    if (cache.quality() != preparedQuality.load())
        triggerAsyncUpdate();

    cache.fill (sp);
    eng.setParams (sp);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    inPeak.update ((float) kv::gainToDb (std::max (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n)), -120.0));
    eng.process (L, R, n);
    outPeak.update ((float) kv::gainToDb (std::max (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n)), -120.0));
}

void TishinaProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Keep the timing identical when the host bypasses the plugin: delay by the reported latency.
    const int lat = bypassDelay.getNumSamples();
    const int n = buffer.getNumSamples();
    if (getLatencySamples() <= 0 || buffer.getNumChannels() < 2)
        return;
    for (int ch = 0; ch < 2; ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        float* line = bypassDelay.getWritePointer (ch);
        int p = bypassPos;
        for (int i = 0; i < n; ++i)
        {
            const float t = line[p];
            line[p] = d[i];
            d[i] = t;
            if (++p == lat)
                p = 0;
        }
    }
    bypassPos = (bypassPos + n) % lat;
}

//==============================================================================
const juce::String TishinaProcessor::getProgramName (int index)
{
    const auto& p = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void TishinaProcessor::loadPreset (int index)
{
    const auto& list = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return;
    currentProgram = index;
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (const auto& [id, value] : list[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

//==============================================================================
void TishinaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TishinaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (tree);
    // replaceState skips parameters whose snapped value already matches (e.g. a switch sitting at 0.86),
    // so push every stored value explicitly; the host then sees exactly what was saved.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (auto* raw = apvts.getRawParameterValue (rp->getParameterID()))
            {
                const float norm = rp->convertTo0to1 (raw->load());
                if (std::abs (rp->getValue() - norm) > 1.0e-6f)
                    rp->setValueNotifyingHost (norm);
            }
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));
}

juce::AudioProcessorEditor* TishinaProcessor::createEditor() { return new TishinaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TishinaProcessor(); }

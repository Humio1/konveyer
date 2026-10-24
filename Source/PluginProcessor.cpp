#include "PluginProcessor.h"
#include "PluginEditor.h"

KonveyerProcessor::KonveyerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KONVEYER", params::createLayout())
{
    cache.init (apvts);
    apvts.state.setProperty ("order", orderToString (kv::defaultOrder()), nullptr);
    // Estimate until prepareToPlay knows the sample rate (look-ahead is 5 ms plus the true-peak filter).
    setLatencySamples (kv::TruePeakDetector::kDelay + (int) std::ceil (0.005 * 44100.0) - 1);
}

bool KonveyerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void KonveyerProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateUsed.store (sampleRate);
    cache.fill (ep);
    ep.order = getOrder();
    eng.setParams (ep);
    eng.prepare (sampleRate);
    setLatencySamples (eng.latencySamples());
    bypassDelay.setSize (2, std::max (1, eng.latencySamples()));
    bypassDelay.clear();
    bypassPos = 0;
}

void KonveyerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    cache.fill (ep);
    ep.order = getOrder();
    eng.setParams (ep);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    for (int s = 0; s < n; s += kv::kMaxChunk)
        eng.process (L + s, R + s, std::min (kv::kMaxChunk, n - s));
}

void KonveyerProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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
void KonveyerProcessor::setOrder (const kv::Order& o)
{
    if (! kv::isValidOrder (o))
        return;
    orderPacked.store (kv::packOrder (o));
    apvts.state.setProperty ("order", orderToString (o), nullptr);
}

juce::String KonveyerProcessor::orderToString (const kv::Order& o)
{
    juce::StringArray s;
    for (int m : o)
        s.add (params::moduleShortIds()[m]);
    return s.joinIntoString (",");
}

kv::Order KonveyerProcessor::orderFromString (const juce::String& text)
{
    juce::StringArray s;
    s.addTokens (text, ",", {});
    kv::Order o = kv::defaultOrder();
    if (s.size() != kv::kNumModules)
        return o;
    for (int i = 0; i < kv::kNumModules; ++i)
        o[(size_t) i] = params::moduleShortIds().indexOf (s[i].trim());
    return kv::isValidOrder (o) ? o : kv::defaultOrder();
}

const juce::String KonveyerProcessor::getProgramName (int index)
{
    const auto& p = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void KonveyerProcessor::loadPreset (int index)
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
    setOrder (list[(size_t) index].order);
    apvts.state.setProperty ("program", index, nullptr);
}

//==============================================================================
void KonveyerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("order", orderToString (getOrder()), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void KonveyerProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    const auto o = orderFromString (tree.getProperty ("order").toString());
    orderPacked.store (kv::packOrder (o));
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));
}

juce::AudioProcessorEditor* KonveyerProcessor::createEditor() { return new KonveyerEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KonveyerProcessor(); }

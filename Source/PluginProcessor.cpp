#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Text.h"

// ---------------------------------------------------------------------------
//  Parámetros: acá se define cada control, su rango y cómo se muestra.
// ---------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout MeteringABProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto dbAttributes = AudioParameterFloatAttributes()
        .withLabel ("dB")
        .withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " dB"; })
        .withValueFromStringFunction ([] (const String& t) { return t.retainCharacters ("-+.,0123456789").replaceCharacter (',', '.').getFloatValue(); });

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Modo",
                                                        StringArray { "Send (antes de FX)"_u, "Receive (después de FX)"_u }, 0));

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::link, 1 }, "Link ID", 1, SharedBus::numLinks, 1));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::match, 1 }, "Matcheo",
                                                        StringArray { "Auto", "Manual", "Apagado" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::manual, 1 }, "Ganancia manual",
                                                       NormalisableRange<float> (minGainDb, maxGainDb, 0.01f), 0.0f, dbAttributes));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::listen, 1 }, "Escuchar",
                                                        StringArray { "Procesado", "Original", "Delta" }, 0));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::measure, 1 }, "Medición"_u,
                                                        StringArray { "RMS", "LUFS (filtro K)" }, 1));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::window, 1 }, "Ventana",
                                                       NormalisableRange<float> (0.2f, 10.0f, 0.01f, 0.5f), 3.0f,
                                                       AudioParameterFloatAttributes()
                                                           .withLabel ("s")
                                                           .withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " s"; })
                                                           .withValueFromStringFunction ([] (const String& t) { return t.retainCharacters (".,0123456789").replaceCharacter (',', '.').getFloatValue(); })));

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::latency, 1 }, "Latencia", 0, maxLatencySamples, 0,
                                                     AudioParameterIntAttributes().withLabel ("muestras")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::limit, 1 }, "Límite auto"_u,
                                                       NormalisableRange<float> (0.0f, maxGainDb, 0.1f), 18.0f, dbAttributes));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::hold, 1 }, "Congelar", false));

    return layout;
}

MeteringABProcessor::MeteringABProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pMode    = apvts.getRawParameterValue (ParamIDs::mode);
    pLink    = apvts.getRawParameterValue (ParamIDs::link);
    pMatch   = apvts.getRawParameterValue (ParamIDs::match);
    pManual  = apvts.getRawParameterValue (ParamIDs::manual);
    pListen  = apvts.getRawParameterValue (ParamIDs::listen);
    pMeasure = apvts.getRawParameterValue (ParamIDs::measure);
    pWindow  = apvts.getRawParameterValue (ParamIDs::window);
    pLatency = apvts.getRawParameterValue (ParamIDs::latency);
    pLimit   = apvts.getRawParameterValue (ParamIDs::limit);
    pHold    = apvts.getRawParameterValue (ParamIDs::hold);
}

MeteringABProcessor::~MeteringABProcessor()
{
    updateSenderRegistration (false, -1);
}

void MeteringABProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    preMeter.prepare (sampleRate);
    postMeter.prepare (sampleRate);
    dryBuffer.setSize (2, juce::jmax (samplesPerBlock, 4096));
    gainSmoother.reset (sampleRate, 0.05);           // 50 ms de suavizado
    gainSmoother.setCurrentAndTargetValue (1.0f);
    blocksWithoutHeartbeat = 0;
}

bool MeteringABProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

// Lleva la cuenta de cuántos Send hay en cada Link ID (para avisar si hay dos).
void MeteringABProcessor::updateSenderRegistration (bool isSend, int linkIndex)
{
    const int wanted = isSend ? linkIndex : -1;

    if (wanted == registeredSendLink)
        return;

    if (registeredSendLink >= 0)
        SharedBus::get().slot (registeredSendLink).senderCount.fetch_sub (1);

    if (wanted >= 0)
        SharedBus::get().slot (wanted).senderCount.fetch_add (1);

    registeredSendLink = wanted;
}

void MeteringABProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const bool isSend = (int) pMode->load() == (int) Mode::send;
    const int link = (int) pLink->load() - 1;

    updateSenderRegistration (isSend, link);
    auto& slot = SharedBus::get().slot (link);

    if (link != lastLink)
    {
        lastLink = link;
        lastHeartbeat = slot.heartbeat.load();
        blocksWithoutHeartbeat = 0;
        preMeter.reset();
        postMeter.reset();
    }

    duplicateSend = slot.senderCount.load() > 1;

    if (isSend)
        processSend (buffer, slot);
    else
        processReceive (buffer, slot);
}

// ---------------------------------------------------------------------------
//  SEND: no toca el audio. Solo copia el original al bus compartido.
// ---------------------------------------------------------------------------
void MeteringABProcessor::processSend (juce::AudioBuffer<float>& buffer, LinkSlot& slot)
{
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);

    if (numCh == 0 || n == 0)
        return;

    const auto start = slot.writePos.load (std::memory_order_relaxed);

    for (int ch = 0; ch < 2; ++ch)
    {
        // Si la pista es mono, se copia el mismo canal en ambos lados.
        const float* src = buffer.getReadPointer (juce::jmin (ch, numCh - 1));
        auto& ring = slot.ring[(size_t) ch];

        for (int i = 0; i < n; ++i)
            ring[(size_t) ((start + i) & LinkSlot::ringMask)] = src[i];
    }

    slot.numChannels.store (numCh);
    slot.writePos.store (start + n, std::memory_order_release);
    slot.heartbeat.fetch_add (1, std::memory_order_release);

    preMeter.setMode ((LoudnessMeter::Mode) (int) pMeasure->load());
    preMeter.setWindowSeconds (pWindow->load());
    preMeter.process (buffer.getArrayOfReadPointers(), numCh, n);

    displayPre    = (float) preMeter.getDecibels();
    displayPost   = displayPre.load();
    displayGain   = 0.0f;
    sendConnected = true;
}

// ---------------------------------------------------------------------------
//  RECEIVE: compara el original (del Send) con lo procesado y empareja.
// ---------------------------------------------------------------------------
void MeteringABProcessor::processReceive (juce::AudioBuffer<float>& buffer, LinkSlot& slot)
{
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);

    if (numCh == 0 || n == 0)
        return;

    // ¿Está vivo el Send? Si su contador no se mueve, no hay Send en este ID.
    const auto hb = slot.heartbeat.load (std::memory_order_acquire);

    if (hb != lastHeartbeat)
    {
        lastHeartbeat = hb;
        blocksWithoutHeartbeat = 0;
    }
    else
    {
        ++blocksWithoutHeartbeat;
    }

    const int timeoutBlocks = juce::jmax (4, (int) (0.3 * currentSampleRate / n));
    const bool connected = blocksWithoutHeartbeat <= timeoutBlocks && n < LinkSlot::ringSize / 2;
    sendConnected = connected;

    const auto measureMode = (LoudnessMeter::Mode) (int) pMeasure->load();
    const auto windowSeconds = (double) pWindow->load();
    preMeter.setMode (measureMode);
    postMeter.setMode (measureMode);
    preMeter.setWindowSeconds (windowSeconds);
    postMeter.setWindowSeconds (windowSeconds);

    postMeter.process (buffer.getArrayOfReadPointers(), numCh, n);

    if (! connected)
    {
        // Sin Send: el audio pasa sin cambios.
        gainSmoother.setCurrentAndTargetValue (1.0f);
        displayPre  = -120.0f;
        displayPost = (float) postMeter.getDecibels();
        displayGain = 0.0f;
        return;
    }

    // 1) Traer el audio original, retrasado según la latencia de tus FX.
    if (dryBuffer.getNumSamples() < n)
        dryBuffer.setSize (2, n, false, false, true);  // caso raro: bloque más grande de lo esperado

    const int latency = juce::jlimit (0, LinkSlot::ringSize - n - 1, (int) pLatency->load());
    const auto end = slot.writePos.load (std::memory_order_acquire);
    const auto start = end - n - latency;

    for (int ch = 0; ch < numCh; ++ch)
    {
        const auto& ring = slot.ring[(size_t) ch];
        float* dst = dryBuffer.getWritePointer (ch);

        for (int i = 0; i < n; ++i)
        {
            const auto pos = start + i;
            dst[i] = pos >= 0 ? ring[(size_t) (pos & LinkSlot::ringMask)] : 0.0f;
        }
    }

    preMeter.process (dryBuffer.getArrayOfReadPointers(), numCh, n);

    // 2) Calcular cuánta ganancia hace falta.
    float targetDb = 0.0f;

    switch ((Match) (int) pMatch->load())
    {
        case Match::automatic:
        {
            const double preMs = preMeter.getMeanSquare();
            const double postMs = postMeter.getMeanSquare();
            constexpr double gate = 1.0e-6;              // -60 dB: no matchear silencio

            if (pHold->load() < 0.5f && preMs > gate && postMs > 1.0e-12)
            {
                const auto limit = pLimit->load();
                autoGainDb = juce::jlimit (-limit, limit, (float) (10.0 * std::log10 (preMs / postMs)));
            }

            targetDb = autoGainDb;
            break;
        }

        case Match::manual: targetDb = pManual->load(); break;
        case Match::off:    targetDb = 0.0f;            break;
    }

    gainSmoother.setTargetValue (juce::Decibels::decibelsToGain (targetDb));

    // 3) Aplicar según lo que quieras escuchar (A/B).
    const auto listen = (Listen) (int) pListen->load();

    for (int i = 0; i < n; ++i)
    {
        const float g = gainSmoother.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* out = buffer.getWritePointer (ch);
            const float dry = dryBuffer.getSample (ch, i);

            switch (listen)
            {
                case Listen::processed: out[i] = out[i] * g;       break;
                case Listen::original:  out[i] = dry;              break;
                case Listen::delta:     out[i] = out[i] * g - dry; break;
            }
        }
    }

    displayPre  = (float) preMeter.getDecibels();
    displayPost = (float) postMeter.getDecibels();
    displayGain = targetDb;
}

void MeteringABProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MeteringABProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MeteringABProcessor::createEditor()
{
    return new MeteringABEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MeteringABProcessor();
}

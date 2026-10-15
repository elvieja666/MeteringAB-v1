#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LoudnessMeter.h"
#include "SharedBus.h"

// IDs de los parámetros (así los encuentra Logic para automatizar y guardar).
namespace ParamIDs
{
    inline constexpr auto mode     = "mode";      // Send / Receive
    inline constexpr auto link     = "link";      // Link ID 1..16
    inline constexpr auto match    = "match";     // Auto / Manual / Off
    inline constexpr auto manual   = "manual";    // ganancia manual en dB
    inline constexpr auto listen   = "listen";    // Procesado / Original / Delta
    inline constexpr auto measure  = "measure";   // RMS / LUFS
    inline constexpr auto window   = "window";    // ventana de medición (s)
    inline constexpr auto latency  = "latency";   // compensación (muestras)
    inline constexpr auto limit    = "limit";     // límite de auto gain (dB)
    inline constexpr auto hold     = "hold";      // congelar el auto gain
}

class MeteringABProcessor : public juce::AudioProcessor
{
public:
    enum class Mode   { send = 0, receive = 1 };
    enum class Match  { automatic = 0, manual = 1, off = 2 };
    enum class Listen { processed = 0, original = 1, delta = 2 };

    MeteringABProcessor();
    ~MeteringABProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override                             { return 1; }
    int getCurrentProgram() override                          { return 0; }
    void setCurrentProgram (int) override                     {}
    const juce::String getProgramName (int) override          { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Valores para la pantalla (los escribe el audio, los lee la interfaz).
    std::atomic<float> displayPre    { -120.0f };
    std::atomic<float> displayPost   { -120.0f };
    std::atomic<float> displayGain   { 0.0f };
    std::atomic<bool>  sendConnected { false };
    std::atomic<bool>  duplicateSend { false };

    static constexpr float minGainDb = -24.0f, maxGainDb = 24.0f;
    static constexpr int   maxLatencySamples = 32000;

private:
    void updateSenderRegistration (bool isSend, int linkIndex);
    void processSend (juce::AudioBuffer<float>& buffer, LinkSlot& slot);
    void processReceive (juce::AudioBuffer<float>& buffer, LinkSlot& slot);

    // Punteros rápidos a los parámetros (leer esto en el audio es seguro).
    std::atomic<float>* pMode    = nullptr;
    std::atomic<float>* pLink    = nullptr;
    std::atomic<float>* pMatch   = nullptr;
    std::atomic<float>* pManual  = nullptr;
    std::atomic<float>* pListen  = nullptr;
    std::atomic<float>* pMeasure = nullptr;
    std::atomic<float>* pWindow  = nullptr;
    std::atomic<float>* pLatency = nullptr;
    std::atomic<float>* pLimit   = nullptr;
    std::atomic<float>* pHold    = nullptr;

    LoudnessMeter preMeter, postMeter;
    juce::AudioBuffer<float> dryBuffer;               // audio original alineado
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gainSmoother;

    float autoGainDb = 0.0f;
    int   registeredSendLink = -1;                    // -1 = no registrado
    juce::uint32 lastHeartbeat = 0;
    int   blocksWithoutHeartbeat = 0;
    int   lastLink = -1;
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeteringABProcessor)
};

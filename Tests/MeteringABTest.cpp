#include "PluginProcessor.h"
#include <cstdio>

static void setParam (MeteringABProcessor& p, const char* id, float plainValue)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
}

static double rmsDb (const juce::AudioBuffer<float>& b)
{
    double s = 0; int n = 0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i) { s += b.getSample (ch, i) * b.getSample (ch, i); ++n; }
    return 10.0 * std::log10 (s / n + 1e-30);
}

int failures = 0;
static void check (bool ok, const char* what, double value)
{
    std::printf ("%s  %-55s %.3f\n", ok ? "OK  " : "FAIL", what, value);
    if (! ok) ++failures;
}

struct Chain
{
    MeteringABProcessor send, recv;
    juce::MidiBuffer midi;
    std::vector<float> fxDelayL, fxDelayR;
    int fxLatency;
    float fxGain;
    double phase = 0;
    juce::Random rng { 42 };

    Chain (int link, float gainDb, int latency) : fxLatency (latency), fxGain (juce::Decibels::decibelsToGain (gainDb))
    {
        setParam (send, ParamIDs::mode, 0); setParam (send, ParamIDs::link, (float) link);
        setParam (recv, ParamIDs::mode, 1); setParam (recv, ParamIDs::link, (float) link);
        send.prepareToPlay (48000, 512); recv.prepareToPlay (48000, 512);
        fxDelayL.assign ((size_t) juce::jmax (1, latency), 0); fxDelayR.assign ((size_t) juce::jmax (1, latency), 0);
    }

    // Procesa "seconds" de ruido rosado-ish/sinusoide y devuelve el último bloque.
    juce::AudioBuffer<float> run (double seconds, juce::AudioBuffer<float>* lastDry = nullptr)
    {
        juce::AudioBuffer<float> buf (2, 512), dry (2, 512);
        const int blocks = (int) (seconds * 48000 / 512);
        size_t w = 0;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                float x = 0.25f * (float) std::sin (phase) + 0.05f * (rng.nextFloat() * 2 - 1);
                phase += 2 * juce::MathConstants<double>::pi * 220.0 / 48000.0;
                buf.setSample (0, i, x); buf.setSample (1, i, x * 0.8f);
            }
            dry.makeCopyOf (buf);
            send.processBlock (buf, midi);
            // "Efecto": ganancia + latencia
            for (int i = 0; i < 512; ++i)
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto& d = ch == 0 ? fxDelayL : fxDelayR;
                    size_t idx = (fxWrite + (size_t) i) % d.size();
                    float in = buf.getSample (ch, i);
                    float out = d[idx];
                    d[idx] = in;
                    buf.setSample (ch, i, fxLatency == 0 ? in * fxGain : out * fxGain);
                }
            fxWrite = (fxWrite + 512) % fxDelayL.size();
            recv.processBlock (buf, midi);
            if (lastDry) lastDry->makeCopyOf (dry);
        }
        (void) w;
        return buf;
    }
    size_t fxWrite = 0;
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc > 1) // Metering A/BTests <carpeta>: guarda capturas de la interfaz
    {
        for (int mode = 0; mode < 2; ++mode)
        {
            MeteringABProcessor a;
            setParam (a, ParamIDs::mode, (float) mode);
            a.displayPre = -14.2f; a.displayPost = -9.8f; a.displayGain = -4.4f; a.sendConnected = true;
            std::unique_ptr<juce::AudioProcessorEditor> ed (a.createEditor());
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            juce::File f (juce::String (argv[1]) + (mode == 0 ? "/send.png" : "/receive.png"));
            f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        }
        return 0;
    }

    {
        Chain c (1, -6.0f, 0);
        auto out = c.run (12.0);
        check (std::abs (c.recv.displayGain.load() - 6.0f) < 0.1f, "Auto (LUFS): FX -6 dB -> ganancia ~ +6 dB", c.recv.displayGain.load());
        check (c.recv.sendConnected.load(), "Receive detecta el Send", 1);
        juce::AudioBuffer<float> dry; c.run (0.5, &dry); out = c.run (0.02, &dry);
        check (std::abs (rmsDb (out) - rmsDb (dry)) < 0.2, "Salida matcheada = nivel original (dB de diferencia)", rmsDb (out) - rmsDb (dry));

        setParam (c.recv, ParamIDs::listen, 2); // delta
        out = c.run (1.0);
        check (rmsDb (out) < -60, "Delta con FX de pura ganancia ~ silencio (dB)", rmsDb (out));

        setParam (c.recv, ParamIDs::listen, 1); // original
        out = c.run (0.5, &dry);
        check (std::abs (rmsDb (out) - rmsDb (dry)) < 0.01, "Escuchar Original = audio del Send", rmsDb (out) - rmsDb (dry));
    }
    {
        Chain c (2, 4.5f, 0);
        setParam (c.recv, ParamIDs::measure, 0);
        c.run (12.0);
        check (std::abs (c.recv.displayGain.load() + 4.5f) < 0.1f, "Auto (RMS): FX +4.5 dB -> ganancia ~ -4.5 dB", c.recv.displayGain.load());
    }
    {
        Chain c (3, -3.0f, 300);
        setParam (c.recv, ParamIDs::latency, 300);
        setParam (c.recv, ParamIDs::listen, 2);
        c.run (12.0);
        auto out = c.run (1.0);
        check (rmsDb (out) < -60, "Latencia 300 compensada: Delta ~ silencio (dB)", rmsDb (out));
        setParam (c.recv, ParamIDs::latency, 0);
        out = c.run (1.0);
        check (rmsDb (out) > -40, "Latencia sin compensar: Delta NO es silencio (dB)", rmsDb (out));
    }
    {
        Chain c (4, -6.0f, 0);
        setParam (c.recv, ParamIDs::match, 1);
        setParam (c.recv, ParamIDs::manual, 2.37f);
        c.run (3.0);
        check (std::abs (c.recv.displayGain.load() - 2.37f) < 0.001f, "Manual: valor exacto 2.37 dB", c.recv.displayGain.load());
        auto* p = c.recv.apvts.getParameter (ParamIDs::manual);
        check (std::abs (p->getValueForText ("-3,25 dB") * 48 - 24 + 3.25f) < 0.01f, "Escribir '-3,25 dB' se interpreta como -3.25", p->getValueForText ("-3,25 dB") * 48 - 24);
    }
    {
        Chain c (5, -20.0f, 0);
        setParam (c.recv, ParamIDs::limit, 12.0f);
        c.run (12.0);
        check (std::abs (c.recv.displayGain.load() - 12.0f) < 0.01f, "Límite auto 12 dB respeta el tope", c.recv.displayGain.load());
    }
    {
        MeteringABProcessor lonely; juce::MidiBuffer m;
        setParam (lonely, ParamIDs::mode, 1); setParam (lonely, ParamIDs::link, 9);
        lonely.prepareToPlay (48000, 512);
        juce::AudioBuffer<float> b (2, 512);
        for (int k = 0; k < 100; ++k) { for (int i = 0; i < 512; ++i) { b.setSample (0, i, 0.3f); b.setSample (1, i, 0.3f); } lonely.processBlock (b, m); }
        check (! lonely.sendConnected.load() && std::abs (b.getSample (0, 100) - 0.3f) < 1e-6f, "Sin Send: avisa y deja pasar el audio igual", b.getSample (0, 100));
    }
    {
        MeteringABProcessor a, b2; juce::MidiBuffer m;
        setParam (a, ParamIDs::link, 10); setParam (b2, ParamIDs::link, 10);
        a.prepareToPlay (48000, 512); b2.prepareToPlay (48000, 512);
        juce::AudioBuffer<float> buf (2, 512);
        a.processBlock (buf, m); b2.processBlock (buf, m); a.processBlock (buf, m);
        check (a.duplicateSend.load() && b2.duplicateSend.load(), "Dos Send en el mismo ID: avisan los dos", 1);
    }
    {
        // Guardar/recuperar estado (como cuando abrís el proyecto de Logic).
        MeteringABProcessor a, b2;
        setParam (a, ParamIDs::manual, -7.13f); setParam (a, ParamIDs::latency, 128);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        b2.setStateInformation (mb.getData(), (int) mb.getSize());
        check (std::abs (b2.apvts.getRawParameterValue (ParamIDs::manual)->load() + 7.13f) < 0.005f, "Estado guardado y recuperado (manual)", b2.apvts.getRawParameterValue (ParamIDs::manual)->load());
    }
    {
        // El editor se puede crear y destruir sin problemas.
        MeteringABProcessor a;
        std::unique_ptr<juce::AudioProcessorEditor> ed (a.createEditor());
        check (ed != nullptr && ed->getWidth() == 640 && ed->getHeight() == 450, "Editor se crea (ancho px)", ed->getWidth());
    }

    std::printf ("\n%s (%d fallas)\n", failures == 0 ? "TODO OK" : "HAY FALLAS", failures);
    return failures;
}

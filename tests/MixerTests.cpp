// Pruebas de la Fase 2: EQ isolator, filtro, trim, limitador, CUE de
// auriculares, CUE MIX, ruteo de 4 salidas, SPLIT, curvas del crossfader y
// medición (vúmetros + balística).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/AudioEngine.h"
#include "engine/ChannelStrip.h"
#include "engine/MeterBallistics.h"
#include "engine/dsp/DjFilter.h"
#include "engine/dsp/Limiter.h"
#include "engine/dsp/ThreeBandIsolator.h"

#include <cmath>
#include <random>
#include <vector>

using namespace kb;
using namespace kb::engine;
using Catch::Approx;

namespace
{
    constexpr double kSr = 48000.0;

    std::vector<float> sine (double freq, int n, float amp = 0.5f, double sr = kSr)
    {
        std::vector<float> v ((size_t) n);
        for (int i = 0; i < n; ++i)
            v[(size_t) i] = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr);
        return v;
    }

    float db (float linear) { return juce::Decibels::gainToDecibels (linear, -200.0f); }

    /** Ganancia (dB) de una tira de canal para un seno, medida en régimen estable. */
    float stripGainDb (const ChannelStrip::Params& p, double freq)
    {
        ChannelStrip strip;
        strip.prepare (kSr);
        constexpr int n = 48000;
        auto l = sine (freq, n), r = l;
        // En bloques, como en el audio real.
        for (int pos = 0; pos < n; pos += 256)
            strip.process (l.data() + pos, r.data() + pos, std::min (256, n - pos), p);
        const auto in  = test::rms (sine (freq, n).data() + n / 2, (size_t) n / 2);
        const auto out = test::rms (l.data() + n / 2, (size_t) n / 2);
        return db (out / in);
    }

    /** Motor con el deck 1 sonando un seno, listo para medir 4 salidas. */
    struct FourOutputRig
    {
        FourOutputRig()
        {
            engine.prepare ({ kSr, 256, 4 });
            engine.setMasterVolume (1.0f);
            engine.setHeadphoneVolume (1.0f);
            engine.setChannelVolume (0, 1.0f);
            engine.setCrossfader (-1.0f);
            engine.loadTrack (0, test::makeSineTrack (5.0, 1000.0, kSr, 0.5f));
            engine.play (0);
        }

        /** Renderiza y devuelve el RMS (dB) de cada una de las 4 salidas. */
        std::array<float, 4> measure (int frames = 24000)
        {
            std::array<std::vector<float>, 4> o;
            for (auto& c : o) c.assign ((size_t) frames, 0.0f);
            for (int pos = 0; pos < frames; pos += 256)
            {
                float* chans[] = { o[0].data() + pos, o[1].data() + pos, o[2].data() + pos, o[3].data() + pos };
                engine.process (chans, 4, std::min (256, frames - pos));
            }
            std::array<float, 4> r {};
            for (int c = 0; c < 4; ++c)
                r[(size_t) c] = db (test::rms (o[(size_t) c].data() + frames / 2, (size_t) frames / 2));
            return r;
        }

        AudioEngine engine;
    };

    constexpr float kSilent = -90.0f;
    const float kSineRmsDb = db (0.5f / std::sqrt (2.0f));   // -9.03 dB
}

// =============================================================================
//  EQ isolator
// =============================================================================
TEST_CASE ("EQ: perilla -> ganancia", "[eq]")
{
    CHECK (juce::exactlyEqual (dsp::eqKnobToGain (0.0f), 0.0f));                 // KILL
    CHECK (dsp::eqKnobToGain (0.5f) == Approx (1.0f));        // 0 dB
    CHECK (db (dsp::eqKnobToGain (1.0f)) == Approx (6.0f).margin (0.01f));
    CHECK (db (dsp::eqKnobToGain (0.25f)) == Approx (-12.04f).margin (0.1f));
}

TEST_CASE ("EQ: plano al centro y kill de cada banda", "[eq]")
{
    ChannelStrip::Params flat;
    for (double f : { 40.0, 120.0, 500.0, 1000.0, 2000.0, 6000.0, 15000.0 })
        CHECK (stripGainDb (flat, f) == Approx (0.0f).margin (0.05f));

    ChannelStrip::Params killLow;  killLow.low = 0.0f;
    ChannelStrip::Params killMid;  killMid.mid = 0.0f;
    ChannelStrip::Params killHigh; killHigh.high = 0.0f;

    CHECK (stripGainDb (killLow, 50.0) < -40.0f);       // graves eliminados
    CHECK (stripGainDb (killLow, 5000.0) == Approx (0.0f).margin (0.5f));   // agudos intactos
    CHECK (stripGainDb (killMid, 900.0) < -30.0f);      // medios eliminados
    CHECK (stripGainDb (killHigh, 10000.0) < -40.0f);   // agudos eliminados
    CHECK (stripGainDb (killHigh, 60.0) == Approx (0.0f).margin (0.5f));    // graves intactos
}

TEST_CASE ("EQ: +6 dB al maximo en el centro de cada banda", "[eq]")
{
    ChannelStrip::Params boostLow;  boostLow.low = 1.0f;
    ChannelStrip::Params boostMid;  boostMid.mid = 1.0f;
    ChannelStrip::Params boostHigh; boostHigh.high = 1.0f;
    CHECK (stripGainDb (boostLow, 50.0) == Approx (6.0f).margin (0.3f));
    CHECK (stripGainDb (boostMid, 900.0) == Approx (6.0f).margin (0.6f));
    CHECK (stripGainDb (boostHigh, 12000.0) == Approx (6.0f).margin (0.3f));
}

TEST_CASE ("Trim: +-12 dB", "[eq]")
{
    CHECK (db (ChannelStrip::trimKnobToGain (0.5f)) == Approx (0.0f).margin (1e-4));
    CHECK (db (ChannelStrip::trimKnobToGain (1.0f)) == Approx (12.0f).margin (1e-3));
    CHECK (db (ChannelStrip::trimKnobToGain (0.0f)) == Approx (-12.0f).margin (1e-3));

    ChannelStrip::Params p; p.trim = 0.25f;   // -6 dB
    CHECK (stripGainDb (p, 1000.0) == Approx (-6.0f).margin (0.05f));
}

// =============================================================================
//  Filtro
// =============================================================================
TEST_CASE ("Filtro: centro transparente, LPF a la izquierda, HPF a la derecha", "[filter]")
{
    ChannelStrip::Params centre;
    CHECK (stripGainDb (centre, 5000.0) == Approx (0.0f).margin (0.05f));

    ChannelStrip::Params lpf; lpf.filter = 0.0f;   // LPF a ~60 Hz
    CHECK (stripGainDb (lpf, 5000.0) < -40.0f);
    CHECK (stripGainDb (lpf, 30.0) > -3.0f);

    ChannelStrip::Params hpf; hpf.filter = 1.0f;   // HPF a ~8 kHz
    CHECK (stripGainDb (hpf, 100.0) < -40.0f);
    CHECK (stripGainDb (hpf, 16000.0) > -3.0f);

    CHECK (dsp::DjFilter::cutoffFor (0.0f) == Approx (60.0));
    CHECK (dsp::DjFilter::cutoffFor (1.0f) == Approx (8000.0));
}

TEST_CASE ("Filtro: barrido rapido sin explosiones ni clicks", "[filter]")
{
    dsp::DjFilter f;
    f.prepare (kSr);
    auto l = sine (440.0, 96000, 0.5f), r = l;
    float maxAbs = 0.0f;
    for (int pos = 0; pos < 96000; pos += 128)
    {
        // Barre de punta a punta varias veces por segundo.
        f.setPosition (0.5f + 0.5f * (float) std::sin (pos / 3000.0));
        f.process (l.data() + pos, r.data() + pos, 128);
    }
    for (auto x : l)
    {
        REQUIRE (std::isfinite (x));
        maxAbs = std::max (maxAbs, std::abs (x));
    }
    CHECK (maxAbs < 1.5f);   // la resonancia moderada no se dispara
}

// =============================================================================
//  Limitador
// =============================================================================
TEST_CASE ("Limitador: nunca supera el techo", "[limiter]")
{
    dsp::Limiter lim;
    lim.prepare (kSr);

    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> dist (-4.0f, 4.0f);   // hasta +12 dB y transitorios
    std::vector<float> l (96000), r (96000);
    for (size_t i = 0; i < l.size(); ++i)
    {
        l[i] = dist (rng) * (i % 4800 < 100 ? 1.0f : 0.3f);
        r[i] = -l[i] * 0.7f;
    }

    for (int pos = 0; pos < 96000; pos += 256)
        lim.process (l.data() + pos, r.data() + pos, 256);

    float maxAbs = 0.0f;
    for (size_t i = 0; i < l.size(); ++i)
        maxAbs = std::max ({ maxAbs, std::abs (l[i]), std::abs (r[i]) });
    CHECK (maxAbs <= lim.getCeiling() + 1.0e-6f);
}

TEST_CASE ("Limitador: por debajo del techo la senal pasa intacta (solo retrasada)", "[limiter]")
{
    dsp::Limiter lim;
    lim.prepare (kSr);
    const auto in = sine (1000.0, 9600, 0.5f);
    auto l = in, r = in;
    lim.process (l.data(), r.data(), 9600);

    const auto d = lim.getLatencySamples();
    CHECK (d == 95);   // 2 ms a 48 kHz
    for (int i = d; i < 9600; i += 37)
        CHECK (l[(size_t) i] == Approx (in[(size_t) (i - d)]).margin (1e-7));
}

TEST_CASE ("Limitador: el master con dos decks fuertes no satura", "[limiter][mixer]")
{
    AudioEngine engine;
    test::prepareUnity (engine);
    engine.setCrossfader (0.0f);
    engine.setTrim (0, 1.0f);   // +12 dB
    engine.setTrim (1, 1.0f);
    engine.loadTrack (0, test::makeSineTrack (2.0, 100.0, kSr, 0.9f));
    engine.loadTrack (1, test::makeSineTrack (2.0, 100.0, kSr, 0.9f));
    engine.play (0);
    engine.play (1);

    const auto out = test::run (engine, 48000);
    float maxAbs = 0.0f;
    for (size_t i = 0; i < out.left.size(); ++i)
        maxAbs = std::max ({ maxAbs, std::abs (out.left[i]), std::abs (out.right[i]) });
    CHECK (maxAbs <= dsp::Limiter::kDefaultCeiling + 1.0e-6f);
    CHECK (engine.getMasterLimiterReductionDb() > 6.0f);
    CHECK (engine.getClippedSamples() > 0);   // lo informa como "habría saturado"
}

// =============================================================================
//  CUE de auriculares y ruteo
// =============================================================================
TEST_CASE ("Ruteo: master en 1-2, auriculares en 3-4", "[routing]")
{
    FourOutputRig rig;

    SECTION ("sin CUE: auriculares en silencio")
    {
        const auto r = rig.measure();
        CHECK (r[0] == Approx (kSineRmsDb).margin (0.1f));
        CHECK (r[1] == Approx (kSineRmsDb).margin (0.1f));
        CHECK (r[2] < kSilent);
        CHECK (r[3] < kSilent);
    }

    SECTION ("CUE del canal con el fader abajo: se escucha solo en auriculares")
    {
        rig.engine.setChannelCue (0, true);
        rig.engine.setChannelVolume (0, 0.0f);
        const auto r = rig.measure();
        CHECK (r[0] < kSilent);
        CHECK (r[2] == Approx (kSineRmsDb).margin (0.1f));   // pre-fader
        CHECK (r[3] == Approx (kSineRmsDb).margin (0.1f));
    }

    SECTION ("CUE MIX al maximo: los auriculares escuchan el master")
    {
        rig.engine.setChannelCue (0, false);
        rig.engine.setCueMix (1.0f);
        const auto r = rig.measure();
        CHECK (r[2] == Approx (kSineRmsDb).margin (0.1f));
    }

    SECTION ("el volumen MASTER no afecta a los auriculares")
    {
        rig.engine.setChannelCue (0, true);
        rig.engine.setMasterVolume (0.0f);
        const auto r = rig.measure();
        CHECK (r[0] < kSilent);
        CHECK (r[2] == Approx (kSineRmsDb).margin (0.1f));
    }

    SECTION ("volumen de auriculares")
    {
        rig.engine.setChannelCue (0, true);
        rig.engine.setHeadphoneVolume (0.5f);   // -12 dB
        const auto r = rig.measure();
        CHECK (r[2] == Approx (kSineRmsDb - 12.04f).margin (0.1f));
    }

    SECTION ("master en 3-4 y auriculares en 1-2")
    {
        rig.engine.setMasterOutputPair (1);
        rig.engine.setChannelCue (0, true);
        rig.engine.setChannelVolume (0, 0.0f);
        const auto r = rig.measure();
        CHECK (r[0] == Approx (kSineRmsDb).margin (0.1f));   // auriculares
        CHECK (r[2] < kSilent);                               // master (fader abajo)
    }

    SECTION ("SPLIT: izquierdo = master mono, derecho = auriculares mono; 3-4 en silencio")
    {
        rig.engine.setSplit (true);
        rig.engine.setChannelCue (0, true);
        rig.engine.setChannelVolume (0, 0.0f);   // master mudo, cue sonando
        auto r = rig.measure();
        CHECK (r[0] < kSilent);
        CHECK (r[1] == Approx (kSineRmsDb).margin (0.1f));
        CHECK (r[2] < kSilent);
        CHECK (r[3] < kSilent);

        rig.engine.setChannelVolume (0, 1.0f);
        rig.engine.setChannelCue (0, false);     // master sonando, cue mudo
        r = rig.measure();
        CHECK (r[0] == Approx (kSineRmsDb).margin (0.1f));
        CHECK (r[1] < kSilent);
    }
}

TEST_CASE ("Ruteo: con una placa de 2 salidas el master nunca se pierde", "[routing]")
{
    AudioEngine engine;
    test::prepareUnity (engine);
    engine.setMasterOutputPair (1);   // pidió 3-4 pero no existen
    engine.loadTrack (0, test::makeSineTrack (2.0, 1000.0, kSr, 0.5f));
    engine.play (0);
    const auto out = test::run (engine, 24000);
    CHECK (db (test::rms (out.left.data() + 12000, 12000)) == Approx (kSineRmsDb).margin (0.1f));
}

// =============================================================================
//  Crossfader
// =============================================================================
TEST_CASE ("Crossfader: curvas suave, corte y scratch, e inversion", "[mixer][crossfader]")
{
    float a = 0, b = 0;

    SECTION ("corte: los dos al máximo en casi todo el recorrido")
    {
        AudioEngine::crossfaderGains (0.0f, CrossfaderCurve::cut, false, a, b);
        CHECK (a == Approx (1.0f));
        CHECK (b == Approx (1.0f));
        AudioEngine::crossfaderGains (0.75f, CrossfaderCurve::cut, false, a, b);   // 87,5 % del recorrido
        CHECK (a == Approx (1.0f));
        AudioEngine::crossfaderGains (1.0f, CrossfaderCurve::cut, false, a, b);
        CHECK (juce::exactlyEqual (a, 0.0f));
        CHECK (b == Approx (1.0f));
    }

    SECTION ("scratch: corta en el 2 % de la punta")
    {
        AudioEngine::crossfaderGains (-0.97f, CrossfaderCurve::scratch, false, a, b);   // 1,5 % del recorrido
        CHECK (b == Approx (0.75f));
        AudioEngine::crossfaderGains (-0.9f, CrossfaderCurve::scratch, false, a, b);
        CHECK (b == Approx (1.0f));
    }

    SECTION ("inversión")
    {
        AudioEngine::crossfaderGains (-1.0f, CrossfaderCurve::smooth, true, a, b);
        CHECK (juce::exactlyEqual (a, 0.0f));
        CHECK (b == Approx (1.0f));
    }
}

// =============================================================================
//  Medición
// =============================================================================
TEST_CASE ("Vumetros: pico, RMS y clip", "[meter]")
{
    MeterSource m;
    const auto s = sine (1000.0, 4800, 0.5f);
    m.addBlock (s.data(), s.data(), 2400);
    m.addBlock (s.data() + 2400, nullptr, 2400);
    auto r = m.read();
    CHECK (r.peak == Approx (0.5f).margin (1e-3));
    CHECK (r.rms == Approx (0.5f / std::sqrt (2.0f)).margin (1e-3));
    CHECK_FALSE (r.clipped);

    // La lectura reinicia.
    r = m.read();
    CHECK (juce::exactlyEqual (r.peak, 0.0f));

    std::vector<float> loud (64, 1.0f);
    m.addBlock (loud.data(), nullptr, 64);
    CHECK (m.read().clipped);
}

TEST_CASE ("Vumetros: los canales y el master miden lo que suena", "[meter][mixer]")
{
    AudioEngine engine;
    test::prepareUnity (engine);
    engine.setCrossfader (0.0f);
    engine.loadTrack (0, test::makeSineTrack (2.0, 1000.0, kSr, 0.5f));
    engine.play (0);
    test::run (engine, 24000);
    engine.readMeter (MeterId::channel1);   // descarta el arranque
    engine.readMeter (MeterId::masterLeft);
    test::run (engine, 4800);

    const auto ch1 = engine.readMeter (MeterId::channel1);
    const auto ch2 = engine.readMeter (MeterId::channel2);
    const auto mL  = engine.readMeter (MeterId::masterLeft);
    CHECK (ch1.peak == Approx (0.5f).margin (0.01f));       // pre-fader, post-EQ
    CHECK (juce::exactlyEqual (ch2.peak, 0.0f));
    CHECK (mL.peak == Approx (0.5f * std::sqrt (0.5f)).margin (0.01f));   // crossfader al centro (-3 dB)
}

TEST_CASE ("Balistica: ataque instantaneo, caida suave, retencion y clip", "[meter]")
{
    MeterBallistics b;
    b.update (1.0f, 0.5f, false, 1.0f / 60.0f);
    CHECK (b.getPeakDb() == Approx (0.0f));          // ataque instantáneo
    CHECK (b.getHoldDb() == Approx (0.0f));

    // Silencio durante 1 s: el pico cae 18 dB, la retención sigue arriba.
    for (int i = 0; i < 60; ++i)
        b.update (0.0f, 0.0f, false, 1.0f / 60.0f);
    CHECK (b.getPeakDb() == Approx (-18.0f).margin (0.5f));
    CHECK (b.getHoldDb() == Approx (0.0f));

    // Pasado 1,5 s la retención empieza a caer.
    for (int i = 0; i < 60; ++i)
        b.update (0.0f, 0.0f, false, 1.0f / 60.0f);
    CHECK (b.getHoldDb() < 0.0f);
    CHECK (b.getHoldDb() > -8.0f);

    // Clip: encendido 2 s.
    b.update (1.0f, 1.0f, true, 1.0f / 60.0f);
    CHECK (b.isClipLit());
    for (int i = 0; i < 130; ++i)
        b.update (0.0f, 0.0f, false, 1.0f / 60.0f);
    CHECK_FALSE (b.isClipLit());

    CHECK (juce::exactlyEqual (MeterBallistics::toDb (0.0f), MeterBallistics::kFloorDb));
}

// =============================================================================
//  Tiempo real con todo el mixer en movimiento
// =============================================================================
TEST_CASE ("Tiempo real: el mixer completo no reserva memoria", "[realtime][mixer]")
{
    AudioEngine engine;
    engine.prepare ({ kSr, 256, 4 });
    auto a = test::makeSineTrack (5.0, 440.0);
    auto b = test::makeSineTrack (5.0, 330.0);
    engine.loadTrack (0, a);
    engine.loadTrack (1, b);
    engine.play (0);
    engine.play (1);

    std::vector<std::vector<float>> o (4, std::vector<float> (256));
    float* outs[] = { o[0].data(), o[1].data(), o[2].data(), o[3].data() };

    int allocations = 0;
    for (int block = 0; block < 400; ++block)
    {
        // Se mueve todo, como un DJ apurado.
        const auto t = (float) block / 400.0f;
        engine.setEq (block % 2, (EqBand) (block % 3), t);
        engine.setFilter (0, t);
        engine.setTrim (1, 1.0f - t);
        engine.setChannelCue (block % 2, block % 7 < 3);
        engine.setCueMix (t);
        engine.setCrossfaderCurve ((CrossfaderCurve) (block % 3));
        engine.setSplit (block % 50 < 25);
        engine.setMasterOutputPair ((block / 100) % 2);

        test::AllocationGuard guard;
        engine.process (outs, 4, 256);
        allocations += guard.count();
    }
    CHECK (allocations == 0);

    for (int m = 0; m < (int) MeterId::count; ++m)
        engine.readMeter ((MeterId) m);
}

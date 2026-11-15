// Pruebas del motor: ciclo de vida, mixer, decks, CUE estilo CDJ, clicks,
// liberación de pistas y reglas de tiempo real.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/AudioEngine.h"
#include "engine/LockFreeQueue.h"

#include <chrono>
#include <thread>
#include <vector>

using namespace kb;
using namespace kb::engine;
using Catch::Approx;

namespace
{
    // Buffers de salida "sucios" para comprobar que el motor los limpia.
    struct Outputs
    {
        Outputs (int channels, int samples)
            : data (static_cast<size_t> (channels), std::vector<float> (static_cast<size_t> (samples), 0.123f))
        {
            for (auto& ch : data)
                ptrs.push_back (ch.data());
        }

        bool allZero() const
        {
            for (const auto& ch : data)
                for (auto s : ch)
                    if (! juce::exactlyEqual (s, 0.0f))
                        return false;
            return true;
        }

        std::vector<std::vector<float>> data;
        std::vector<float*> ptrs;
    };

    constexpr double kSr = 48000.0;
}

// =============================================================================
//  Ciclo de vida
// =============================================================================
TEST_CASE ("AudioEngine: prepare guarda la configuracion", "[engine]")
{
    AudioEngine engine;
    REQUIRE_FALSE (engine.isPrepared());

    engine.prepare ({ 44100.0, 256, 4 });
    CHECK (engine.isPrepared());
    CHECK (juce::exactlyEqual (engine.getSampleRate(), 44100.0));
    CHECK (engine.getMaxBlockSize() == 256);
    CHECK (engine.getNumOutputs() == 4);

    engine.release();
    CHECK_FALSE (engine.isPrepared());
}

TEST_CASE ("AudioEngine: limita los canales al maximo soportado", "[engine]")
{
    AudioEngine engine;
    engine.prepare ({ 48000.0, 128, 16 });
    CHECK (engine.getNumOutputs() == AudioEngine::kMaxOutputChannels);
}

TEST_CASE ("AudioEngine: sin pistas genera silencio en todas las salidas", "[engine]")
{
    AudioEngine engine;
    engine.prepare ({ 48000.0, 512, 4 });

    Outputs out (4, 512);
    engine.process (out.ptrs.data(), 4, 512);
    CHECK (out.allZero());
    CHECK (engine.getProcessedBlocks() == 1);
}

TEST_CASE ("AudioEngine: process es robusto ante entradas atipicas", "[engine]")
{
    AudioEngine engine;
    engine.prepare ({ 48000.0, 64, 2 });

    SECTION ("bloque mas grande que maxBlockSize")
    {
        Outputs out (2, 1024);
        engine.process (out.ptrs.data(), 2, 1024);
        CHECK (out.allZero());
    }

    SECTION ("punteros nulos y bloques vacios")
    {
        engine.process (nullptr, 2, 64);
        Outputs out (2, 64);
        engine.process (out.ptrs.data(), 2, 0);
        float* withNull[] = { out.ptrs[0], nullptr };
        engine.process (withNull, 2, 64);
        CHECK (juce::exactlyEqual (out.data[0][0], 0.0f));
    }

    SECTION ("sin prepare previo no falla")
    {
        AudioEngine fresh;
        Outputs out (2, 32);
        fresh.process (out.ptrs.data(), 2, 32);
        CHECK (out.allZero());
    }
}

// =============================================================================
//  Curvas del mixer
// =============================================================================
TEST_CASE ("Mixer: curva del fader de canal", "[mixer]")
{
    CHECK (AudioEngine::faderGain (1.0f) == Approx (1.0f));
    CHECK (AudioEngine::faderGain (0.0f) == Approx (0.0f));
    CHECK (juce::Decibels::gainToDecibels (AudioEngine::faderGain (0.5f)) == Approx (-12.04f).margin (0.1f));
    CHECK (AudioEngine::faderGain (2.0f) == Approx (1.0f));   // se limita
}

TEST_CASE ("Mixer: crossfader de potencia constante", "[mixer]")
{
    float a = 0, b = 0;
    AudioEngine::crossfaderGains (-1.0f, a, b);
    CHECK (a == Approx (1.0f));
    CHECK (b == Approx (0.0f));

    AudioEngine::crossfaderGains (1.0f, a, b);
    CHECK (a == Approx (0.0f));
    CHECK (b == Approx (1.0f));

    AudioEngine::crossfaderGains (0.0f, a, b);
    CHECK (a == Approx (std::sqrt (0.5f)));
    CHECK (b == Approx (std::sqrt (0.5f)));

    // Potencia constante en todo el recorrido.
    for (float x = -1.0f; x <= 1.0f; x += 0.1f)
    {
        AudioEngine::crossfaderGains (x, a, b);
        CHECK (a * a + b * b == Approx (1.0f).margin (1e-4));
    }
}

// =============================================================================
//  Reproducción
// =============================================================================
TEST_CASE ("Deck: reproduce la pista sin alterarla (ganancia unidad)", "[deck]")
{
    AudioEngine engine;
    test::prepareUnity (engine);

    auto track = test::makeRampTrack (48000);
    engine.loadTrack (0, track);
    engine.play (0);

    const auto out = test::run (engine, 9600);   // 200 ms

    // Pasados el fundido de entrada y el suavizado (20 ms), la salida es la pista exacta.
    const auto st = engine.getDeckStatus (0);
    CHECK (st.playing);
    CHECK (st.positionSeconds == Approx (0.2).margin (1e-6));

    for (int i = 4800; i < 9600; i += 97)
        CHECK (out.left[(size_t) i] == Approx (track->getReadPointer (0)[i]).margin (1e-6));
}

TEST_CASE ("Deck: se detiene al final de la pista", "[deck]")
{
    AudioEngine engine;
    test::prepareUnity (engine);
    engine.loadTrack (0, test::makeSineTrack (0.1, 440.0));
    engine.play (0);

    test::run (engine, 9600);   // 200 ms > 100 ms de pista
    const auto st = engine.getDeckStatus (0);
    CHECK_FALSE (st.playing);
    CHECK (st.positionSeconds == Approx (0.1).margin (1e-4));

    // Play al final no hace nada.
    engine.play (0);
    test::run (engine, 256);
    CHECK_FALSE (engine.getDeckStatus (0).playing);
}

TEST_CASE ("Deck: el volumen del canal y el crossfader se aplican", "[deck][mixer]")
{
    AudioEngine engine;
    test::prepareUnity (engine);
    engine.loadTrack (0, test::makeSineTrack (2.0, 1000.0, kSr, 0.5f));
    engine.play (0);
    test::run (engine, 4800);

    auto steadyRms = [&]
    {
        auto o = test::run (engine, 9600);   // deja asentar el suavizado y mide el final
        return test::rms (o.left.data() + 4800, 4800);
    };

    const auto full = steadyRms();
    CHECK (full == Approx (0.5f / std::sqrt (2.0f)).margin (0.005f));

    engine.setChannelVolume (0, 0.5f);
    CHECK (steadyRms() == Approx (full * 0.25f).margin (0.005f));   // -12 dB

    engine.setChannelVolume (0, 1.0f);
    engine.setCrossfader (1.0f);     // todo al deck 2 (vacío)
    CHECK (steadyRms() < 1.0e-4f);

    engine.setCrossfader (0.0f);     // centro: -3 dB
    CHECK (steadyRms() == Approx (full * std::sqrt (0.5f)).margin (0.005f));
}

TEST_CASE ("Deck: dos decks se mezclan", "[deck][mixer]")
{
    AudioEngine engine;
    test::prepareUnity (engine);
    engine.setCrossfader (0.0f);
    engine.loadTrack (0, test::makeSineTrack (2.0, 440.0, kSr, 0.25f));
    engine.loadTrack (1, test::makeSineTrack (2.0, 440.0, kSr, 0.25f));
    engine.play (0);
    engine.play (1);

    const auto out = test::run (engine, 19200);
    // Misma señal en fase en ambos decks: 2 * 0.25 * 0.707 de amplitud.
    CHECK (test::rms (out.left.data() + 9600, 9600) == Approx (2.0f * 0.25f * std::sqrt (0.5f) / std::sqrt (2.0f)).margin (0.005f));
}

// =============================================================================
//  CUE estilo CDJ
// =============================================================================
TEST_CASE ("Deck: CUE con comportamiento CDJ", "[deck][cue]")
{
    AudioEngine engine;
    test::prepareUnity (engine);
    engine.loadTrack (0, test::makeSineTrack (10.0, 440.0));
    test::run (engine, 256);

    SECTION ("en pausa sobre el cue: preescucha mientras se mantiene, al soltar vuelve")
    {
        engine.cueDown (0);
        test::run (engine, 4800);   // 100 ms
        auto st = engine.getDeckStatus (0);
        CHECK (st.playing);
        CHECK (st.cuePreview);
        CHECK (st.positionSeconds > 0.09);

        engine.cueUp (0);
        test::run (engine, 256);
        st = engine.getDeckStatus (0);
        CHECK_FALSE (st.playing);
        CHECK (st.positionSeconds == Approx (0.0));
    }

    SECTION ("PLAY durante la preescucha: sigue sonando al soltar CUE")
    {
        engine.cueDown (0);
        test::run (engine, 4800);
        engine.play (0);
        engine.cueUp (0);
        test::run (engine, 4800);
        const auto st = engine.getDeckStatus (0);
        CHECK (st.playing);
        CHECK_FALSE (st.cuePreview);
        CHECK (st.positionSeconds > 0.19);
    }

    SECTION ("reproduciendo: CUE vuelve al punto de cue y pausa")
    {
        engine.play (0);
        test::run (engine, 48000);   // 1 s
        engine.cueDown (0);
        engine.cueUp (0);
        test::run (engine, 256);
        const auto st = engine.getDeckStatus (0);
        CHECK_FALSE (st.playing);
        CHECK (st.positionSeconds == Approx (0.0));
    }

    SECTION ("en pausa fuera del cue: fija un nuevo cue")
    {
        engine.seek (0, 2.5);
        engine.cueDown (0);
        engine.cueUp (0);
        test::run (engine, 256);
        auto st = engine.getDeckStatus (0);
        CHECK (st.cueSeconds == Approx (2.5));
        CHECK_FALSE (st.playing);

        // Ahora, reproducir y volver con CUE regresa a 2,5 s.
        engine.play (0);
        test::run (engine, 24000);
        engine.cueDown (0);
        engine.cueUp (0);
        test::run (engine, 256);
        st = engine.getDeckStatus (0);
        CHECK (st.positionSeconds == Approx (2.5));
        CHECK_FALSE (st.playing);
    }

    SECTION ("play/pausa alterna y mantiene la posicion")
    {
        engine.togglePlay (0);
        test::run (engine, 24000);
        engine.togglePlay (0);
        test::run (engine, 256);
        auto st = engine.getDeckStatus (0);
        CHECK_FALSE (st.playing);
        const auto pausedAt = st.positionSeconds;
        CHECK (pausedAt == Approx (0.5).margin (0.01));

        test::run (engine, 24000);
        CHECK (engine.getDeckStatus (0).positionSeconds == Approx (pausedAt));
    }
}

// =============================================================================
//  Sin clicks
// =============================================================================
TEST_CASE ("Deck: play, pausa, saltos, CUE y cargas no generan clicks", "[deck][click]")
{
    // Seno de 1 kHz, amplitud 0.9: la mayor diferencia natural entre muestras es
    // 2*pi*1000/48000*0.9 = 0.118. Un corte brusco produce saltos de hasta 1.8.
    constexpr float kNaturalMaxStep = 0.118f;
    constexpr float kTolerance      = 0.03f;

    AudioEngine engine;
    test::prepareUnity (engine);
    engine.loadTrack (0, test::makeSineTrack (20.0, 1000.0, kSr, 0.9f));

    std::vector<float> all;
    auto chunk = [&] (int frames)
    {
        auto o = test::run (engine, frames, 128);
        all.insert (all.end(), o.left.begin(), o.left.end());
    };

    chunk (1000);
    engine.play (0);          chunk (4801);
    engine.pause (0);         chunk (1237);
    engine.play (0);          chunk (3333);
    engine.seek (0, 7.3331);  chunk (2000);   // salto reproduciendo
    engine.cueDown (0);       chunk (999);    // vuelve al cue y pausa
    engine.cueUp (0);         chunk (500);
    engine.cueDown (0);       chunk (2222);   // preescucha
    engine.cueUp (0);         chunk (777);    // suelta: vuelve
    engine.play (0);          chunk (3000);
    engine.loadTrack (0, test::makeSineTrack (5.0, 1000.0, kSr, 0.9f));   // carga reproduciendo
    chunk (2000);
    engine.play (0);          chunk (3000);
    engine.setChannelVolume (0, 0.0f);   chunk (2000);   // movimiento brusco del fader
    engine.setChannelVolume (0, 1.0f);   chunk (2000);
    engine.setCrossfader (1.0f);         chunk (2000);
    engine.setCrossfader (-1.0f);        chunk (2000);

    CHECK (test::maxStep (all) < kNaturalMaxStep + kTolerance);
    CHECK (engine.getClippedSamples() == 0);
}

// =============================================================================
//  Gestión de memoria de pistas
// =============================================================================
TEST_CASE ("AudioEngine: las pistas reemplazadas se liberan fuera del audio", "[engine][memory]")
{
    AudioEngine engine;
    test::prepareUnity (engine);

    std::weak_ptr<TrackBuffer> first;
    {
        auto t = test::makeSineTrack (1.0, 440.0);
        first = t;
        engine.loadTrack (0, t);
    }
    engine.play (0);
    test::run (engine, 2048);
    CHECK (engine.getNumLiveTracks() == 1);
    CHECK_FALSE (first.expired());

    engine.loadTrack (0, test::makeSineTrack (1.0, 440.0));
    test::run (engine, 2048);   // termina el fundido y el audio suelta la pista vieja
    CHECK_FALSE (first.expired());   // el audio nunca libera memoria por su cuenta

    engine.collectGarbage();
    CHECK (first.expired());
    CHECK (engine.getNumLiveTracks() == 1);

    engine.ejectTrack (0);
    test::run (engine, 2048);
    engine.collectGarbage();
    CHECK (engine.getNumLiveTracks() == 0);
}

TEST_CASE ("AudioEngine: muchas cargas seguidas no pierden pistas", "[engine][memory]")
{
    AudioEngine engine;
    test::prepareUnity (engine);
    engine.loadTrack (0, test::makeSineTrack (1.0, 440.0));
    engine.play (0);

    for (int i = 0; i < 50; ++i)
    {
        engine.loadTrack (i % 2, test::makeSineTrack (0.5, 440.0));
        engine.play (i % 2);
        test::run (engine, 64);
        engine.collectGarbage();
    }
    test::run (engine, 4096);
    engine.collectGarbage();
    CHECK (engine.getNumLiveTracks() == 2);
}

// =============================================================================
//  Reglas de tiempo real
// =============================================================================
TEST_CASE ("Tiempo real: process() no reserva ni libera memoria", "[engine][realtime]")
{
    AudioEngine engine;
    test::prepareUnity (engine, 48000.0, 256);
    auto a = test::makeSineTrack (5.0, 440.0);
    auto b = test::makeSineTrack (5.0, 330.0, 44100.0);   // otro sample rate: interpola
    auto c = test::makeSineTrack (5.0, 550.0);

    std::vector<float> l (256), r (256);
    float* outs[] = { l.data(), r.data() };

    // Los comandos se encolan fuera de la medición (son del hilo de control).
    engine.loadTrack (0, a);
    engine.loadTrack (1, b);
    engine.play (0);
    engine.play (1);
    engine.setCrossfader (0.0f);

    int allocations = 0;
    auto audioCallback = [&] (int blocks)
    {
        test::AllocationGuard guard;
        for (int i = 0; i < blocks; ++i)
            engine.process (outs, 2, 256);
        allocations += guard.count();
    };

    audioCallback (50);
    engine.seek (0, 3.0);     audioCallback (5);
    engine.cueDown (1);       audioCallback (5);
    engine.cueUp (1);         audioCallback (5);
    engine.pause (0);         audioCallback (5);
    engine.loadTrack (0, c);  audioCallback (20);   // carga + fundido + retiro de la vieja
    engine.ejectTrack (1);    audioCallback (20);
    engine.setMasterVolume (0.3f); audioCallback (5);

    {
        // Bloque más grande que maxBlockSize (algunos drivers lo hacen).
        std::vector<float> bl (1000), br (1000);
        float* big[] = { bl.data(), br.data() };
        test::AllocationGuard guard;
        engine.process (big, 2, 1000);
        allocations += guard.count();
    }

    CHECK (allocations == 0);
    engine.collectGarbage();
}

TEST_CASE ("Tiempo real: la cola SPSC es correcta entre dos hilos", "[engine][realtime]")
{
    SpscQueue<int, 64> q;
    constexpr int kCount = 200000;
    std::thread producer ([&]
    {
        for (int i = 0; i < kCount; ++i)
            while (! q.push (i))
                std::this_thread::yield();
    });

    int expected = 0;
    bool inOrder = true;
    while (expected < kCount)
    {
        int v;
        if (q.pop (v))
        {
            inOrder = inOrder && (v == expected);
            ++expected;
        }
        else
        {
            std::this_thread::yield();
        }
    }
    producer.join();
    CHECK (inOrder);
    CHECK (q.isEmpty());
}

TEST_CASE ("Rendimiento: 2 decks mezclando cuestan una fraccion minima de CPU", "[engine][perf]")
{
    AudioEngine engine;
    test::prepareUnity (engine, 48000.0, 128);
    engine.setCrossfader (0.0f);
    engine.loadTrack (0, test::makeSineTrack (70.0, 440.0));
    engine.loadTrack (1, test::makeSineTrack (70.0, 330.0, 44100.0));   // con interpolación
    engine.play (0);
    engine.play (1);

    std::vector<float> l (128), r (128);
    float* outs[] = { l.data(), r.data() };
    constexpr int kSeconds = 60;
    const int blocks = (int) (kSeconds * 48000 / 128);

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < blocks; ++i)
        engine.process (outs, 2, 128);
    const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();

    const auto realtimeFactor = kSeconds / elapsed;
    INFO ("Factor de tiempo real: " << realtimeFactor << "x (1 minuto de audio en " << elapsed * 1000.0 << " ms)");
    WARN ("Rendimiento del motor: " << (int) realtimeFactor << "x tiempo real ("
          << (100.0 / realtimeFactor) << " % de un núcleo)");
    // Incluso en Debug y en una CI lenta, tiene que sobrar margen de sobra.
    CHECK (realtimeFactor > 20.0);
}

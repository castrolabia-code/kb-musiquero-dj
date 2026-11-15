// Pruebas de tempo y SYNC: velocidad, igualación de BPM (incluido mitad/doble),
// alineación de fase al activar y estabilidad durante 5 minutos.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/AudioEngine.h"

#include <cmath>

using namespace kb;
using namespace kb::engine;
using Catch::Approx;

namespace
{
    // Frecuencia baja para que 5+ minutos de pista no ocupen cientos de MB.
    constexpr double kSr = 8000.0;

    std::shared_ptr<TrackBuffer> beatTrack (double seconds, double bpm, double firstBeat)
    {
        auto t = test::makeSineTrack (seconds, 220.0, kSr, 0.3f);
        t->setBeatInfo (bpm, firstBeat);
        return t;
    }

    /** Fase de beat (0..1) de un deck a partir del estado publicado. */
    double beatPhase (const AudioEngine& e, int deck, double bpm, double firstBeat)
    {
        const auto beats = (e.getDeckStatus (deck).positionSeconds - firstBeat) * bpm / 60.0;
        return beats - std::floor (beats);
    }

    double phaseDistance (double a, double b)
    {
        auto d = std::abs (a - b);
        return std::min (d, 1.0 - d);
    }

    void prepare (AudioEngine& e)
    {
        e.prepare ({ kSr, 256, 2 });
        e.setMasterVolume (1.0f);
    }
}

TEST_CASE ("Tempo: el fader cambia la velocidad", "[tempo]")
{
    AudioEngine e;
    prepare (e);
    e.loadTrack (0, beatTrack (30.0, 120.0, 0.0));
    e.setTempo (0, 10.0f);
    e.play (0);
    test::run (e, (int) (kSr * 10));   // 10 s reales
    CHECK (e.getDeckStatus (0).positionSeconds == Approx (11.0).margin (0.01));

    e.setTempo (0, -8.0f);
    const auto before = e.getDeckStatus (0).positionSeconds;
    test::run (e, (int) (kSr * 5));
    CHECK (e.getDeckStatus (0).positionSeconds - before == Approx (4.6).margin (0.01));

    e.setTempo (0, 200.0f);   // se limita a ±50 %
    CHECK (e.getTempo (0) == Approx (50.0f));
}

TEST_CASE ("SYNC: relacion de velocidad, con mitad y doble tempo", "[sync]")
{
    CHECK (AudioEngine::syncRatio (128.0, 124.0) == Approx (128.0 / 124.0));
    CHECK (AudioEngine::syncRatio (140.0, 72.0) == Approx (70.0 / 72.0));    // el lento va a 70
    CHECK (AudioEngine::syncRatio (87.0, 172.0) == Approx (174.0 / 172.0));
    CHECK (AudioEngine::syncRatio (0.0, 120.0) == Approx (1.0));             // sin BPM no hace nada
}

TEST_CASE ("SYNC: iguala el tempo y alinea los beats al activarlo", "[sync]")
{
    AudioEngine e;
    prepare (e);
    e.loadTrack (0, beatTrack (60.0, 128.0, 0.10));
    e.loadTrack (1, beatTrack (60.0, 124.0, 0.37));
    e.setTempo (0, 2.0f);   // master a 130,56 BPM efectivos
    e.play (0);
    e.play (1);
    test::run (e, (int) (kSr * 3.3));   // fases arbitrarias

    e.toggleSync (1);
    test::run (e, (int) (kSr * 0.5));

    CHECK (e.getSync (1));
    CHECK_FALSE (e.getSync (0));
    CHECK (e.getTempo (1) == Approx ((128.0 * 1.02 / 124.0 - 1.0) * 100.0).margin (0.01));
    CHECK (phaseDistance (beatPhase (e, 0, 128.0, 0.10), beatPhase (e, 1, 124.0, 0.37)) < 0.01);

    // El master cambia el tempo: el esclavo lo sigue.
    e.setTempo (0, -3.0f);
    test::run (e, (int) (kSr * 1.0));
    CHECK (e.getTempo (1) == Approx ((128.0 * 0.97 / 124.0 - 1.0) * 100.0).margin (0.01));

    // Mover el tempo del esclavo a mano suelta el SYNC.
    e.setTempo (1, 0.0f);
    test::run (e, 256);
    CHECK_FALSE (e.getSync (1));
}

TEST_CASE ("SYNC: activarlo en un deck lo desactiva en el otro", "[sync]")
{
    AudioEngine e;
    prepare (e);
    e.loadTrack (0, beatTrack (10.0, 128.0, 0.0));
    e.loadTrack (1, beatTrack (10.0, 126.0, 0.0));
    e.toggleSync (0);
    test::run (e, 256);
    e.toggleSync (1);
    test::run (e, 256);
    CHECK_FALSE (e.getSync (0));
    CHECK (e.getSync (1));
}

TEST_CASE ("SYNC: al dar PLAY con SYNC activo arranca en fase", "[sync]")
{
    AudioEngine e;
    prepare (e);
    e.loadTrack (0, beatTrack (60.0, 125.0, 0.05));
    e.loadTrack (1, beatTrack (60.0, 121.0, 0.29));
    e.play (0);
    test::run (e, (int) (kSr * 2.0));
    e.toggleSync (1);          // en pausa: solo iguala el tempo
    e.seek (1, 1.234);         // cualquier punto
    test::run (e, 256);
    e.play (1);
    test::run (e, (int) (kSr * 0.25));
    CHECK (phaseDistance (beatPhase (e, 0, 125.0, 0.05), beatPhase (e, 1, 121.0, 0.29)) < 0.01);
}

TEST_CASE ("SYNC: mantiene la fase durante 5 minutos", "[sync][long]")
{
    AudioEngine e;
    prepare (e);
    e.loadTrack (0, beatTrack (330.0, 128.0, 0.12));
    e.loadTrack (1, beatTrack (400.0, 123.37, 0.44));
    e.setTempo (0, 1.5f);
    e.play (0);
    e.play (1);
    test::run (e, (int) (kSr * 1.0));
    e.toggleSync (1);

    double worst = 0.0;
    for (int second = 0; second < 300; ++second)
    {
        test::run (e, (int) kSr);
        if (second > 1)
            worst = std::max (worst, phaseDistance (beatPhase (e, 0, 128.0, 0.12), beatPhase (e, 1, 123.37, 0.44)));
    }
    INFO ("Peor desfase en 5 minutos: " << worst << " beats (" << worst * 60.0 / 128.0 * 1000.0 << " ms)");
    CHECK (worst < 0.01);   // < 5 ms
    CHECK (e.getDeckStatus (0).playing);
    CHECK (e.getDeckStatus (1).playing);
}

TEST_CASE ("SYNC: sin BPM detectado no cambia nada", "[sync]")
{
    AudioEngine e;
    prepare (e);
    e.loadTrack (0, test::makeSineTrack (10.0, 220.0, kSr));   // sin análisis
    e.loadTrack (1, beatTrack (10.0, 128.0, 0.0));
    e.toggleSync (0);
    test::run (e, 1024);
    CHECK (e.getTempo (0) == Approx (0.0f));
}

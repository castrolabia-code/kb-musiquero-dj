// Pruebas de Key Lock y key shift (Signalsmith Stretch): el tono se mantiene al
// cambiar el tempo, el key shift transpone, la posición publicada coincide con
// lo que se escucha, y todo sin reservar memoria en el audio.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/AudioEngine.h"

#include <chrono>
#include <cmath>

using namespace kb;
using namespace kb::engine;
using Catch::Approx;

namespace
{
    constexpr double kSr = 44100.0;

    double frequencyOf (const std::vector<float>& x, size_t from, size_t n)
    {
        long first = -1, last = -1, count = 0;
        for (size_t i = from + 1; i < from + n && i < x.size(); ++i)
            if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            {
                if (first < 0) first = (long) i;
                last = (long) i;
                ++count;
            }
        return count > 1 ? (double) (count - 1) * kSr / (double) (last - first) : 0.0;
    }

    struct Rig
    {
        Rig()
        {
            engine.prepare ({ kSr, 512, 2 });
            engine.setMasterVolume (1.0f);
            engine.setCrossfader (-1.0f);
        }
        AudioEngine engine;
    };
}

TEST_CASE ("Key Lock: al subir el tempo el tono no cambia", "[keylock]")
{
    Rig rig;
    rig.engine.loadTrack (0, test::makeSineTrack (10.0, 1000.0, kSr, 0.4f));
    rig.engine.setTempo (0, 10.0f);

    SECTION ("sin Key Lock: sube el tono (como un vinilo)")
    {
        rig.engine.play (0);
        const auto out = test::run (rig.engine, (int) kSr, 512);
        CHECK (frequencyOf (out.left, 22050, 11025) == Approx (1100.0).margin (5.0));
    }

    SECTION ("con Key Lock: mismo tono, más rápido")
    {
        rig.engine.setKeyLock (0, true);
        rig.engine.play (0);
        const auto out = test::run (rig.engine, (int) kSr, 512);
        CHECK (frequencyOf (out.left, 22050, 11025) == Approx (1000.0).margin (5.0));
        CHECK (rig.engine.getDeckStatus (0).positionSeconds == Approx (1.1).margin (0.02));
    }
}

TEST_CASE ("Key shift: transpone sin cambiar la velocidad", "[keylock]")
{
    Rig rig;
    rig.engine.loadTrack (0, test::makeSineTrack (10.0, 440.0, kSr, 0.4f));
    rig.engine.setKeyShift (0, 12);   // una octava arriba
    rig.engine.play (0);
    const auto out = test::run (rig.engine, (int) kSr, 512);
    CHECK (frequencyOf (out.left, 22050, 11025) == Approx (880.0).margin (6.0));
    CHECK (rig.engine.getDeckStatus (0).positionSeconds == Approx (1.0).margin (0.02));

    rig.engine.setKeyShift (0, 99);
    CHECK (rig.engine.getKeyShift (0) == 12);   // se limita a ±12
}

TEST_CASE ("Key Lock: la posicion publicada es la que se escucha", "[keylock]")
{
    // Pista con un "click" fuerte cada 0,5 s; con Key Lock y tempo 0, el primer
    // click tiene que salir enseguida (la latencia del stretch se compensa).
    TrackInfo info;
    const auto frames = (std::int64_t) (kSr * 6);
    auto t = std::make_shared<TrackBuffer> (info, kSr, frames);
    for (int c = 0; c < 2; ++c)
        for (std::int64_t i = 0; i < frames; ++i)
            t->getWritePointer (c)[i] = (i % (std::int64_t) (kSr / 2) < 200) ? 0.8f : 0.0f;
    t->markComplete (frames);

    Rig rig;
    rig.engine.loadTrack (0, t);
    rig.engine.setKeyLock (0, true);
    rig.engine.seek (0, 1.0);
    rig.engine.play (0);
    const auto out = test::run (rig.engine, (int) (kSr * 1.2), 512);

    // Buscar el segundo click (el de 1,5 s de la pista): debería aparecer a ~0,5 s de salida.
    size_t clickAt = 0;
    for (size_t i = (size_t) (kSr * 0.3); i < out.left.size(); ++i)
        if (std::abs (out.left[i]) > 0.3f) { clickAt = i; break; }
    INFO ("Click a " << clickAt / kSr << " s de salida");
    CHECK (clickAt / kSr == Approx (0.5).margin (0.015));
}

TEST_CASE ("Key Lock: SYNC entre un deck con Key Lock y otro sin", "[keylock][sync]")
{
    Rig rig;
    auto a = test::makeSineTrack (40.0, 220.0, kSr, 0.3f);
    auto b = test::makeSineTrack (40.0, 330.0, kSr, 0.3f);
    a->setBeatInfo (128.0, 0.1);
    b->setBeatInfo (120.0, 0.3);
    rig.engine.loadTrack (0, a);
    rig.engine.loadTrack (1, b);
    rig.engine.setKeyLock (1, true);
    rig.engine.play (0);
    rig.engine.play (1);
    test::run (rig.engine, (int) kSr, 512);
    rig.engine.toggleSync (1);
    test::run (rig.engine, (int) (kSr * 10), 512);

    const auto pa = rig.engine.getDeckStatus (0).positionSeconds;
    const auto pb = rig.engine.getDeckStatus (1).positionSeconds;
    auto phase = [] (double pos, double bpm, double first) { const auto x = (pos - first) * bpm / 60.0; return x - std::floor (x); };
    auto d = std::abs (phase (pa, 128.0, 0.1) - phase (pb, 120.0, 0.3));
    d = std::min (d, 1.0 - d);
    CHECK (d < 0.01);
}

TEST_CASE ("Key Lock: no reserva memoria en el audio", "[keylock][realtime]")
{
    Rig rig;
    rig.engine.loadTrack (0, test::makeSineTrack (20.0, 440.0, kSr, 0.4f));
    rig.engine.loadTrack (1, test::makeSineTrack (20.0, 550.0, kSr, 0.4f));
    rig.engine.play (0);
    rig.engine.play (1);
    test::run (rig.engine, 4096, 512);

    std::vector<float> l (512), r (512);
    float* outs[] = { l.data(), r.data() };
    int allocations = 0;
    for (int block = 0; block < 300; ++block)
    {
        // Se prende/apaga el Key Lock, se mueve el tempo y el tono, se salta.
        rig.engine.setKeyLock (block % 2, (block / 20) % 2 == 0);
        rig.engine.setTempo (0, (float) (block % 30) - 15.0f);
        rig.engine.setKeyShift (1, (block / 25) % 5 - 2);
        if (block % 50 == 10)
            rig.engine.seek (0, 3.0 + block * 0.01);

        test::AllocationGuard guard;
        rig.engine.process (outs, 2, 512);
        allocations += guard.count();
    }
    CHECK (allocations == 0);
}

TEST_CASE ("Rendimiento: 2 decks con Key Lock", "[keylock][perf]")
{
    Rig rig;
    rig.engine.loadTrack (0, test::makeSineTrack (40.0, 440.0, kSr, 0.4f));
    rig.engine.loadTrack (1, test::makeSineTrack (40.0, 550.0, kSr, 0.4f));
    rig.engine.setKeyLock (0, true);
    rig.engine.setKeyLock (1, true);
    rig.engine.setTempo (0, 6.0f);
    rig.engine.setTempo (1, -4.0f);
    rig.engine.play (0);
    rig.engine.play (1);

    std::vector<float> l (256), r (256);
    float* outs[] = { l.data(), r.data() };
    constexpr int kSeconds = 20;
    const auto blocks = (int) (kSeconds * kSr / 256);
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < blocks; ++i)
        rig.engine.process (outs, 2, 256);
    const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    const auto factor = kSeconds / elapsed;
    WARN ("2 decks con Key Lock: " << (int) factor << "x tiempo real (" << 100.0 / factor << " % de un núcleo)");
   #if defined (NDEBUG)
    CHECK (factor > 10.0);   // Release (lo que se distribuye): ~30x medido
   #else
    CHECK (factor > 1.0);    // Debug: sin optimizaciones, solo que alcance el tiempo real
   #endif
}

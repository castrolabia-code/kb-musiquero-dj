// Pruebas de la Fase 4: hot cues, loops, loop roll, slip, reversa, scratch,
// pitch bend, freno, backspin y sampler. Todo sin clicks ni allocations.
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
    constexpr double kSr = 48000.0;

    struct Rig
    {
        explicit Rig (double bpm = 120.0, double firstBeat = 0.0)
        {
            test::prepareUnity (engine, kSr, 256);
            track = test::makeSineTrack (60.0, 440.0, kSr, 0.5f);
            track->setBeatInfo (bpm, firstBeat);
            engine.loadTrack (0, track);
            test::run (engine, 256);
        }
        double pos() const { return engine.getDeckStatus (0).positionSeconds; }
        void   seconds (double s) { test::run (engine, (int) std::lround (s * kSr)); }

        AudioEngine engine;
        std::shared_ptr<TrackBuffer> track;
    };

    // Seno de 440 Hz a 0.5: el salto natural máximo entre muestras es ~0.029.
    constexpr float kClickThreshold = 0.07f;
}

TEST_CASE ("Hot cues: grabar, saltar y arrancar desde pausa", "[performance][hotcue]")
{
    Rig rig;
    rig.engine.seek (0, 10.0);
    rig.seconds (0.01);
    rig.engine.hotCue (0, 2);          // vacío: graba en 10 s
    rig.seconds (0.01);
    CHECK (rig.engine.getDeckStatus (0).hotCues[2] == Approx (10.0).margin (0.001));
    CHECK (rig.engine.getDeckStatus (0).hotCues[0] < 0.0);

    rig.engine.seek (0, 30.0);
    rig.seconds (0.01);
    rig.engine.hotCue (0, 2);          // salta y reproduce
    rig.seconds (0.5);
    CHECK (rig.engine.getDeckStatus (0).playing);
    CHECK (rig.pos() == Approx (10.5).margin (0.01));

    rig.engine.setHotCue (0, 5, 42.0);
    rig.engine.setHotCue (0, 2, -1.0);   // borrar
    rig.seconds (0.01);
    CHECK (rig.engine.getDeckStatus (0).hotCues[5] == Approx (42.0).margin (0.001));
    CHECK (rig.engine.getDeckStatus (0).hotCues[2] < 0.0);
}

TEST_CASE ("Hot cues: cargar otra pista los borra", "[performance][hotcue]")
{
    Rig rig;
    rig.engine.setHotCue (0, 0, 5.0);
    rig.seconds (0.01);
    rig.engine.loadTrack (0, test::makeSineTrack (10.0, 220.0, kSr));
    rig.seconds (0.01);
    CHECK (rig.engine.getDeckStatus (0).hotCues[0] < 0.0);
}

TEST_CASE ("Auto-loop: cuantizado a la grilla, se mantiene y no hace click", "[performance][loop]")
{
    Rig rig (120.0, 0.25);                 // beats en 0.25, 0.75, 1.25...
    rig.engine.seek (0, 5.4);
    rig.engine.play (0);
    rig.seconds (0.05);                    // 5.45
    rig.engine.autoLoop (0, 1.0);          // 1 beat = 0.5 s desde el beat 5.25
    test::run (rig.engine, 64);
    auto st = rig.engine.getDeckStatus (0);
    REQUIRE (st.loopActive);
    CHECK (st.loopInSeconds == Approx (5.25).margin (0.0005));
    CHECK (st.loopOutSeconds == Approx (5.75).margin (0.0005));

    const auto out = test::run (rig.engine, (int) (kSr * 3));   // 6 vueltas
    CHECK (rig.pos() >= 5.25 - 0.001);
    CHECK (rig.pos() < 5.75 + 0.001);
    CHECK (test::maxStep (out.left) < kClickThreshold);

    // Mismo tamaño otra vez: sale del loop y sigue.
    rig.engine.autoLoop (0, 1.0);
    rig.seconds (1.0);
    CHECK_FALSE (rig.engine.getDeckStatus (0).loopActive);
    CHECK (rig.pos() > 5.75);
}

TEST_CASE ("Loop de 1/32 de beat: sin clicks", "[performance][loop]")
{
    Rig rig (128.0, 0.0);
    rig.engine.seek (0, 3.0);
    rig.engine.play (0);
    rig.seconds (0.1);
    rig.engine.autoLoop (0, 1.0 / 32.0);
    const auto out = test::run (rig.engine, (int) kSr);
    const auto st = rig.engine.getDeckStatus (0);
    CHECK (st.loopOutSeconds - st.loopInSeconds == Approx (60.0 / 128.0 / 32.0).margin (0.0001));
    CHECK (test::maxStep (out.left, 512) < kClickThreshold);
}

TEST_CASE ("Loop: ½ y ×2, IN/OUT manual y reloop", "[performance][loop]")
{
    Rig rig;
    rig.engine.seek (0, 2.0);
    rig.engine.play (0);
    rig.seconds (0.01);
    rig.engine.autoLoop (0, 4.0);   // 2 s
    rig.engine.loopHalve (0);       // 1 s
    rig.seconds (0.01);
    auto st = rig.engine.getDeckStatus (0);
    CHECK (st.loopOutSeconds - st.loopInSeconds == Approx (1.0).margin (0.001));
    rig.engine.loopDouble (0);
    rig.engine.loopDouble (0);      // 4 s
    rig.seconds (0.01);
    st = rig.engine.getDeckStatus (0);
    CHECK (st.loopOutSeconds - st.loopInSeconds == Approx (4.0).margin (0.001));
    rig.engine.loopExit (0);

    // Manual: IN en ~20 s, OUT 1.5 s después -> vuelve a 20 y loopea.
    rig.engine.seek (0, 20.0);
    rig.seconds (0.01);
    rig.engine.loopIn (0);
    rig.seconds (1.5);
    rig.engine.loopOut (0);
    rig.seconds (0.2);
    st = rig.engine.getDeckStatus (0);
    REQUIRE (st.loopActive);
    CHECK (st.loopInSeconds == Approx (20.0).margin (0.02));
    CHECK (st.loopOutSeconds - st.loopInSeconds == Approx (1.5).margin (0.02));
    CHECK (rig.pos() == Approx (st.loopInSeconds + 0.2).margin (0.02));

    rig.engine.loopOut (0);         // con loop activo: sale
    rig.seconds (2.0);
    CHECK_FALSE (rig.engine.getDeckStatus (0).loopActive);
    rig.engine.loopOut (0);         // reloop
    rig.seconds (0.1);
    CHECK (rig.engine.getDeckStatus (0).loopActive);
    CHECK (rig.pos() == Approx (st.loopInSeconds + 0.1).margin (0.02));
}

TEST_CASE ("Loop roll: al soltar sigue donde habría estado", "[performance][roll]")
{
    Rig rig;
    rig.engine.seek (0, 10.0);
    rig.engine.play (0);
    rig.seconds (0.1);
    const auto start = rig.pos();
    rig.engine.loopRoll (0, 0.25, true);
    rig.seconds (1.0);
    CHECK (rig.engine.getDeckStatus (0).rolling);
    CHECK (rig.pos() < start + 0.2);              // está loopeando
    rig.engine.loopRoll (0, 0.25, false);
    rig.seconds (0.1);
    CHECK_FALSE (rig.engine.getDeckStatus (0).loopActive);
    CHECK (rig.pos() == Approx (start + 1.1).margin (0.02));
}

TEST_CASE ("Slip + reversa: vuelve a donde habría estado", "[performance][slip]")
{
    Rig rig;
    rig.engine.seek (0, 10.0);
    rig.engine.play (0);
    rig.engine.setSlip (0, true);
    rig.seconds (0.1);
    rig.engine.setReverse (0, true);
    rig.seconds (0.5);
    CHECK (rig.pos() == Approx (9.6).margin (0.02));   // fue para atrás
    rig.engine.setReverse (0, false);
    rig.seconds (0.1);
    CHECK (rig.pos() == Approx (10.7).margin (0.02));

    // Sin slip, la reversa deja la pista donde quedó.
    rig.engine.setSlip (0, false);
    rig.engine.setReverse (0, true);
    rig.seconds (0.5);
    rig.engine.setReverse (0, false);
    rig.seconds (0.1);
    CHECK (rig.pos() == Approx (10.3).margin (0.02));
}

TEST_CASE ("Censor: reversa mientras se mantiene, con slip", "[performance][fx]")
{
    Rig rig;
    rig.engine.seek (0, 10.0);
    rig.engine.play (0);
    rig.seconds (0.1);
    rig.engine.setDeckEffect (0, Deck::Effect::censor, true);
    rig.seconds (0.4);
    CHECK (rig.pos() < 10.0);
    rig.engine.setDeckEffect (0, Deck::Effect::censor, false);
    rig.seconds (0.1);
    CHECK (rig.pos() == Approx (10.6).margin (0.02));
}

TEST_CASE ("Freno: la pista se detiene y al soltar sigue", "[performance][fx]")
{
    Rig rig;
    rig.engine.seek (0, 10.0);
    rig.engine.play (0);
    rig.seconds (0.1);
    rig.engine.setDeckEffect (0, Deck::Effect::brake, true);
    rig.seconds (Deck::kBrakeSeconds + 0.2);
    const auto stopped = rig.pos();
    // Frenada lineal: recorre la mitad del tiempo de freno.
    CHECK (stopped == Approx (10.1 + Deck::kBrakeSeconds * 0.5).margin (0.03));
    const auto tail = test::run (rig.engine, 4800);
    CHECK (test::rms (tail.left.data(), tail.left.size()) < 0.01f);
    CHECK (rig.pos() == Approx (stopped).margin (0.001));

    rig.engine.setDeckEffect (0, Deck::Effect::brake, false);
    rig.seconds (0.5);
    CHECK (rig.pos() == Approx (stopped + 0.5).margin (0.01));
}

TEST_CASE ("Backspin: va para atrás y al soltar vuelve (slip)", "[performance][fx]")
{
    Rig rig;
    rig.engine.seek (0, 10.0);
    rig.engine.play (0);
    rig.seconds (0.1);
    rig.engine.setDeckEffect (0, Deck::Effect::backspin, true);
    rig.seconds (0.5);
    CHECK (rig.pos() < 9.6);    // ~3x hacia atrás, frenando
    rig.engine.setDeckEffect (0, Deck::Effect::backspin, false);
    rig.seconds (0.1);
    CHECK (rig.pos() == Approx (10.7).margin (0.02));
}

TEST_CASE ("Scratch: la pista sigue a la mano y al soltar sigue normal", "[performance][jog]")
{
    Rig rig;
    rig.engine.seek (0, 10.0);
    rig.engine.play (0);
    rig.seconds (0.1);
    rig.engine.scratchBegin (0);
    rig.seconds (0.2);
    const auto held = rig.pos();
    CHECK (held == Approx (10.1).margin (0.02));    // mano quieta: disco quieto
    CHECK (rig.engine.getDeckStatus (0).scratching);

    for (int i = 0; i < 10; ++i) { rig.engine.scratchMove (0, 0.05); rig.seconds (0.02); }
    rig.seconds (0.1);
    CHECK (rig.pos() == Approx (held + 0.5).margin (0.01));
    for (int i = 0; i < 10; ++i) { rig.engine.scratchMove (0, -0.03); rig.seconds (0.02); }
    rig.seconds (0.1);
    CHECK (rig.pos() == Approx (held + 0.2).margin (0.01));

    rig.engine.scratchEnd (0);
    rig.seconds (0.5);
    CHECK (rig.pos() == Approx (held + 0.7).margin (0.02));
}

TEST_CASE ("Scratch en pausa mueve la pista; sin clicks al agarrar el disco", "[performance][jog]")
{
    Rig rig;
    rig.engine.seek (0, 5.0);
    rig.seconds (0.01);
    rig.engine.scratchBegin (0);
    rig.engine.scratchMove (0, 0.25);
    rig.seconds (0.2);
    CHECK (rig.pos() == Approx (5.25).margin (0.005));
    rig.engine.scratchEnd (0);
    rig.seconds (0.2);
    CHECK_FALSE (rig.engine.getDeckStatus (0).playing);
    CHECK (rig.pos() == Approx (5.25).margin (0.005));

    rig.engine.play (0);
    rig.seconds (0.2);
    rig.engine.scratchBegin (0);    // agarrar un disco que suena: frena suave
    const auto out = test::run (rig.engine, 4800);
    CHECK (test::maxStep (out.left) < kClickThreshold);
}

TEST_CASE ("Pitch bend: empujar el plato acelera un momento", "[performance][jog]")
{
    Rig rig;
    rig.engine.seek (0, 10.0);
    rig.engine.play (0);
    rig.seconds (0.1);
    const auto before = rig.pos();
    for (int i = 0; i < 25; ++i) { rig.engine.nudge (0, 0.1); rig.seconds (0.02); }   // 0.5 s empujando a +10 %
    const auto pushed = rig.pos() - before;
    CHECK (pushed > 0.53);
    CHECK (pushed < 0.56);
    rig.seconds (1.0);   // se suelta: vuelve a 1x
    const auto t0 = rig.pos();
    rig.seconds (0.5);
    CHECK (rig.pos() - t0 == Approx (0.5).margin (0.002));
}

TEST_CASE ("Sampler: one-shot, loop, stop y volumen", "[performance][sampler]")
{
    AudioEngine engine;
    test::prepareUnity (engine, kSr, 256);
    auto shot = test::makeSineTrack (0.3, 1000.0, kSr, 0.5f);
    REQUIRE (engine.loadSample (3, shot));
    test::run (engine, 256);

    engine.triggerSample (3);
    auto out = test::run (engine, (int) (kSr * 0.1));
    CHECK (test::rms (out.left.data() + 2400, 2400) > 0.25f);   // suena aunque no haya decks
    CHECK (engine.getSampleProgress (3) >= 0.0f);
    test::run (engine, (int) (kSr * 0.3));
    CHECK (engine.getSampleProgress (3) < 0.0f);                 // terminó solo

    engine.setSampleLooping (3, true);
    engine.triggerSample (3);
    test::run (engine, (int) kSr);
    CHECK (engine.getSampleProgress (3) >= 0.0f);                // loop: sigue sonando
    engine.setSampleVolume (3, 0.0f);
    out = test::run (engine, 4800);
    CHECK (test::rms (out.left.data() + 2400, 2400) < 0.001f);
    engine.setSampleVolume (3, 0.8f);
    engine.triggerSample (3);                                    // loop: tocar de nuevo lo apaga
    out = test::run (engine, 4800);
    CHECK (engine.getSampleProgress (3) < 0.0f);
    CHECK (test::maxStep (out.left) < 0.12f);

    // Reemplazar el sample libera el anterior.
    engine.loadSample (3, test::makeSineTrack (0.2, 500.0, kSr));
    shot.reset();
    test::run (engine, 512);
    engine.collectGarbage();
    CHECK (engine.getNumLiveTracks() == 1);
}

TEST_CASE ("Fase 4: ningún comando reserva memoria en el hilo de audio", "[performance][realtime]")
{
    Rig rig;
    auto sample = test::makeSineTrack (0.5, 800.0, kSr);
    rig.engine.loadSample (0, sample);
    rig.engine.setKeyLock (0, true);
    rig.engine.play (0);
    rig.seconds (0.2);

    std::vector<float> l (256), r (256);
    float* outs[] = { l.data(), r.data() };
    int allocations = 0;
    auto audio = [&] (int blocks)
    {
        test::AllocationGuard guard;
        for (int i = 0; i < blocks; ++i)
            rig.engine.process (outs, 2, 256);
        allocations += guard.count();
    };

    rig.engine.hotCue (0, 0);              audio (5);
    rig.engine.hotCue (0, 0);              audio (20);
    rig.engine.autoLoop (0, 0.5);          audio (40);
    rig.engine.loopHalve (0);              audio (20);
    rig.engine.loopExit (0);               audio (5);
    rig.engine.loopRoll (0, 0.25, true);   audio (20);
    rig.engine.loopRoll (0, 0.25, false);  audio (5);
    rig.engine.setSlip (0, true);
    rig.engine.setReverse (0, true);       audio (20);
    rig.engine.setReverse (0, false);      audio (5);
    rig.engine.scratchBegin (0);
    rig.engine.scratchMove (0, -0.2);      audio (20);
    rig.engine.scratchEnd (0);             audio (5);
    rig.engine.setDeckEffect (0, Deck::Effect::brake, true);    audio (20);
    rig.engine.setDeckEffect (0, Deck::Effect::brake, false);   audio (5);
    rig.engine.setDeckEffect (0, Deck::Effect::backspin, true); audio (20);
    rig.engine.setDeckEffect (0, Deck::Effect::backspin, false); audio (5);
    rig.engine.nudge (0, 0.1);             audio (10);
    rig.engine.triggerSample (0);          audio (30);
    rig.engine.stopSample (0);             audio (5);

    CHECK (allocations == 0);
}


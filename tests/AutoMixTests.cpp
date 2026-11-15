// Pruebas del AUTO MIX: pase completo de una bandeja a la otra, sincronía,
// bass swap, nivel parejo, cancelación y casos sin pista.
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
    constexpr double kSr = 8000.0;

    std::shared_ptr<TrackBuffer> beatTrack (double seconds, double freq, double bpm, double firstBeat)
    {
        auto t = test::makeSineTrack (seconds, freq, kSr, 0.4f);
        t->setBeatInfo (bpm, firstBeat);
        return t;
    }

    struct Rig
    {
        Rig()
        {
            engine.prepare ({ kSr, 256, 2 });
            engine.setMasterVolume (1.0f);
            engine.setCrossfader (-1.0f);   // suena el deck 1
        }
        AudioEngine engine;
    };
}

TEST_CASE ("AUTO MIX: pasa del deck 1 al deck 2 en el tiempo pedido", "[automix]")
{
    Rig rig;
    rig.engine.loadTrack (0, beatTrack (60.0, 220.0, 128.0, 0.1));
    rig.engine.loadTrack (1, beatTrack (60.0, 330.0, 124.0, 0.3));
    rig.engine.play (0);
    test::run (rig.engine, (int) (kSr * 2));

    rig.engine.startAutoMix (4.0);
    test::run (rig.engine, (int) (kSr * 2));   // a mitad de camino

    CHECK (rig.engine.isAutoMixing());
    CHECK (rig.engine.getAutoMixProgress() == Approx (0.5f).margin (0.02f));
    CHECK (rig.engine.getCrossfader() == Approx (0.0f).margin (0.05f));
    CHECK (rig.engine.getDeckStatus (1).playing);   // arrancó solo
    CHECK (rig.engine.getSync (1));                 // y sincronizado (124 -> 128: +3,2 %)
    CHECK (rig.engine.getTempo (1) == Approx ((128.0 / 124.0 - 1.0) * 100.0).margin (0.05));

    test::run (rig.engine, (int) (kSr * 2.2));
    CHECK_FALSE (rig.engine.isAutoMixing());
    CHECK (rig.engine.getCrossfader() == Approx (1.0f));
    CHECK_FALSE (rig.engine.getDeckStatus (0).playing);   // el que salió queda en pausa
    CHECK (rig.engine.getDeckStatus (1).playing);
    CHECK_FALSE (rig.engine.getSync (1));                 // libre para la próxima mezcla
}

TEST_CASE ("AUTO MIX: vuelve del deck 2 al deck 1", "[automix]")
{
    Rig rig;
    rig.engine.setCrossfader (1.0f);
    rig.engine.loadTrack (0, beatTrack (30.0, 220.0, 120.0, 0.0));
    rig.engine.loadTrack (1, beatTrack (30.0, 330.0, 120.0, 0.0));
    rig.engine.play (1);
    test::run (rig.engine, 1024);
    rig.engine.startAutoMix (2.0);
    test::run (rig.engine, (int) (kSr * 2.2));
    CHECK (rig.engine.getCrossfader() == Approx (-1.0f));
    CHECK (rig.engine.getDeckStatus (0).playing);
    CHECK_FALSE (rig.engine.getDeckStatus (1).playing);
}

TEST_CASE ("AUTO MIX: nivel parejo durante todo el pase (sin pozos ni picos)", "[automix]")
{
    Rig rig;
    rig.engine.loadTrack (0, beatTrack (30.0, 300.0, 120.0, 0.0));
    rig.engine.loadTrack (1, beatTrack (30.0, 450.0, 120.0, 0.0));
    rig.engine.play (0);
    test::run (rig.engine, (int) kSr);
    const auto before = test::rms (test::run (rig.engine, (int) kSr).left.data(), (size_t) kSr);

    rig.engine.startAutoMix (8.0);
    for (int i = 0; i < 16; ++i)
    {
        const auto o = test::run (rig.engine, (int) (kSr / 2));
        const auto db = juce::Decibels::gainToDecibels (test::rms (o.left.data(), o.left.size()) / before);
        INFO ("Tramo " << i << ": " << db << " dB");
        CHECK (db > -3.5f);   // potencia constante: sin "pozo" en el medio
        CHECK (db < 2.5f);
    }
}

TEST_CASE ("AUTO MIX: tocar el crossfader cancela la mezcla", "[automix]")
{
    Rig rig;
    rig.engine.loadTrack (0, beatTrack (30.0, 220.0, 120.0, 0.0));
    rig.engine.loadTrack (1, beatTrack (30.0, 330.0, 120.0, 0.0));
    rig.engine.play (0);
    rig.engine.startAutoMix (10.0);
    test::run (rig.engine, (int) kSr);
    REQUIRE (rig.engine.isAutoMixing());

    rig.engine.setCrossfader (-0.3f);
    test::run (rig.engine, 512);
    CHECK_FALSE (rig.engine.isAutoMixing());
    CHECK (rig.engine.getCrossfader() == Approx (-0.3f));
    CHECK (rig.engine.getDeckStatus (0).playing);   // no pausa nada si se cancela
}

TEST_CASE ("AUTO MIX: sin pista en el otro deck no hace nada", "[automix]")
{
    Rig rig;
    rig.engine.loadTrack (0, beatTrack (30.0, 220.0, 120.0, 0.0));
    rig.engine.play (0);
    rig.engine.startAutoMix (4.0);
    test::run (rig.engine, 1024);
    CHECK_FALSE (rig.engine.isAutoMixing());
    CHECK (rig.engine.getCrossfader() == Approx (-1.0f));
}

TEST_CASE ("AUTO MIX: con BPM muy distintos no cambia el tempo", "[automix]")
{
    Rig rig;
    rig.engine.loadTrack (0, beatTrack (30.0, 220.0, 128.0, 0.0));
    rig.engine.loadTrack (1, beatTrack (30.0, 330.0, 95.0, 0.0));   // +35 %: demasiado
    rig.engine.play (0);
    rig.engine.startAutoMix (2.0);
    test::run (rig.engine, (int) kSr);
    CHECK_FALSE (rig.engine.getSync (1));
    CHECK (rig.engine.getTempo (1) == Approx (0.0f));
}

TEST_CASE ("AUTO MIX: origen segun el crossfader", "[automix]")
{
    CHECK (AudioEngine::autoMixSource (-1.0f, false, true, false) == 0);
    CHECK (AudioEngine::autoMixSource (1.0f, false, true, true) == 1);
    CHECK (AudioEngine::autoMixSource (0.0f, false, false, true) == 1);
    CHECK (AudioEngine::autoMixSource (-1.0f, true, true, true) == 1);   // invertido
}

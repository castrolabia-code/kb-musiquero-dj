// Pruebas de la Fase 5: unidades de FX (sincronía al BPM, cola, ruteo, estabilidad).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/AudioEngine.h"
#include "engine/fx/FxUnit.h"

#include <cmath>
#include <vector>

using namespace kb;
using namespace kb::engine;
using Catch::Approx;

namespace
{
    constexpr double kSr = 48000.0;

    struct Signal
    {
        std::vector<float> l, r;
        explicit Signal (size_t n) : l (n, 0.0f), r (n, 0.0f) {}
    };

    /** Pasa la señal por la unidad en bloques de 256. */
    void runUnit (fx::FxUnit& u, Signal& s, double bpm, size_t from = 0, size_t to = 0)
    {
        if (to == 0) to = s.l.size();
        for (size_t p = from; p < to; p += 256)
        {
            const auto n = (int) std::min<size_t> (256, to - p);
            u.process (s.l.data() + p, s.r.data() + p, n, bpm);
        }
    }

    bool finite (const Signal& s)
    {
        for (size_t i = 0; i < s.l.size(); ++i)
            if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i]))
                return false;
        return true;
    }

    float peak (const std::vector<float>& x, size_t from, size_t to)
    {
        float m = 0.0f;
        for (size_t i = from; i < to; ++i) m = std::max (m, std::abs (x[i]));
        return m;
    }
}

TEST_CASE ("FX: Echo sincronizado al BPM (1 beat a 120 = 0.5 s)", "[fx]")
{
    fx::FxUnit u;
    u.prepare (kSr, 256);
    u.setType (fx::Type::echo);
    u.setBeatsIndex (3);   // 1 beat
    u.setWet (1.0f);
    u.setParam (0.3f);
    u.setOn (true);

    Signal s ((size_t) (kSr * 1.6));
    s.l[1000] = s.r[1000] = 1.0f;   // impulso
    runUnit (u, s, 120.0);
    const auto echo1 = 1000 + (size_t) (0.5 * kSr);
    CHECK (peak (s.l, echo1 - 20, echo1 + 20) > 0.2f);              // primer eco en su lugar
    CHECK (peak (s.l, 1200, echo1 - 200) < 0.01f);                   // nada antes
    CHECK (peak (s.l, echo1 + kSr / 2 - 40, echo1 + kSr / 2 + 40) < peak (s.l, echo1 - 20, echo1 + 20));   // se apaga
    CHECK (finite (s));
}

TEST_CASE ("FX: con WET en 0 el audio pasa intacto", "[fx]")
{
    for (int t = 0; t < (int) fx::Type::count; ++t)
    {
        fx::FxUnit u;
        u.prepare (kSr, 256);
        u.setType ((fx::Type) t);
        u.setWet (0.0f);
        u.setOn (true);
        auto sig = test::makeSineTrack (0.5, 440.0, kSr, 0.5f);
        Signal s ((size_t) (kSr * 0.5));
        for (size_t i = 0; i < s.l.size(); ++i) s.l[i] = s.r[i] = sig->getReadPointer (0)[i];
        const auto original = s.l;
        runUnit (u, s, 120.0);
        float maxDiff = 0.0f;
        for (size_t i = 4800; i < s.l.size(); ++i) maxDiff = std::max (maxDiff, std::abs (s.l[i] - original[i]));
        INFO ("Efecto " << t);
        CHECK (maxDiff < 1.0e-4f);
    }
}

TEST_CASE ("FX: todos los efectos son estables (sin NaN, sin explotar)", "[fx]")
{
    juce::Random rng (7);
    for (int t = 0; t < (int) fx::Type::count; ++t)
    {
        for (float param : { 0.0f, 0.5f, 1.0f })
        {
            fx::FxUnit u;
            u.prepare (kSr, 512);
            u.setType ((fx::Type) t);
            u.setWet (1.0f);
            u.setParam (param);
            u.setBeatsIndex (t % 7);
            u.setOn (true);
            Signal s ((size_t) (kSr * 2.0));
            for (size_t i = 0; i < s.l.size(); ++i) { s.l[i] = rng.nextFloat() - 0.5f; s.r[i] = rng.nextFloat() - 0.5f; }
            runUnit (u, s, 128.0);
            INFO ("Efecto " << t << " param " << param);
            CHECK (finite (s));
            CHECK (peak (s.l, 0, s.l.size()) < 4.0f);
        }
    }
}

TEST_CASE ("FX: Gater corta al ritmo (1/2 beat a 120 = 0.25 s)", "[fx]")
{
    fx::FxUnit u;
    u.prepare (kSr, 256);
    u.setType (fx::Type::gater);
    u.setBeatsIndex (2);
    u.setWet (1.0f);
    u.setParam (1.0f);
    u.setOn (true);
    Signal s ((size_t) kSr);
    std::fill (s.l.begin(), s.l.end(), 0.5f);
    std::fill (s.r.begin(), s.r.end(), 0.5f);
    runUnit (u, s, 120.0);
    // Abierto en la primera mitad de cada período de 0.25 s, cerrado en la segunda.
    CHECK (s.l[(size_t) (0.06 * kSr)] > 0.45f);
    CHECK (s.l[(size_t) (0.2 * kSr)] < 0.05f);
    CHECK (s.l[(size_t) (0.31 * kSr)] > 0.45f);
    CHECK (s.l[(size_t) (0.45 * kSr)] < 0.05f);
}

TEST_CASE ("FX: Roll repite el fragmento del encendido", "[fx]")
{
    fx::FxUnit u;
    u.prepare (kSr, 256);
    u.setType (fx::Type::roll);
    u.setBeatsIndex (2);   // 1/2 beat a 120 = 0.25 s
    u.setWet (1.0f);
    u.setOn (true);
    Signal s ((size_t) kSr);
    for (size_t i = 0; i < s.l.size(); ++i) s.l[i] = s.r[i] = (float) i / (float) s.l.size();   // rampa
    runUnit (u, s, 120.0);
    const auto len = (size_t) (0.25 * kSr);
    // Después del primer fragmento, la salida vuelve a empezar (la rampa se repite).
    CHECK (s.l[len + 1000] == Approx (s.l[1000]).margin (0.002));
    CHECK (s.l[2 * len + 3000] == Approx (s.l[3000]).margin (0.002));
}

TEST_CASE ("FX: Brake termina en silencio", "[fx]")
{
    fx::FxUnit u;
    u.prepare (kSr, 256);
    u.setType (fx::Type::brake);
    u.setBeatsIndex (3);   // 0.5 s a 120
    u.setWet (1.0f);
    u.setOn (true);
    auto sig = test::makeSineTrack (1.5, 300.0, kSr, 0.5f);
    Signal s ((size_t) (kSr * 1.5));
    for (size_t i = 0; i < s.l.size(); ++i) s.l[i] = s.r[i] = sig->getReadPointer (0)[i];
    runUnit (u, s, 120.0);
    CHECK (peak (s.l, 0, 4800) > 0.3f);                                         // empieza sonando
    CHECK (peak (s.l, (size_t) (0.7 * kSr), (size_t) (1.5 * kSr)) < 0.001f);   // después: silencio
}

TEST_CASE ("FX: Echo deja sonar la cola al apagarlo y despues se desactiva", "[fx]")
{
    fx::FxUnit u;
    u.prepare (kSr, 256);
    u.setType (fx::Type::echo);
    u.setWet (1.0f);
    u.setParam (0.5f);
    u.setOn (true);
    Signal s ((size_t) (kSr * 12.0));
    for (size_t i = 0; i < 2400; ++i) s.l[i] = s.r[i] = 0.5f * std::sin ((float) i * 0.06f);
    runUnit (u, s, 120.0, 0, 4800);
    u.setOn (false);
    runUnit (u, s, 120.0, 4800, (size_t) (kSr * 2.0));
    CHECK (peak (s.l, (size_t) (0.9 * kSr), (size_t) (1.1 * kSr)) > 0.05f);   // la cola sigue (2do eco)
    runUnit (u, s, 120.0, (size_t) (kSr * 2.0), s.l.size());
    CHECK_FALSE (u.isActive());                                                // terminó y se apagó sola
}

TEST_CASE ("FX: cambiar de efecto encendido no hace clicks", "[fx]")
{
    fx::FxUnit u;
    u.prepare (kSr, 256);
    u.setType (fx::Type::flanger);
    u.setWet (1.0f);
    u.setParam (0.0f);   // bitcrusher en su nivel más suave (si no, sus escalones son el efecto)
    u.setOn (true);
    auto sig = test::makeSineTrack (1.0, 220.0, kSr, 0.4f);
    Signal s ((size_t) kSr);
    for (size_t i = 0; i < s.l.size(); ++i) s.l[i] = s.r[i] = sig->getReadPointer (0)[i];
    runUnit (u, s, 120.0, 0, 24000);
    u.setType (fx::Type::bitcrusher);
    runUnit (u, s, 120.0, 24000, s.l.size());
    CHECK (u.getType() == fx::Type::bitcrusher);
    CHECK (test::maxStep (std::vector<float> (s.l.begin() + 23000, s.l.begin() + 26000)) < 0.08f);
}

TEST_CASE ("FX: ruteo en el motor (deck 1, deck 2 o master)", "[fx][engine]")
{
    AudioEngine engine;
    test::prepareUnity (engine, kSr, 256);
    engine.setCrossfader (0.0f);
    auto a = test::makeSineTrack (10.0, 440.0, kSr, 0.4f);
    a->setBeatInfo (120.0, 0.0);
    engine.loadTrack (0, a);
    engine.play (0);
    test::run (engine, 4800);
    const auto dry = test::run (engine, 9600);

    auto& u = engine.getFxUnit (0);
    u.setType (fx::Type::gater);
    u.setParam (1.0f);
    u.setWet (1.0f);
    u.setBeatsIndex (2);

    u.setTarget (fx::Target::deck2);   // en el otro deck: no cambia nada
    u.setOn (true);
    const auto other = test::run (engine, 9600);
    CHECK (test::rms (other.left.data(), 9600) == Approx (test::rms (dry.left.data(), 9600)).epsilon (0.05));

    u.setTarget (fx::Target::deck1);   // en el deck que suena: corta la mitad del tiempo
    test::run (engine, 4800);
    const auto gated = test::run (engine, 24000);
    CHECK (test::rms (gated.left.data(), 24000) < 0.8f * test::rms (dry.left.data(), 9600));

    CHECK (engine.getDeckBpm (0) == Approx (120.0));
}

TEST_CASE ("FX: ningun efecto reserva memoria en el hilo de audio", "[fx][realtime]")
{
    AudioEngine engine;
    test::prepareUnity (engine, kSr, 256);
    auto a = test::makeSineTrack (10.0, 440.0, kSr, 0.4f);
    a->setBeatInfo (126.0, 0.0);
    engine.loadTrack (0, a);
    engine.play (0);
    test::run (engine, 2048);

    std::vector<float> l (256), r (256);
    float* outs[] = { l.data(), r.data() };
    int allocations = 0;
    for (int t = 0; t < (int) fx::Type::count; ++t)
    {
        for (auto target : { fx::Target::deck1, fx::Target::master })
        {
            auto& u = engine.getFxUnit (t % 2);
            u.setType ((fx::Type) t);
            u.setTarget (target);
            u.setOn (true);
            test::AllocationGuard guard;
            for (int b = 0; b < 30; ++b)
                engine.process (outs, 2, 256);
            u.setOn (false);
            for (int b = 0; b < 10; ++b)
                engine.process (outs, 2, 256);
            allocations += guard.count();
        }
    }
    CHECK (allocations == 0);
}

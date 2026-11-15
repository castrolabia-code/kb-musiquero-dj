// Pruebas de piezas de la app que no necesitan ventana: cache de análisis.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "app/AnalysisCache.h"

using kb::app::AnalysisCache;
using kb::app::AnalysisEntry;
using Catch::Approx;

TEST_CASE ("AnalysisCache: guarda, persiste y se invalida si el archivo cambia", "[app][cache]")
{
    const auto dir = juce::File::createTempFile ("kbcache");
    dir.createDirectory();
    const auto store = dir.getChildFile ("analysis.json");
    const auto song = dir.getChildFile ("tema.mp3");
    song.replaceWithText ("contenido");

    {
        AnalysisCache cache (store);
        CHECK_FALSE (cache.find (song).has_value());

        AnalysisEntry e;
        e.bpm = 124.5;
        e.firstBeatSeconds = 0.31;
        e.durationSeconds = 301.0;
        e.formatName = "MP3";
        cache.store (song, e);
        REQUIRE (cache.find (song).has_value());
        CHECK (cache.find (song)->bpm == Approx (124.5));
    }   // se guarda al destruirse

    {
        AnalysisCache reloaded (store);
        const auto e = reloaded.find (song);
        REQUIRE (e.has_value());
        CHECK (e->bpm == Approx (124.5));
        CHECK (e->firstBeatSeconds == Approx (0.31));
        CHECK (e->formatName == "MP3");

        // El archivo cambió: el análisis viejo ya no vale.
        song.replaceWithText ("otro contenido, otro tamaño");
        CHECK_FALSE (reloaded.find (song).has_value());
    }

    dir.deleteRecursively();
}

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
    const auto store = dir.getChildFile ("library.db");
    const auto song = dir.getChildFile ("tema.mp3");
    song.replaceWithText ("contenido");

    {
        kb::library::LibraryDatabase db (store);
        AnalysisCache cache (db);
        CHECK_FALSE (cache.find (song).has_value());

        AnalysisEntry e;
        e.bpm = 124.5;
        e.firstBeatSeconds = 0.31;
        e.durationSeconds = 301.0;
        e.formatName = "MP3";
        cache.store (song, e);
        REQUIRE (cache.find (song).has_value());
        CHECK (cache.find (song)->bpm == Approx (124.5));
    }   // queda guardado en la base

    {
        kb::library::LibraryDatabase db (store);
        AnalysisCache reloaded (db);
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

TEST_CASE ("AnalysisCache: importa la lista y los hot cues de las versiones anteriores (JSON)", "[app][cache]")
{
    const auto dir = juce::File::createTempFile ("kbmig");
    dir.createDirectory();
    const auto song = dir.getChildFile ("tema.mp3");
    song.replaceWithText ("contenido");
    const auto analysis = dir.getChildFile ("analysis.json");
    const auto library = dir.getChildFile ("library.json");
    analysis.replaceWithText (juce::String (R"({"version":2,"entries":[{"path":"PATH","bpm":128.0,"firstBeat":0.2,"duration":10.0,"format":"MP3","key":5,"size":SIZE,"modified":MOD}],
                                              "hotCues":[{"path":"PATH","cues":[1.5,-1,-1,-1,-1,-1,-1,7.25]}]})")
                                 .replace ("PATH", song.getFullPathName().replace ("\\", "\\\\"))
                                 .replace ("SIZE", juce::String (song.getSize()))
                                 .replace ("MOD", juce::String (song.getLastModificationTime().toMilliseconds())));
    library.replaceWithText (juce::String (R"({"version":1,"files":["PATH"]})").replace ("PATH", song.getFullPathName().replace ("\\", "\\\\")));

    kb::library::LibraryDatabase db (dir.getChildFile ("library.db"));
    CHECK (AnalysisCache::importLegacy (db, analysis, library) == 1);
    AnalysisCache cache (db);
    REQUIRE (cache.find (song).has_value());
    CHECK (cache.find (song)->bpm == Approx (128.0));
    CHECK (cache.find (song)->key == 5);
    REQUIRE (cache.findHotCues (song).has_value());
    CHECK ((*cache.findHotCues (song))[7] == Approx (7.25));
    CHECK (db.collectionSize() == 1);
    CHECK_FALSE (analysis.existsAsFile());   // renombrado (.migrado)
    dir.deleteRecursively();
}

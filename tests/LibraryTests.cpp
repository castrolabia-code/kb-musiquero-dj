// Pruebas de la Fase 6: tags y carátulas (TagLib).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "library/ArtworkCache.h"
#include "library/TagReader.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <mutex>

using namespace kb;
using namespace kb::library;
using Catch::Approx;

namespace
{
    juce::File fixture (const char* name) { return juce::File (KB_SOURCE_DIR).getChildFile ("tests/fixtures").getChildFile (name); }

    juce::MemoryBlock makePng (juce::Colour c, int w, int h)
    {
        juce::Image img (juce::Image::RGB, w, h, true, juce::SoftwareImageType());
        juce::Graphics g (img);
        g.fillAll (c);
        juce::MemoryOutputStream out;
        juce::PNGImageFormat().writeImageToStream (img, out);
        return out.getMemoryBlock();
    }

    struct TempDir
    {
        TempDir() { dir.createDirectory(); }
        ~TempDir() { dir.deleteRecursively(); }
        juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("kb_lib_test", "");
    };
}

TEST_CASE ("Tags: escribir y leer titulo, artista, genero, BPM, tonalidad y caratula (MP3 y M4A)", "[library][tags]")
{
    for (const auto* name : { "sine440_2s_44k.mp3", "sine440_2s_44k.m4a" })
    {
        TempDir tmp;
        const auto file = tmp.dir.getChildFile (name);
        REQUIRE (fixture (name).copyFileTo (file));

        TrackTags in;
        in.title = juce::String::fromUTF8 ("Canción de Prueba");
        in.artist = "Castro Labs";
        in.genre = "Cumbia";
        in.bpm = 98.0;
        in.keyText = "8A";
        REQUIRE (TagReader::writeForTest (file, in, makePng (juce::Colours::orange, 600, 400)));

        INFO (name);
        const auto t = TagReader::read (file);
        CHECK (t.valid);
        CHECK (t.title == in.title);
        CHECK (t.artist == "Castro Labs");
        CHECK (t.genre == "Cumbia");
        CHECK (t.bpm == Approx (98.0));
        if (juce::String (name).endsWith ("mp3"))
            CHECK (t.keyText == "8A");
        CHECK (t.durationSeconds == Approx (2.0).margin (0.1));

        // Carátula: cuadrada (recortada al centro) y achicada.
        const auto art = TagReader::readArtwork (file, 256);
        REQUIRE (art.isValid());
        CHECK (art.getWidth() == 256);
        CHECK (art.getHeight() == 256);
        CHECK (art.getPixelAt (128, 128).getRed() > 200);
    }
}

TEST_CASE ("Tags: sin tags usa el nombre del archivo; sin caratula, la de la carpeta", "[library][tags]")
{
    TempDir tmp;
    const auto file = tmp.dir.getChildFile ("Mi Tema.mp3");
    REQUIRE (fixture ("sine440_2s_44k.mp3").copyFileTo (file));
    const auto t = TagReader::read (file);
    CHECK (t.valid);
    CHECK (t.title == "Mi Tema");
    CHECK_FALSE (TagReader::readArtwork (file, 128).isValid());

    const auto png = makePng (juce::Colours::blue, 300, 300);
    REQUIRE (tmp.dir.getChildFile ("Folder.PNG").replaceWithData (png.getData(), png.getSize()));
    const auto art = TagReader::readArtwork (file, 128);
    REQUIRE (art.isValid());
    CHECK (art.getPixelAt (64, 64).getBlue() > 200);
}

TEST_CASE ("Tags: un archivo que no es audio no rompe nada", "[library][tags]")
{
    TempDir tmp;
    const auto file = tmp.dir.getChildFile ("roto.mp3");
    REQUIRE (file.replaceWithText ("esto no es un mp3"));
    CHECK_NOTHROW (TagReader::read (file));
    CHECK (TagReader::readEmbeddedArtwork (file).getSize() == 0);
}

namespace
{
    /** Cola de "hilo de mensajes" manual para la prueba. */
    struct ManualQueue
    {
        std::mutex lock;
        std::vector<std::function<void()>> items;
        void post (std::function<void()> f) { std::scoped_lock l (lock); items.push_back (std::move (f)); }
        void drain()
        {
            std::vector<std::function<void()>> now;
            { std::scoped_lock l (lock); now.swap (items); }
            for (auto& f : now) f();
        }
    };
}

TEST_CASE ("Caratulas: el cache las guarda en disco y avisa en el hilo de mensajes", "[library][artwork]")
{
    ManualQueue queue;
    TempDir tmp;
    const auto file = tmp.dir.getChildFile ("con_tapa.mp3");
    REQUIRE (fixture ("sine440_2s_44k.mp3").copyFileTo (file));
    TrackTags in;
    in.title = "Tapa";
    REQUIRE (TagReader::writeForTest (file, in, makePng (juce::Colours::green, 800, 800)));

    const auto cacheDir = tmp.dir.getChildFile ("artwork");
    {
        ArtworkCache cache (cacheDir);
        cache.postToMessageThread = [&queue] (std::function<void()> f) { queue.post (std::move (f)); };
        juce::Image got;
        bool called = false;
        cache.request (file, [&] (const juce::Image& img) { got = img; called = true; });
        for (int i = 0; i < 200 && ! called; ++i)
        { queue.drain(); juce::Thread::sleep (10); }
        REQUIRE (called);
        CHECK (got.getWidth() == ArtworkCache::kSize);
        CHECK (cacheDir.getChildFile (ArtworkCache::keyFor (file) + ".jpg").existsAsFile());
        CHECK (cache.getThumbnail (file, 40).getWidth() == 40);
    }
    {
        // Otra sesión: sale del disco.
        ArtworkCache cache (cacheDir);
        cache.postToMessageThread = [&queue] (std::function<void()> f) { queue.post (std::move (f)); };
        bool called = false;
        juce::Image got;
        cache.request (file, [&] (const juce::Image& img) { got = img; called = true; });
        for (int i = 0; i < 200 && ! called; ++i)
        { queue.drain(); juce::Thread::sleep (10); }
        CHECK (got.isValid());
    }
}

#include "library/LibraryDatabase.h"

TEST_CASE ("Base de datos: pistas, analisis, rating, hot cues y persistencia", "[library][db]")
{
    TempDir tmp;
    const auto dbFile = tmp.dir.getChildFile ("library.db");
    const juce::File song ("/musica/Castro Labs - Tema.mp3");
    juce::int64 id = 0;
    {
        LibraryDatabase db (dbFile);
        REQUIRE (db.isOpen());
        id = db.ensureTrack (song, true);
        CHECK (id > 0);
        CHECK (db.ensureTrack (song, true) == id);   // no se duplica
        db.setTags (id, "Tema", "Castro Labs", "Album", "Cumbia", 2024, 96.0, 180.0);
        db.setAnalysis (id, 95.5, 0.12, 7, 181.0, "MP3", 1234, 5678);
        db.setRating (id, 4);
        HotCues cues;
        cues.fill (-1.0);
        cues[0] = 10.0; cues[5] = 42.5;
        db.setHotCues (id, cues);
        CHECK (db.collectionSize() == 1);
    }
    {
        LibraryDatabase db (dbFile);   // reabrir: todo quedó guardado
        const auto t = db.findByPath (song);
        REQUIRE (t.has_value());
        CHECK (t->title == "Tema");
        CHECK (t->genre == "Cumbia");
        CHECK (t->bpm == Approx (95.5));       // el análisis manda sobre el BPM del tag
        CHECK (t->key == 7);
        CHECK (t->rating == 4);
        CHECK (t->analysed);
        const auto cues = db.hotCues (id);
        CHECK (cues[0] == Approx (10.0));
        CHECK (cues[5] == Approx (42.5));
        CHECK (cues[1] < 0.0);
        // El BPM del tag no pisa al analizado.
        db.setTags (id, "Tema", "Castro Labs", "Album", "Cumbia", 2024, 120.0, 180.0);
        CHECK (db.findById (id)->bpm == Approx (95.5));
    }
}

TEST_CASE ("Base de datos: playlists, preparacion e historial", "[library][db]")
{
    TempDir tmp;
    LibraryDatabase db (tmp.dir.getChildFile ("library.db"));
    std::vector<juce::int64> ids;
    for (int i = 0; i < 5; ++i)
        ids.push_back (db.ensureTrack (juce::File ("/m/" + juce::String (i) + ".mp3"), true));

    const auto p = db.createPlaylist ("Fiesta");
    db.addToPlaylist (p, { ids[3], ids[1], ids[4] });
    CHECK (db.playlistTracks (p) == std::vector<juce::int64> { ids[3], ids[1], ids[4] });
    db.removeFromPlaylist (p, { 1 });
    CHECK (db.playlistTracks (p) == std::vector<juce::int64> { ids[3], ids[4] });
    db.renamePlaylist (p, "Fiesta 2");

    const auto prep = db.preparationPlaylist ("Preparación");
    CHECK (db.preparationPlaylist ("x") == prep);   // una sola
    const auto lists = db.playlists();
    REQUIRE (lists.size() == 2);
    CHECK (lists[0].kind == Playlist::preparation);   // primero la de preparación
    CHECK (lists[1].name == "Fiesta 2");
    CHECK (lists[1].count == 2);

    const auto s1 = db.newSession();
    db.markPlayed (ids[2], s1);
    db.markPlayed (ids[0], s1);
    CHECK (db.historyTracks (s1) == std::vector<juce::int64> { ids[2], ids[0] });
    CHECK (db.findById (ids[2])->playCount == 1);
    CHECK (db.newSession() > s1);

    db.setInCollection ({ ids[0] }, false);
    CHECK (db.collectionSize() == 4);
    db.deletePlaylist (p);
    CHECK (db.playlists().size() == 1);
    db.clearCollection();
    CHECK (db.collectionSize() == 0);
    CHECK (db.findById (ids[0]).has_value());   // la pista sigue (análisis, cues, historial)
}

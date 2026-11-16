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
        juce::Image img (juce::Image::RGB, w, h, true);
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

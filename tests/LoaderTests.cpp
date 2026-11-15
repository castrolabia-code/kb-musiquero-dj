// Pruebas de carga de pistas: formatos, conversión de sample rate, metadatos
// del nombre de archivo, cancelación y carga progresiva.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/AudioFileFormats.h"
#include "engine/TrackLoader.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <chrono>
#include <thread>

using namespace kb::engine;
using Catch::Approx;

namespace
{
    juce::File fixturesDir()
    {
        return juce::File (KB_SOURCE_DIR).getChildFile ("tests").getChildFile ("fixtures");
    }

    /** Escribe un seno estéreo con un formato de JUCE. Devuelve el archivo. */
    juce::File writeSine (juce::AudioFormat& format, const juce::File& file, double sr,
                          double seconds, double freq, float amp, int bits, int quality = 0)
    {
        file.deleteFile();
        const auto frames = (int) std::lround (seconds * sr);
        juce::AudioBuffer<float> buf (2, frames);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < frames; ++i)
                buf.setSample (c, i, amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr));

        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
        auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                          .withSampleRate (sr)
                                                          .withNumChannels (2)
                                                          .withBitsPerSample (bits)
                                                          .withQualityOptionIndex (quality));
        REQUIRE (writer != nullptr);
        REQUIRE (writer->writeFromAudioSampleBuffer (buf, 0, frames));
        return file;
    }

    /** Frecuencia estimada por cruces por cero ascendentes. */
    double estimateFrequency (const float* x, std::int64_t n, double sr)
    {
        std::int64_t first = -1, last = -1, crossings = 0;
        for (std::int64_t i = 1; i < n; ++i)
        {
            if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            {
                if (first < 0) first = i;
                last = i;
                ++crossings;
            }
        }
        return crossings > 1 ? (double) (crossings - 1) * sr / (double) (last - first) : 0.0;
    }

    struct TempDir
    {
        TempDir()  { dir.createDirectory(); }
        ~TempDir() { dir.deleteRecursively(); }
        juce::File dir = juce::File::createTempFile ("kbtest");
    };

    void checkSineTrack (const TrackBuffer& t, double expectedSeconds, double freq, float amp,
                         double lengthTolerance, double ampToleranceDb)
    {
        CHECK (t.isComplete());
        CHECK (t.getLengthSeconds() == Approx (expectedSeconds).margin (lengthTolerance));

        // Se mide en el medio de la pista (lejos de bordes y retardos de códec).
        const auto len   = t.getLengthFrames();
        const auto start = len / 4;
        const auto n     = len / 2;
        for (int c = 0; c < 2; ++c)
        {
            const auto* d = t.getReadPointer (c) + start;
            CHECK (estimateFrequency (d, n, t.getSampleRate()) == Approx (freq).margin (freq * 0.002));
            const auto measuredDb = juce::Decibels::gainToDecibels (kb::test::rms (d, (size_t) n) * std::sqrt (2.0f));
            CHECK (measuredDb == Approx (juce::Decibels::gainToDecibels (amp)).margin (ampToleranceDb));
        }
    }
}

// =============================================================================
TEST_CASE ("Formatos: lista de extensiones soportadas", "[loader]")
{
    const auto exts = formats::supportedExtensions();
    for (auto e : { "mp3", "wav", "aif", "aiff", "flac", "ogg" })
        CHECK (exts.contains (e));
   #if JUCE_WINDOWS
    CHECK (exts.contains ("m4a"));
   #endif
    CHECK (formats::isSupportedFile (juce::File::getCurrentWorkingDirectory().getChildFile ("Tema.MP3")));
    CHECK_FALSE (formats::isSupportedFile (juce::File::getCurrentWorkingDirectory().getChildFile ("notas.txt")));
}

TEST_CASE ("Loader: decodifica WAV, AIFF, FLAC y OGG", "[loader]")
{
    TempDir tmp;
    juce::WavAudioFormat wav;
    juce::AiffAudioFormat aiff;
    juce::FlacAudioFormat flac;
    juce::OggVorbisAudioFormat ogg;

    struct Case { juce::AudioFormat* format; const char* name; int bits; int quality; double ampTolDb; };
    const Case cases[] = {
        { &wav,  "a.wav",  24, 0, 0.01 },
        { &aiff, "a.aiff", 16, 0, 0.01 },
        { &flac, "a.flac", 24, 0, 0.01 },
        { &ogg,  "a.ogg",  16, 8, 0.3 },   // con pérdida
    };

    for (const auto& c : cases)
    {
        DYNAMIC_SECTION (c.name)
        {
            const auto f = writeSine (*c.format, tmp.dir.getChildFile (c.name), 48000.0, 1.5, 1000.0, 0.5f, c.bits, c.quality);
            const auto result = TrackLoader::loadBlocking (f, 48000.0);
            REQUIRE (result.error == LoadError::none);
            REQUIRE (result.track != nullptr);
            checkSineTrack (*result.track, 1.5, 1000.0, 0.5f, 0.01, c.ampTolDb);
            CHECK (result.track->getInfo().sourceSampleRate == Approx (48000.0));
        }
    }
}

TEST_CASE ("Loader: decodifica MP3", "[loader]")
{
    const auto f = fixturesDir().getChildFile ("sine440_2s_44k.mp3");
    REQUIRE (f.existsAsFile());
    const auto result = TrackLoader::loadBlocking (f, 44100.0);
    REQUIRE (result.error == LoadError::none);
    // El MP3 agrega retardo y relleno del codificador (~50 ms), y LAME lo deja
    // ~0.45 dB más bajo (ffmpeg mide lo mismo sobre este archivo).
    checkSineTrack (*result.track, 2.0, 440.0, 0.5f, 0.06, 0.6);
}

TEST_CASE ("Loader: decodifica M4A/AAC (Media Foundation en Windows)", "[loader]")
{
    const auto f = fixturesDir().getChildFile ("sine440_2s_44k.m4a");
    REQUIRE (f.existsAsFile());

    if (! formats::isSupportedFile (f))
    {
        SUCCEED ("M4A no se soporta en esta plataforma (solo Windows/macOS)");
        return;
    }

    const auto result = TrackLoader::loadBlocking (f, 48000.0);
    REQUIRE (result.error == LoadError::none);
    checkSineTrack (*result.track, 2.0, 440.0, 0.5f, 0.06, 0.3);
}

TEST_CASE ("Loader: convierte el sample rate con calidad (r8brain)", "[loader][resampler]")
{
    TempDir tmp;
    juce::WavAudioFormat wav;

    SECTION ("44.1 kHz -> 48 kHz")
    {
        const auto f = writeSine (wav, tmp.dir.getChildFile ("up.wav"), 44100.0, 2.0, 1000.0, 0.5f, 24);
        const auto r = TrackLoader::loadBlocking (f, 48000.0);
        REQUIRE (r.error == LoadError::none);
        CHECK (r.track->getSampleRate() == Approx (48000.0));
        CHECK (r.track->getLengthFrames() == 96000);
        checkSineTrack (*r.track, 2.0, 1000.0, 0.5f, 1e-4, 0.01);

        // Sin retardo: la señal arranca en fase (el seno empieza en 0 y sube).
        const auto* d = r.track->getReadPointer (0);
        const auto expected = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * 4800 / 48000.0);
        CHECK (d[4800] == Approx (expected).margin (0.002));
    }

    SECTION ("96 kHz -> 44.1 kHz: elimina lo que no entra (anti-aliasing)")
    {
        // 30 kHz no existe a 44.1 kHz. Sin filtro aparecería como alias en 14.1 kHz.
        const auto f = writeSine (wav, tmp.dir.getChildFile ("down.wav"), 96000.0, 1.0, 30000.0, 0.5f, 24);
        const auto r = TrackLoader::loadBlocking (f, 44100.0);
        REQUIRE (r.error == LoadError::none);
        CHECK (r.track->getLengthFrames() == 44100);
        const auto residual = kb::test::rms (r.track->getReadPointer (0) + 4410, 30000);
        INFO ("Residuo del alias (RMS): " << residual);
        CHECK (residual < 1.0e-5f);   // menos de -100 dBFS
    }

    SECTION ("mono -> estéreo")
    {
        const auto file = tmp.dir.getChildFile ("mono.wav");
        juce::AudioBuffer<float> buf (1, 48000);
        for (int i = 0; i < 48000; ++i)
            buf.setSample (0, i, 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 500.0 * i / 48000.0));
        {
            std::unique_ptr<juce::OutputStream> s = std::make_unique<juce::FileOutputStream> (file);
            auto w = wav.createWriterFor (s, juce::AudioFormatWriterOptions{}.withSampleRate (48000.0).withNumChannels (1).withBitsPerSample (24));
            REQUIRE (w != nullptr);
            w->writeFromAudioSampleBuffer (buf, 0, 48000);
        }
        const auto r = TrackLoader::loadBlocking (file, 48000.0);
        REQUIRE (r.error == LoadError::none);
        CHECK (r.track->getInfo().sourceChannels == 1);
        CHECK (kb::test::rms (r.track->getReadPointer (1), 48000) == Approx (kb::test::rms (r.track->getReadPointer (0), 48000)));
        CHECK (kb::test::rms (r.track->getReadPointer (1), 48000) > 0.15f);
    }
}

TEST_CASE ("Loader: errores claros", "[loader]")
{
    TempDir tmp;
    CHECK (TrackLoader::loadBlocking (tmp.dir.getChildFile ("no-existe.mp3"), 48000.0).error == LoadError::fileNotFound);

    const auto txt = tmp.dir.getChildFile ("notas.txt");
    txt.replaceWithText ("hola");
    CHECK (TrackLoader::loadBlocking (txt, 48000.0).error == LoadError::unsupportedFormat);

    const auto fake = tmp.dir.getChildFile ("roto.flac");
    fake.replaceWithText ("esto no es un flac");
    CHECK (TrackLoader::loadBlocking (fake, 48000.0).error == LoadError::unreadable);
}

TEST_CASE ("Loader: artista y titulo desde el nombre de archivo", "[loader]")
{
    juce::String artist, title;
    TrackLoader::parseFileName (juce::File::getCurrentWorkingDirectory().getChildFile ("Los Palmeras - Bombon Asesino.mp3"), artist, title);
    CHECK (artist == "Los Palmeras");
    CHECK (title == "Bombon Asesino");

    TrackLoader::parseFileName (juce::File::getCurrentWorkingDirectory().getChildFile ("track01.wav"), artist, title);
    CHECK (artist.isEmpty());
    CHECK (title == "track01");
}

TEST_CASE ("Loader: carga asincronica, progresiva y cancelable", "[loader]")
{
    TempDir tmp;
    juce::FlacAudioFormat flac;
    const auto f = writeSine (flac, tmp.dir.getChildFile ("long.flac"), 44100.0, 20.0, 440.0, 0.5f, 16);

    TrackLoader loader (2);

    SECTION ("la pista llega antes de terminar de decodificar y luego se completa")
    {
        std::shared_ptr<TrackBuffer> received;
        std::atomic<bool> ready { false };
        loader.loadAsync (0, f, 48000.0,
                          [&] (std::shared_ptr<TrackBuffer> t) { received = t; ready = true; },
                          [] (LoadError) { FAIL ("no debería fallar"); });

        for (int i = 0; i < 500 && ! ready; ++i)
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        REQUIRE (ready);
        REQUIRE (received != nullptr);
        CHECK (received->getLengthSeconds() == Approx (20.0).margin (0.001));

        for (int i = 0; i < 1000 && ! received->isComplete(); ++i)
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        CHECK (received->isComplete());
        CHECK (received->getOverviewBinsReady() == TrackBuffer::kOverviewBins);
        CHECK (received->getOverviewPeak (TrackBuffer::kOverviewBins / 2) == Approx (0.5f).margin (0.01f));
    }

    SECTION ("una carga nueva en el mismo slot cancela la anterior")
    {
        std::shared_ptr<TrackBuffer> first, second;
        loader.loadAsync (0, f, 48000.0, [&] (auto t) { first = t; }, {});
        loader.loadAsync (0, f, 48000.0, [&] (auto t) { second = t; }, {});

        for (int i = 0; i < 1000 && (second == nullptr || ! second->isComplete()); ++i)
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        REQUIRE (second != nullptr);
        CHECK (second->isComplete());
        if (first != nullptr)
            CHECK_FALSE (first->isComplete());   // quedó a medias: se canceló
    }
}

TEST_CASE ("Deck + Loader: se puede reproducir mientras la pista sigue cargando", "[loader][deck]")
{
    // Pista que "carga" a mano: solo el primer segundo está decodificado.
    TrackInfo info;
    auto t = std::make_shared<TrackBuffer> (info, 48000.0, 48000 * 4);
    t->setEstimatedLength (48000 * 4);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 48000; ++i)
            t->getWritePointer (c)[i] = 0.5f;
    t->publishDecoded (48000);

    AudioEngine engine;
    kb::test::prepareUnity (engine);
    engine.loadTrack (0, t);
    engine.play (0);

    kb::test::run (engine, 96000);   // 2 s: el deck tiene que esperar en el borde decodificado
    auto st = engine.getDeckStatus (0);
    CHECK (st.playing);
    CHECK (st.positionSeconds < 1.0);
    CHECK (st.positionSeconds > 0.99);

    // Llega el resto: sigue avanzando.
    for (int c = 0; c < 2; ++c)
        for (int i = 48000; i < 48000 * 4; ++i)
            t->getWritePointer (c)[i] = 0.5f;
    t->markComplete (48000 * 4);
    kb::test::run (engine, 48000);
    CHECK (engine.getDeckStatus (0).positionSeconds > 1.9);
}

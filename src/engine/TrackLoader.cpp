#include "engine/TrackLoader.h"

#include "engine/AudioFileFormats.h"

#include <CDSPResampler.h>

#include <cmath>
#include <vector>

namespace kb::engine
{
    // =========================================================================
    //  Job del pool
    // =========================================================================
    class TrackLoader::Job final : public juce::ThreadPoolJob
    {
    public:
        Job (juce::File f, double sr, std::shared_ptr<std::atomic<bool>> cancel,
             ReadyCallback ready, ErrorCallback error)
            : juce::ThreadPoolJob ("KB track load"),
              file (std::move (f)), targetSampleRate (sr), cancelFlag (std::move (cancel)),
              onReady (std::move (ready)), onError (std::move (error))
        {
        }

        JobStatus runJob() override
        {
            std::shared_ptr<TrackBuffer> track;
            const auto err = decode (file, targetSampleRate, *cancelFlag, onReady, track);

            if (err != LoadError::none && err != LoadError::cancelled && onError)
                onError (err);

            return jobHasFinished;
        }

    private:
        juce::File file;
        double targetSampleRate;
        std::shared_ptr<std::atomic<bool>> cancelFlag;
        ReadyCallback onReady;
        ErrorCallback onError;
    };

    // =========================================================================
    TrackLoader::TrackLoader (int numSlots)
        : pool (juce::ThreadPoolOptions{}
                    .withThreadName ("KB Loader")
                    .withNumberOfThreads (juce::jmax (1, numSlots)))
    {
        for (int i = 0; i < numSlots; ++i)
            slotCancelFlags.push_back (std::make_shared<std::atomic<bool>> (false));
    }

    TrackLoader::~TrackLoader()
    {
        for (auto& f : slotCancelFlags)
            f->store (true);
        pool.removeAllJobs (true, 10000);
    }

    void TrackLoader::loadAsync (int slot, const juce::File& file, double targetSampleRate,
                                 ReadyCallback onReady, ErrorCallback onError)
    {
        jassert (juce::isPositiveAndBelow (slot, (int) slotCancelFlags.size()));
        if (! juce::isPositiveAndBelow (slot, (int) slotCancelFlags.size()))
            return;

        // Cancela la carga previa del slot y crea una bandera nueva para esta.
        cancel (slot);
        auto flag = std::make_shared<std::atomic<bool>> (false);
        slotCancelFlags[(size_t) slot] = flag;

        pool.addJob (new Job (file, targetSampleRate, flag, std::move (onReady), std::move (onError)), true);
    }

    void TrackLoader::cancel (int slot)
    {
        if (juce::isPositiveAndBelow (slot, (int) slotCancelFlags.size()))
            slotCancelFlags[(size_t) slot]->store (true);
    }

    LoadResult TrackLoader::loadBlocking (const juce::File& file, double targetSampleRate)
    {
        std::atomic<bool> neverCancel { false };
        LoadResult result;
        result.error = decode (file, targetSampleRate, neverCancel, {}, result.track);
        if (result.error != LoadError::none)
            result.track.reset();
        return result;
    }

    void TrackLoader::parseFileName (const juce::File& file, juce::String& artist, juce::String& title)
    {
        const auto name = file.getFileNameWithoutExtension().trim();
        const auto sep  = name.indexOf (" - ");
        if (sep > 0)
        {
            artist = name.substring (0, sep).trim();
            title  = name.substring (sep + 3).trim();
        }
        else
        {
            artist = {};
            title  = name;
        }
        // Fase 6: estos datos se reemplazan por los tags leídos con TagLib.
    }

    // =========================================================================
    //  Decodificación + conversión de sample rate
    // =========================================================================
    LoadError TrackLoader::decode (const juce::File& file, double targetSampleRate,
                                   const std::atomic<bool>& cancelFlag,
                                   const ReadyCallback& onReady,
                                   std::shared_ptr<TrackBuffer>& outTrack)
    {
        jassert (targetSampleRate > 0.0);

        formats::initialiseDecodingThread();
        struct ThreadGuard { ~ThreadGuard() { formats::shutdownDecodingThread(); } } threadGuard;

        if (! file.existsAsFile())
            return LoadError::fileNotFound;
        if (! formats::isSupportedFile (file))
            return LoadError::unsupportedFormat;

        auto reader = formats::createReader (file);
        if (reader == nullptr)
            return LoadError::unreadable;

        const auto srcRate     = reader->sampleRate;
        const auto srcLength   = (std::int64_t) reader->lengthInSamples;
        const auto srcChannels = (int) reader->numChannels;
        if (srcRate <= 0.0 || srcLength <= 0 || srcChannels <= 0)
            return LoadError::empty;

        const auto ratio        = targetSampleRate / srcRate;
        const bool resample     = std::abs (ratio - 1.0) > 1.0e-9;
        // (el epsilon evita que un error de redondeo agregue un frame: 88200 * 48000/44100)
        const auto expectedOut  = (std::int64_t) std::ceil ((double) srcLength * ratio - 1.0e-6);
        // Margen de 1 s por si el largo informado por el decodificador es estimado (AAC).
        const auto capacity     = expectedOut + (std::int64_t) targetSampleRate;

        if (capacity * TrackBuffer::kNumChannels * (std::int64_t) sizeof (float) > kMaxBytesPerTrack)
            return LoadError::tooLong;

        TrackInfo info;
        info.file             = file;
        info.formatName       = reader->getFormatName();
        info.sourceSampleRate = srcRate;
        info.sourceChannels   = srcChannels;
        parseFileName (file, info.artist, info.title);

        std::shared_ptr<TrackBuffer> track;
        try
        {
            track = std::make_shared<TrackBuffer> (std::move (info), targetSampleRate, capacity);
        }
        catch (const std::bad_alloc&)
        {
            return LoadError::outOfMemory;
        }

        track->setEstimatedLength (expectedOut);
        outTrack = track;

        // Ya se puede reproducir lo que se vaya decodificando.
        if (onReady)
            onReady (track);

        auto isCancelled = [&] { return cancelFlag.load (std::memory_order_relaxed) || track->isLoadingCancelled(); };

        // Bloques chicos al principio para que el PLAY responda enseguida.
        constexpr int kFirstChunk = 4096;
        constexpr int kChunk      = 32768;
        constexpr int kNumCh      = TrackBuffer::kNumChannels;

        juce::AudioBuffer<float> in (kNumCh, kChunk);
        std::vector<double> inD ((size_t) kChunk);

        std::unique_ptr<r8b::CDSPResampler24> resamplers[kNumCh];
        if (resample)
            for (auto& r : resamplers)
                r = std::make_unique<r8b::CDSPResampler24> (srcRate, targetSampleRate, kChunk);

        std::int64_t srcPos = 0;
        std::int64_t outPos = 0;
        int flushBlocks = 0;

        while (srcPos < srcLength || (resample && outPos < expectedOut))
        {
            if (isCancelled())
                return LoadError::cancelled;

            const auto chunk = srcPos == 0 ? kFirstChunk : kChunk;

            // --- Leer del archivo (o ceros para vaciar el resampler al final) ---
            int n = chunk;
            if (srcPos < srcLength)
            {
                n = (int) std::min<std::int64_t> (chunk, srcLength - srcPos);
                const auto chToRead = std::min (srcChannels, kNumCh);
                reader->read (in.getArrayOfWritePointers(), chToRead, srcPos, n);
                if (chToRead == 1)
                    in.copyFrom (1, 0, in, 0, 0, n);   // mono -> estéreo
                srcPos += n;
            }
            else
            {
                in.clear (0, n);
                if (++flushBlocks > 64)   // salvaguarda: nunca debería hacer falta
                    break;
            }

            // --- Escribir en la pista -------------------------------------------
            const auto room = track->getCapacityFrames() - outPos;
            if (! resample)
            {
                const auto toWrite = (int) std::min<std::int64_t> (n, room);
                for (int c = 0; c < kNumCh; ++c)
                    juce::FloatVectorOperations::copy (track->getWritePointer (c) + outPos, in.getReadPointer (c), toWrite);
                outPos += toWrite;
            }
            else
            {
                int produced = 0;
                for (int c = 0; c < kNumCh; ++c)
                {
                    const auto* src = in.getReadPointer (c);
                    for (int i = 0; i < n; ++i)
                        inD[(size_t) i] = (double) src[i];

                    double* out = nullptr;
                    const auto got = resamplers[c]->process (inD.data(), n, out);

                    // No escribir más allá del largo esperado (cola del filtro).
                    const auto limit = std::min<std::int64_t> (room, expectedOut - outPos);
                    produced = (int) juce::jlimit<std::int64_t> (0, got, limit);

                    auto* dest = track->getWritePointer (c) + outPos;
                    for (int i = 0; i < produced; ++i)
                        dest[i] = (float) out[i];
                }
                outPos += produced;
            }

            track->publishDecoded (outPos);
        }

        track->markComplete (resample ? std::min (outPos, expectedOut) : outPos);
        return LoadError::none;
    }
} // namespace kb::engine

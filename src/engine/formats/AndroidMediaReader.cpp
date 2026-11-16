#include "engine/formats/AndroidMediaReader.h"

#if JUCE_ANDROID

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace kb::engine::formats
{
    namespace
    {
        constexpr int32_t kPcm16    = 2;   // AudioFormat.ENCODING_PCM_16BIT
        constexpr int32_t kPcmFloat = 4;   // AudioFormat.ENCODING_PCM_FLOAT
        constexpr int64_t kTimeoutUs = 10000;
    }

    struct AndroidMediaReader::Impl
    {
        ~Impl()
        {
            if (codec != nullptr)     { AMediaCodec_stop (codec); AMediaCodec_delete (codec); }
            if (extractor != nullptr) AMediaExtractor_delete (extractor);
            if (fd >= 0)              ::close (fd);
        }

        bool open (const juce::File& file)
        {
            fd = ::open (file.getFullPathName().toRawUTF8(), O_RDONLY | O_CLOEXEC);
            if (fd < 0)
                return false;
            struct stat st {};
            if (::fstat (fd, &st) != 0)
                return false;

            extractor = AMediaExtractor_new();
            if (extractor == nullptr || AMediaExtractor_setDataSourceFd (extractor, fd, 0, st.st_size) != AMEDIA_OK)
                return false;

            const auto tracks = AMediaExtractor_getTrackCount (extractor);
            for (size_t t = 0; t < tracks; ++t)
            {
                auto* format = AMediaExtractor_getTrackFormat (extractor, t);
                const char* mime = nullptr;
                if (AMediaFormat_getString (format, AMEDIAFORMAT_KEY_MIME, &mime) && mime != nullptr
                    && std::strncmp (mime, "audio/", 6) == 0)
                {
                    int32_t rate = 0, ch = 0;
                    int64_t durationUs = 0;
                    AMediaFormat_getInt32 (format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &rate);
                    AMediaFormat_getInt32 (format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &ch);
                    AMediaFormat_getInt64 (format, AMEDIAFORMAT_KEY_DURATION, &durationUs);
                    sampleRate = (double) rate;
                    channels = ch;
                    lengthFrames = (juce::int64) std::llround ((double) durationUs * sampleRate / 1.0e6);

                    AMediaExtractor_selectTrack (extractor, t);
                    codec = AMediaCodec_createDecoderByType (mime);
                    // Pedimos float (Android 7+ la mayoría de los decodificadores); si no, llega PCM 16.
                    AMediaFormat_setInt32 (format, "pcm-encoding", kPcmFloat);
                    const auto ok = codec != nullptr
                                 && AMediaCodec_configure (codec, format, nullptr, nullptr, 0) == AMEDIA_OK
                                 && AMediaCodec_start (codec) == AMEDIA_OK;
                    AMediaFormat_delete (format);
                    return ok && rate > 0 && ch > 0 && lengthFrames > 0;
                }
                AMediaFormat_delete (format);
            }
            return false;
        }

        size_t pendingFrames() const noexcept { return channels > 0 ? pending.size() / (size_t) channels : 0; }

        void readOutputFormat()
        {
            auto* f = AMediaCodec_getOutputFormat (codec);
            int32_t enc = kPcm16, ch = channels;
            AMediaFormat_getInt32 (f, "pcm-encoding", &enc);
            AMediaFormat_getInt32 (f, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &ch);
            encoding = enc;
            if (ch > 0) outChannels = ch;
            AMediaFormat_delete (f);
        }

        /** Alimenta el decodificador y saca el próximo bloque de PCM a 'pending'. */
        bool decodeNextPacket()
        {
            for (int guard = 0; guard < 2000 && ! outputDone; ++guard)
            {
                if (! inputDone)
                {
                    const auto in = AMediaCodec_dequeueInputBuffer (codec, kTimeoutUs);
                    if (in >= 0)
                    {
                        size_t capacity = 0;
                        auto* buf = AMediaCodec_getInputBuffer (codec, (size_t) in, &capacity);
                        const auto size = AMediaExtractor_readSampleData (extractor, buf, capacity);
                        if (size < 0)
                        {
                            AMediaCodec_queueInputBuffer (codec, (size_t) in, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                            inputDone = true;
                        }
                        else
                        {
                            AMediaCodec_queueInputBuffer (codec, (size_t) in, 0, (size_t) size,
                                                          (uint64_t) AMediaExtractor_getSampleTime (extractor), 0);
                            AMediaExtractor_advance (extractor);
                        }
                    }
                }

                AMediaCodecBufferInfo info {};
                const auto out = AMediaCodec_dequeueOutputBuffer (codec, &info, kTimeoutUs);
                if (out == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
                {
                    readOutputFormat();
                    continue;
                }
                if (out < 0)
                    continue;   // todavía no hay salida (o cambió el buffer set)

                if ((info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0)
                    outputDone = true;

                size_t outSize = 0;
                const auto* data = AMediaCodec_getOutputBuffer (codec, (size_t) out, &outSize);
                const auto oc = std::max (1, outChannels);
                pending.clear();
                pendingPos = 0;
                if (data != nullptr && info.size > 0)
                {
                    const auto* bytes = data + info.offset;
                    const auto bytesPerSample = encoding == kPcmFloat ? 4 : 2;
                    const auto frames = (size_t) info.size / (size_t) (bytesPerSample * oc);
                    pending.resize (frames * (size_t) channels);
                    for (size_t i = 0; i < frames; ++i)
                        for (int c = 0; c < channels; ++c)
                        {
                            const auto src = i * (size_t) oc + (size_t) std::min (c, oc - 1);
                            float v;
                            if (encoding == kPcmFloat)
                                std::memcpy (&v, bytes + src * 4, 4);
                            else
                            {
                                int16_t s16;
                                std::memcpy (&s16, bytes + src * 2, 2);
                                v = (float) s16 / 32768.0f;
                            }
                            pending[i * (size_t) channels + (size_t) c] = v;
                        }
                    // Después de un salto se descarta lo anterior al frame pedido.
                    const auto firstFrame = (juce::int64) std::llround ((double) info.presentationTimeUs * sampleRate / 1.0e6);
                    if (discardUntil > firstFrame)
                        pendingPos = (size_t) std::min<juce::int64> ((juce::int64) frames, discardUntil - firstFrame);
                }
                AMediaCodec_releaseOutputBuffer (codec, (size_t) out, false);

                if (pendingFrames() > pendingPos)
                {
                    discardUntil = 0;
                    return true;
                }
            }
            return false;
        }

        void seekTo (juce::int64 frame)
        {
            const auto us = (int64_t) ((double) frame * 1.0e6 / sampleRate);
            AMediaExtractor_seekTo (extractor, us, AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC);
            AMediaCodec_flush (codec);
            inputDone = outputDone = false;
            pending.clear();
            pendingPos = 0;
            discardUntil = frame;
            nextFrame = frame;
            endOfStream = false;
        }

        int fd = -1;
        AMediaExtractor* extractor = nullptr;
        AMediaCodec* codec = nullptr;
        int channels = 0, outChannels = 0;
        int32_t encoding = kPcm16;
        double sampleRate = 0.0;
        juce::int64 lengthFrames = 0;

        std::vector<float> pending;
        size_t pendingPos = 0;
        juce::int64 nextFrame = 0, discardUntil = 0;
        bool inputDone = false, outputDone = false, endOfStream = false;
    };

    std::unique_ptr<juce::AudioFormatReader> AndroidMediaReader::create (const juce::File& file)
    {
        auto impl = std::make_unique<Impl>();
        if (! impl->open (file))
            return nullptr;
        impl->outChannels = impl->channels;

        const auto rate = impl->sampleRate;
        const auto ch = impl->channels;
        const auto len = impl->lengthFrames;
        std::unique_ptr<AndroidMediaReader> result (new AndroidMediaReader (std::move (impl)));
        result->sampleRate            = rate;
        result->numChannels           = (unsigned int) ch;
        result->bitsPerSample         = 32;
        result->usesFloatingPointData = true;
        result->lengthInSamples       = len;
        return result;
    }

    AndroidMediaReader::AndroidMediaReader (std::unique_ptr<Impl> i)
        : juce::AudioFormatReader (nullptr, "Android MediaCodec (AAC/M4A)"),
          impl (std::move (i))
    {
    }

    AndroidMediaReader::~AndroidMediaReader() = default;

    bool AndroidMediaReader::readSamples (int* const* destChannels, int numDestChannels,
                                          int startOffsetInDestBuffer, juce::int64 startSampleInFile,
                                          int numSamples)
    {
        auto& d = *impl;
        if (startSampleInFile != d.nextFrame)
            d.seekTo (startSampleInFile);

        int done = 0;
        while (done < numSamples)
        {
            const auto available = d.pendingFrames() - std::min (d.pendingPos, d.pendingFrames());
            if (available > 0)
            {
                const auto n = (int) std::min<size_t> (available, (size_t) (numSamples - done));
                for (int c = 0; c < numDestChannels; ++c)
                    if (auto* dest = reinterpret_cast<float*> (destChannels[c]))
                    {
                        const auto srcCh = (size_t) std::min (c, d.channels - 1);
                        for (int i = 0; i < n; ++i)
                            dest[startOffsetInDestBuffer + done + i] = d.pending[(d.pendingPos + (size_t) i) * (size_t) d.channels + srcCh];
                    }
                d.pendingPos += (size_t) n;
                done += n;
                continue;
            }

            if (d.endOfStream || ! d.decodeNextPacket())
            {
                d.endOfStream = true;
                for (int c = 0; c < numDestChannels; ++c)
                    if (auto* dest = reinterpret_cast<float*> (destChannels[c]))
                        std::fill_n (dest + startOffsetInDestBuffer + done, numSamples - done, 0.0f);
                break;
            }
        }

        d.nextFrame = startSampleInFile + numSamples;
        return true;
    }
} // namespace kb::engine::formats

#endif // JUCE_ANDROID

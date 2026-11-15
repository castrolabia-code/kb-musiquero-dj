#include "engine/formats/MediaFoundationReader.h"

#if JUCE_WINDOWS

#ifndef NOMINMAX
 #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace kb::engine::formats
{
    using Microsoft::WRL::ComPtr;

    static constexpr DWORD kAudioStream = (DWORD) MF_SOURCE_READER_FIRST_AUDIO_STREAM;

    struct MediaFoundationReader::Impl
    {
        ~Impl()
        {
            reader.Reset();
            if (mfStarted)
                MFShutdown();
        }

        /** Decodifica el próximo paquete en 'pending'. false = fin del stream o error. */
        bool decodeNextPacket()
        {
            // MF puede devolver lecturas sin muestra (p. ej. "stream tick"): se
            // reintenta un número acotado de veces para no colgarse nunca.
            for (int attempt = 0; attempt < 64; ++attempt)
            {
                DWORD flags = 0;
                LONGLONG timestamp = 0;
                ComPtr<IMFSample> sample;

                const auto hr = reader->ReadSample (kAudioStream, 0, nullptr, &flags, &timestamp, &sample);
                if (FAILED (hr) || (flags & MF_SOURCE_READERF_ERROR) != 0 || (flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0)
                    return false;

                if (sample == nullptr)
                    continue;

                ComPtr<IMFMediaBuffer> buffer;
                if (FAILED (sample->ConvertToContiguousBuffer (&buffer)))
                    return false;

                BYTE* data = nullptr;
                DWORD bytes = 0;
                if (FAILED (buffer->Lock (&data, nullptr, &bytes)))
                    return false;

                const auto numFloats = (size_t) bytes / sizeof (float);
                pending.resize (numFloats);
                if (numFloats > 0)
                    std::memcpy (pending.data(), data, numFloats * sizeof (float));
                buffer->Unlock();

                pendingPos = 0;
                if (pendingFrames() > 0)
                    return true;
            }
            return false;
        }

        size_t pendingFrames() const noexcept { return channels > 0 ? pending.size() / (size_t) channels : 0; }

        bool seekTo (juce::int64 frame)
        {
            PROPVARIANT pos;
            PropVariantInit (&pos);
            pos.vt = VT_I8;
            pos.hVal.QuadPart = (LONGLONG) ((double) frame * 1.0e7 / sampleRate);
            const auto hr = reader->SetCurrentPosition (GUID_NULL, pos);
            PropVariantClear (&pos);

            pending.clear();
            pendingPos = 0;
            endOfStream = FAILED (hr);
            nextFrame = frame;
            return SUCCEEDED (hr);
        }

        ComPtr<IMFSourceReader> reader;
        bool   mfStarted   = false;
        int    channels    = 0;
        double sampleRate  = 0.0;

        std::vector<float> pending;      // muestras intercaladas del último paquete
        size_t      pendingPos  = 0;     // en frames
        juce::int64 nextFrame   = 0;     // frame del archivo que corresponde a pendingPos
        bool        endOfStream = false;
    };

    std::unique_ptr<juce::AudioFormatReader> MediaFoundationReader::create (const juce::File& file)
    {
        auto impl = std::make_unique<Impl>();

        if (FAILED (MFStartup (MF_VERSION, MFSTARTUP_LITE)))
            return nullptr;
        impl->mfStarted = true;

        if (FAILED (MFCreateSourceReaderFromURL (file.getFullPathName().toWideCharPointer(), nullptr, &impl->reader)))
            return nullptr;

        auto& r = *impl->reader.Get();
        r.SetStreamSelection ((DWORD) MF_SOURCE_READER_ALL_STREAMS, FALSE);
        if (FAILED (r.SetStreamSelection (kAudioStream, TRUE)))
            return nullptr;

        // Pedimos PCM float 32; MF agrega el decodificador AAC correspondiente.
        ComPtr<IMFMediaType> wanted;
        if (FAILED (MFCreateMediaType (&wanted))
            || FAILED (wanted->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio))
            || FAILED (wanted->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_Float))
            || FAILED (r.SetCurrentMediaType (kAudioStream, nullptr, wanted.Get())))
            return nullptr;

        ComPtr<IMFMediaType> actual;
        if (FAILED (r.GetCurrentMediaType (kAudioStream, &actual)))
            return nullptr;

        GUID subtype {};
        UINT32 numChannels = 0, rate = 0, bits = 0;
        actual->GetGUID (MF_MT_SUBTYPE, &subtype);
        actual->GetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, &numChannels);
        actual->GetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        actual->GetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);

        if (subtype != MFAudioFormat_Float || bits != 32 || numChannels == 0 || rate == 0)
            return nullptr;

        // Duración (unidades de 100 ns) -> frames. Es una estimación: el largo
        // exacto lo determina TrackLoader al terminar de decodificar.
        juce::int64 lengthFrames = 0;
        PROPVARIANT duration;
        PropVariantInit (&duration);
        if (SUCCEEDED (r.GetPresentationAttribute ((DWORD) MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &duration))
            && duration.vt == VT_UI8)
        {
            lengthFrames = (juce::int64) std::llround ((double) duration.uhVal.QuadPart * (double) rate / 1.0e7);
        }
        PropVariantClear (&duration);

        if (lengthFrames <= 0)
            return nullptr;

        impl->channels   = (int) numChannels;
        impl->sampleRate = (double) rate;

        std::unique_ptr<MediaFoundationReader> result (new MediaFoundationReader (std::move (impl)));
        result->sampleRate            = (double) rate;
        result->numChannels           = (unsigned int) numChannels;
        result->bitsPerSample         = 32;
        result->usesFloatingPointData = true;
        result->lengthInSamples       = lengthFrames;
        return result;
    }

    MediaFoundationReader::MediaFoundationReader (std::unique_ptr<Impl> i)
        : juce::AudioFormatReader (nullptr, "Media Foundation (AAC/M4A)"),
          impl (std::move (i))
    {
    }

    MediaFoundationReader::~MediaFoundationReader() = default;

    bool MediaFoundationReader::readSamples (int* const* destChannels, int numDestChannels,
                                             int startOffsetInDestBuffer, juce::int64 startSampleInFile,
                                             int numSamples)
    {
        auto& d = *impl;

        if (startSampleInFile != d.nextFrame)
            d.seekTo (startSampleInFile);

        // usesFloatingPointData = true: los punteros de destino son float*.
        auto writeFrames = [&] (int destOffset, const float* interleaved, int frames)
        {
            for (int c = 0; c < numDestChannels; ++c)
            {
                if (auto* dest = reinterpret_cast<float*> (destChannels[c]))
                {
                    const auto srcCh = std::min (c, d.channels - 1);
                    for (int i = 0; i < frames; ++i)
                        dest[destOffset + i] = interleaved[(size_t) i * (size_t) d.channels + (size_t) srcCh];
                }
            }
        };

        int done = 0;
        while (done < numSamples)
        {
            const auto available = d.pendingFrames() - d.pendingPos;
            if (available > 0)
            {
                const auto n = (int) std::min<size_t> (available, (size_t) (numSamples - done));
                writeFrames (startOffsetInDestBuffer + done, d.pending.data() + d.pendingPos * (size_t) d.channels, n);
                d.pendingPos += (size_t) n;
                done += n;
                continue;
            }

            if (d.endOfStream || ! d.decodeNextPacket())
            {
                d.endOfStream = true;
                // Más allá del final: silencio (contrato de AudioFormatReader).
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

#endif // JUCE_WINDOWS

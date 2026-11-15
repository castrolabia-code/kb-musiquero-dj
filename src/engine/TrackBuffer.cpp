#include "engine/TrackBuffer.h"

#include <algorithm>
#include <cmath>

namespace kb::engine
{
    static std::uint64_t nextTrackId() noexcept
    {
        static std::atomic<std::uint64_t> counter { 0 };
        return ++counter;
    }

    TrackBuffer::TrackBuffer (TrackInfo i, double sr, std::int64_t capacityFrames)
        : info (std::move (i)),
          id (nextTrackId()),
          sampleRate (sr),
          capacity (std::max<std::int64_t> (capacityFrames, 1)),
          framesPerBin ((double) std::max<std::int64_t> (capacityFrames, 1) / (double) kOverviewBins)
    {
        jassert (sr > 0.0);

        // make_unique<T[]> inicializa en cero: las zonas no decodificadas son silencio.
        for (auto& ch : channels)
            ch = std::make_unique<float[]> ((size_t) capacity);

        overview = std::make_unique<float[]> ((size_t) kOverviewBins);
        length.store (capacity, std::memory_order_release);
    }

    float TrackBuffer::getLoadProgress() const noexcept
    {
        if (isComplete())
            return 1.0f;
        const auto len = getLengthFrames();
        return len > 0 ? juce::jlimit (0.0f, 1.0f, (float) getDecodedFrames() / (float) len) : 0.0f;
    }

    float TrackBuffer::getOverviewPeak (int bin) const noexcept
    {
        return juce::isPositiveAndBelow (bin, kOverviewBins) ? overview[(size_t) bin] : 0.0f;
    }

    int TrackBuffer::getOverviewBinsReady() const noexcept
    {
        if (isComplete())
            return kOverviewBins;
        return juce::jlimit (0, kOverviewBins, (int) std::floor ((double) getDecodedFrames() / framesPerBin));
    }

    void TrackBuffer::setEstimatedLength (std::int64_t frames) noexcept
    {
        length.store (juce::jlimit<std::int64_t> (0, capacity, frames), std::memory_order_release);
    }

    void TrackBuffer::updateOverview (std::int64_t from, std::int64_t to) noexcept
    {
        const auto* l = channels[0].get();
        const auto* r = channels[1].get();

        for (auto f = from; f < to; ++f)
        {
            const auto bin  = std::min (kOverviewBins - 1, (int) ((double) f / framesPerBin));
            const auto peak = std::max (std::abs (l[f]), std::abs (r[f]));
            if (peak > overview[(size_t) bin])
                overview[(size_t) bin] = peak;
        }
    }

    void TrackBuffer::publishDecoded (std::int64_t totalDecoded) noexcept
    {
        const auto previous = decoded.load (std::memory_order_relaxed);
        const auto now      = juce::jlimit<std::int64_t> (previous, capacity, totalDecoded);
        updateOverview (previous, now);
        decoded.store (now, std::memory_order_release);
    }

    void TrackBuffer::markComplete (std::int64_t finalLength) noexcept
    {
        const auto len = juce::jlimit<std::int64_t> (0, capacity, finalLength);
        publishDecoded (len);
        length.store (len, std::memory_order_release);
        complete.store (true, std::memory_order_release);
    }
} // namespace kb::engine

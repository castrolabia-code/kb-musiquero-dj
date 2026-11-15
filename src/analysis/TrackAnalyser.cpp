#include "analysis/TrackAnalyser.h"

#include "engine/TrackLoader.h"

#include <thread>

namespace kb::analysis
{
    TrackAnalyser::TrackAnalyser (juce::String threadName)
        : pool (juce::ThreadPoolOptions{}
                    .withThreadName (threadName)
                    .withNumberOfThreads (1)
                    .withDesiredThreadPriority (juce::Thread::Priority::low))
    {
    }

    TrackAnalyser::~TrackAnalyser()
    {
        shuttingDown->store (true);
        pool.removeAllJobs (true, 15000);
    }

    void TrackAnalyser::analyseTrack (std::shared_ptr<engine::TrackBuffer> track, std::function<void (BeatInfo, KeyInfo)> onDone)
    {
        auto stop = shuttingDown;
        pool.addJob ([track, onDone, stop]
        {
            // Esperar a que la pista termine de decodificarse (o se descarte).
            while (! track->isComplete())
            {
                if (stop->load() || track->isLoadingCancelled())
                    return;
                std::this_thread::sleep_for (std::chrono::milliseconds (50));
            }

            const auto info = BpmDetector::analyse (track->getReadPointer (0), track->getReadPointer (1),
                                                    track->getLengthFrames(), track->getSampleRate(), stop.get());
            if (stop->load())
                return;
            if (info.isValid())
                track->setBeatInfo (info.bpm, info.firstBeatSeconds);

            const auto key = KeyDetector::analyse (track->getReadPointer (0), track->getReadPointer (1),
                                                   track->getLengthFrames(), track->getSampleRate(), stop.get());
            if (stop->load())
                return;
            track->setKey (key.key);

            if (onDone)
                onDone (info, key);
        });
    }

    void TrackAnalyser::analyseFile (const juce::File& file, std::function<void (FileAnalysis)> onDone)
    {
        auto stop = shuttingDown;
        pool.addJob ([file, onDone, stop]
        {
            const auto result = analyseFileBlocking (file, stop.get());
            if (! stop->load() && onDone)
                onDone (result);
        });
    }

    FileAnalysis TrackAnalyser::analyseFileBlocking (const juce::File& file, const std::atomic<bool>* cancel)
    {
        FileAnalysis r;
        // 22 kHz alcanza para el análisis y ocupa la mitad de memoria.
        const auto loaded = engine::TrackLoader::loadBlocking (file, 22050.0);
        if (loaded.error != engine::LoadError::none || loaded.track == nullptr)
            return r;

        const auto& t = *loaded.track;
        r.durationSeconds = t.getLengthSeconds();
        r.formatName = t.getInfo().formatName;
        r.beat = BpmDetector::analyse (t.getReadPointer (0), t.getReadPointer (1), t.getLengthFrames(),
                                       t.getSampleRate(), cancel);
        r.key = KeyDetector::analyse (t.getReadPointer (0), t.getReadPointer (1), t.getLengthFrames(),
                                      t.getSampleRate(), cancel);
        r.ok = true;
        return r;
    }
} // namespace kb::analysis

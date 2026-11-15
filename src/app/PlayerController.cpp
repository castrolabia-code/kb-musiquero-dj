#include "app/PlayerController.h"

#include <algorithm>

namespace kb::app
{
    PlayerController::PlayerController (engine::AudioEngine& e, AnalysisCache& c)
        : engine (e), cache (c)
    {
        startTimerHz (10);   // recolección de pistas liberadas
    }

    PlayerController::~PlayerController()
    {
        stopTimer();
        // El TrackLoader (miembro) cancela y espera sus hilos al destruirse.
    }

    void PlayerController::loadFile (int deck, const juce::File& file)
    {
        if (! juce::isPositiveAndBelow (deck, kNumDecks))
            return;

        const auto request = ++latestRequest[(size_t) deck];
        loading[(size_t) deck] = true;

        // Sin dispositivo abierto todavía se decodifica a 48 kHz.
        const auto targetRate = engine.isPrepared() ? engine.getSampleRate() : 48000.0;

        juce::WeakReference<PlayerController> weak (this);

        loader.loadAsync (deck, file, targetRate,
            // Hilo de carga -> hilo de mensajes
            [weak, deck, request] (std::shared_ptr<engine::TrackBuffer> track)
            {
                juce::MessageManager::callAsync ([weak, deck, request, track]
                {
                    if (auto* self = weak.get())
                        self->handleReady (deck, request, track);
                    else
                        track->cancelLoading();
                });
            },
            [weak, deck, request, file] (engine::LoadError error)
            {
                juce::MessageManager::callAsync ([weak, deck, request, file, error]
                {
                    if (auto* self = weak.get())
                        self->handleError (deck, request, file, error);
                });
            });
    }

    void PlayerController::handleReady (int deck, std::uint64_t request, std::shared_ptr<engine::TrackBuffer> track)
    {
        if (request != latestRequest[(size_t) deck])
        {
            track->cancelLoading();   // ya se pidió otra pista para este deck
            return;
        }

        loading[(size_t) deck] = false;
        tracks[(size_t) deck] = track;

        // BPM y grilla: del cache si este archivo ya se analizó; si no, en segundo plano.
        const auto file = track->getInfo().file;
        if (const auto known = cache.find (file))
        {
            if (known->bpm > 0.0)
                track->setBeatInfo (known->bpm, known->firstBeatSeconds);
            track->setKey (known->key);
        }
        else
        {
            juce::WeakReference<PlayerController> weak (this);
            const auto id = track->getId();
            analysingIds.insert (id);
            analyser.analyseTrack (track, [weak, file, track, id] (analysis::BeatInfo info, analysis::KeyInfo key)
            {
                const auto duration = track->getLengthSeconds();
                const auto format = track->getInfo().formatName;
                juce::MessageManager::callAsync ([weak, file, info, key, duration, format, id]
                {
                    if (auto* self = weak.get())
                    {
                        self->analysingIds.erase (id);
                        AnalysisEntry e;
                        e.bpm = info.bpm;
                        e.firstBeatSeconds = info.firstBeatSeconds;
                        e.durationSeconds = duration;
                        e.formatName = format;
                        e.key = key.key;
                        self->cache.store (file, e);
                        juce::Logger::writeToLog ("BPM " + file.getFileName() + ": " + juce::String (info.bpm, 2)
                                                  + " (confianza " + juce::String (info.confidence, 2) + "), key "
                                                  + juce::String (analysis::MusicalKey::camelot (key.key)));
                    }
                });
            });
        }

        engine.loadTrack (deck, track);

        // Hot cues guardados de esta canción (Fase 4).
        savedCues[(size_t) deck].fill (-1.0);
        if (const auto cues = cache.findHotCues (file))
        {
            savedCues[(size_t) deck] = *cues;
            for (int i = 0; i < 8; ++i)
                if ((*cues)[(size_t) i] >= 0.0)
                    engine.setHotCue (deck, i, (*cues)[(size_t) i]);
        }

        juce::Logger::writeToLog ("Deck " + juce::String (deck + 1) + ": " + track->getInfo().file.getFullPathName()
                                  + " (" + track->getInfo().formatName + ", "
                                  + juce::String (track->getInfo().sourceSampleRate, 0) + " Hz)");

        const auto toNotify = listeners;
        for (auto* l : toNotify)
            l->trackLoaded (deck);
    }

    void PlayerController::handleError (int deck, std::uint64_t request, const juce::File& file, engine::LoadError error)
    {
        if (request != latestRequest[(size_t) deck])
            return;

        loading[(size_t) deck] = false;
        juce::Logger::writeToLog ("Deck " + juce::String (deck + 1) + ": error " + juce::String ((int) error)
                                  + " al cargar " + file.getFullPathName());

        const auto toNotify = listeners;
        for (auto* l : toNotify)
            l->trackLoadFailed (deck, file, error);
    }

    std::shared_ptr<engine::TrackBuffer> PlayerController::getTrack (int deck) const
    {
        return juce::isPositiveAndBelow (deck, kNumDecks) ? tracks[(size_t) deck] : nullptr;
    }

    bool PlayerController::isAnalysing (int deck) const
    {
        const auto t = getTrack (deck);
        return t != nullptr && analysingIds.count (t->getId()) > 0;
    }

    bool PlayerController::isLoading (int deck) const
    {
        return juce::isPositiveAndBelow (deck, kNumDecks) && loading[(size_t) deck];
    }

    void PlayerController::addListener (Listener* l)
    {
        if (std::find (listeners.begin(), listeners.end(), l) == listeners.end())
            listeners.push_back (l);
    }

    void PlayerController::removeListener (Listener* l)
    {
        listeners.erase (std::remove (listeners.begin(), listeners.end(), l), listeners.end());
    }

    void PlayerController::timerCallback()
    {
        engine.collectGarbage();

        // Hot cues: si cambiaron en el deck, se guardan en el cache de esa canción.
        for (int d = 0; d < kNumDecks; ++d)
        {
            const auto& t = tracks[(size_t) d];
            if (t == nullptr)
                continue;
            const auto st = engine.getDeckStatus (d);
            if (st.trackId != t->getId())
                continue;   // el motor todavía no tomó la pista (y sus cues)
            AnalysisCache::HotCues now;
            for (int i = 0; i < 8; ++i)
                now[(size_t) i] = st.hotCues[(size_t) i] < 0.0 ? -1.0 : std::round (st.hotCues[(size_t) i] * 1000.0) / 1000.0;
            if (now != savedCues[(size_t) d])
            {
                // Recién cargada: esperar a que el motor aplique los cues guardados.
                if (++cueSettleTicks[(size_t) d] < 3)
                    continue;
                savedCues[(size_t) d] = now;
                cache.storeHotCues (t->getInfo().file, now);
            }
            cueSettleTicks[(size_t) d] = 0;
        }

        // Los análisis nuevos se guardan cada 5 s (no solo al cerrar: si la PC se
        // apaga de golpe no se pierden).
        if (++ticks % 50 == 0)
            cache.save();
    }
} // namespace kb::app

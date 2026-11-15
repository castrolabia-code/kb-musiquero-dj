#pragma once
// =============================================================================
//  PlayerController: coordina la carga de pistas (TrackLoader, hilos de fondo)
//  con el motor de audio, desde el hilo de mensajes.
//
//  * loadFile() pide la carga; cuando la pista ya se puede reproducir, se
//    entrega al motor y se avisa a los listeners (la UI).
//  * Si mientras tanto se pidió otra pista para el mismo deck, la vieja se
//    descarta (gana siempre el último pedido).
//  * Libera periódicamente las pistas que el audio ya no usa.
//  * Analiza el BPM y la grilla de cada pista cargada (o los toma del cache).
// =============================================================================
#include <juce_events/juce_events.h>

#include "analysis/TrackAnalyser.h"
#include "app/AnalysisCache.h"
#include "engine/AudioEngine.h"
#include "engine/TrackLoader.h"

#include <array>
#include <set>
#include <memory>
#include <vector>

namespace kb::app
{
    class PlayerController final : private juce::Timer
    {
    public:
        static constexpr int kNumDecks = engine::AudioEngine::kNumDecks;

        struct Listener
        {
            virtual ~Listener() = default;
            virtual void trackLoaded (int deck) = 0;
            virtual void trackLoadFailed (int deck, const juce::File& file, engine::LoadError error) = 0;
        };

        PlayerController (engine::AudioEngine& engine, AnalysisCache& cache);
        ~PlayerController() override;

        /** Carga un archivo en un deck (asincrónico). */
        void loadFile (int deck, const juce::File& file);

        /** Pista del deck (la última pedida que ya está lista), o nullptr. */
        std::shared_ptr<engine::TrackBuffer> getTrack (int deck) const;
        /** true entre el pedido de carga y el momento en que la pista se puede reproducir. */
        bool isLoading (int deck) const;
        /** true mientras se calcula el BPM de la pista del deck. */
        bool isAnalysing (int deck) const;

        engine::Deck::Status getStatus (int deck) const { return engine.getDeckStatus (deck); }
        engine::AudioEngine& getEngine() const noexcept { return engine; }

        // Transporte
        void togglePlay (int deck)              { engine.togglePlay (deck); }
        void play (int deck)                    { engine.play (deck); }
        void pause (int deck)                   { engine.pause (deck); }
        void toggleSync (int deck)              { engine.toggleSync (deck); }
        void setTempo (int deck, float percent) { engine.setTempo (deck, percent); }
        void cueDown (int deck)                 { engine.cueDown (deck); }
        void cueUp (int deck)                   { engine.cueUp (deck); }
        void seek (int deck, double seconds)    { engine.seek (deck, seconds); }

        void addListener (Listener* l);
        void removeListener (Listener* l);

    private:
        void timerCallback() override;
        void handleReady (int deck, std::uint64_t request, std::shared_ptr<engine::TrackBuffer> track);
        void handleError (int deck, std::uint64_t request, const juce::File& file, engine::LoadError error);

        engine::AudioEngine& engine;
        AnalysisCache&       cache;
        engine::TrackLoader  loader { kNumDecks };
        analysis::TrackAnalyser analyser { "KB Deck Analysis" };

        std::array<std::shared_ptr<engine::TrackBuffer>, kNumDecks> tracks;
        std::array<std::uint64_t, kNumDecks> latestRequest {};
        std::array<bool, kNumDecks>          loading {};
        std::set<std::uint64_t>              analysingIds;
        int ticks = 0;
        std::array<AnalysisCache::HotCues, kNumDecks> savedCues {};
        std::array<int, kNumDecks> cueSettleTicks {};
        std::vector<Listener*> listeners;

        JUCE_DECLARE_WEAK_REFERENCEABLE (PlayerController)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerController)
    };
} // namespace kb::app

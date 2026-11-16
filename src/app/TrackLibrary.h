#pragma once
// =============================================================================
//  TrackLibrary (Fase 6): la biblioteca que ve el usuario, sobre la base SQLite.
//
//  Fuentes (columna izquierda):
//    * Todas las pistas (la colección)
//    * Preparación (lista para armar el set)
//    * Historial (lo que sonó en esta sesión)
//    * Playlists del usuario
//  Búsqueda instantánea sobre la fuente elegida (título, artista, álbum, género,
//  BPM y tonalidad). Al agregar canciones se leen sus tags (TagLib) y se ponen
//  en la cola de análisis (BPM, grilla y tonalidad) en segundo plano.
//  Uso exclusivo desde el hilo de mensajes.
// =============================================================================
#include <juce_events/juce_events.h>

#include "analysis/TrackAnalyser.h"
#include "app/AnalysisCache.h"
#include "library/LibraryDatabase.h"

#include <functional>
#include <set>
#include <vector>

namespace kb::app
{
    class TrackLibrary final
    {
    public:
        enum class SourceKind { collection, preparation, history, playlist };
        struct Source
        {
            SourceKind  kind = SourceKind::collection;
            juce::int64 playlistId = 0;
            bool operator== (const Source& o) const { return kind == o.kind && playlistId == o.playlistId; }
        };

        TrackLibrary (AnalysisCache& cache, library::LibraryDatabase& database);
        ~TrackLibrary();

        // --- Vista actual (fuente + búsqueda) -----------------------------------------------
        void   setSource (Source s);
        Source getSource() const noexcept { return source; }
        void   setSearch (const juce::String& text);
        juce::String getSearch() const { return search; }

        int  size() const noexcept { return (int) rows.size(); }
        const library::TrackRecord& operator[] (int row) const { return rows[(size_t) row]; }
        /** Posición de la fila en su fuente (para el número "#" y para quitar de una playlist). */
        int  sourceIndex (int row) const { return sourceIndices[(size_t) row]; }
        bool isAnalysing (int row) const;
        int  getNumPendingAnalyses() const noexcept { return (int) pending.size(); }

        // --- Colección ---------------------------------------------------------------------
        /** Agrega archivos a la colección (y a la playlist de la fuente actual, si es una). */
        int  addFiles (const juce::Array<juce::File>& files);
        void addFolder (const juce::File& folder);
        /** Quita filas: de la colección, o de la playlist si la fuente es una playlist. */
        void remove (const std::vector<int>& rowIndices);
        void clear();
        int  collectionSize() const { return db.collectionSize(); }
        void setRating (int row, int rating);

        // --- Playlists ---------------------------------------------------------------------
        std::vector<library::Playlist> playlists() const { return db.playlists(); }
        juce::int64 createPlaylist (const juce::String& name);
        void renamePlaylist (juce::int64 id, const juce::String& name);
        void deletePlaylist (juce::int64 id);
        void addToPlaylist (juce::int64 playlistId, const std::vector<juce::int64>& trackIds);
        juce::int64 preparationId();
        int historySize() const { return (int) db.historyTracks (session).size(); }

        /** Una canción empezó a sonar en un deck: historial y contador de reproducciones. */
        void markPlayed (const juce::File& file);

        /** Datos guardados de un archivo (tags, rating...). */
        std::optional<library::TrackRecord> find (const juce::File& file) const { return db.findByPath (file); }

        /** Se llama cuando cambia la lista o llega un análisis. */
        std::function<void()> onChanged;

    private:
        void rebuild();
        void notify();
        void queueAnalysis (const juce::File& file);
        void queueTags (const std::vector<juce::int64>& ids);
        bool matches (const library::TrackRecord& t, const juce::StringArray& words) const;

        AnalysisCache& cache;
        library::LibraryDatabase& db;
        Source source;
        juce::String search;
        juce::int64 session = 0;
        std::vector<library::TrackRecord> rows;
        std::vector<int> sourceIndices;
        std::set<juce::String> pending;
        analysis::TrackAnalyser analyser { "KB Library Analysis" };
        juce::ThreadPool tagPool { juce::ThreadPoolOptions().withThreadName ("KB Tags").withNumberOfThreads (1) };

        JUCE_DECLARE_WEAK_REFERENCEABLE (TrackLibrary)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackLibrary)
    };
} // namespace kb::app

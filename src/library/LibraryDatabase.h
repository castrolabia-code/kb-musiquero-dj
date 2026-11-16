#pragma once
// =============================================================================
//  LibraryDatabase (Fase 6): la biblioteca en SQLite
//  (%APPDATA%\Castro Labs\KB Musiquero DJ\library.db).
//
//  Tablas:
//    tracks          una fila por archivo (tags, análisis, rating, reproducciones)
//    hot_cues        8 por pista
//    playlists       playlists del usuario (+ la lista "Preparación")
//    playlist_tracks orden de las canciones en cada playlist
//    history         canciones que sonaron, por sesión
//
//  Uso exclusivo desde el hilo de mensajes (una sola conexión). El análisis y
//  la lectura de tags corren en hilos de fondo y guardan acá al terminar.
// =============================================================================
#include <juce_core/juce_core.h>

#include <array>
#include <optional>
#include <vector>

struct sqlite3;

namespace kb::library
{
    struct TrackRecord
    {
        juce::int64  id = 0;
        juce::File   file;
        juce::String title, artist, album, genre, format;
        int          year = 0;
        double       duration = 0.0;      // segundos
        double       bpm = 0.0, firstBeat = 0.0;
        int          key = -1;            // 0..23 (ver analysis::MusicalKey), -1 desconocida
        bool         analysed = false;    // BPM/key calculados por nosotros
        bool         tagsRead = false;
        int          rating = 0;          // 0..5
        juce::int64  fileSize = 0, modified = 0, added = 0, lastPlayed = 0;
        int          playCount = 0;
        bool         inCollection = true;
    };

    struct Playlist
    {
        enum Kind { normal = 0, preparation = 1 };
        juce::int64  id = 0;
        juce::String name;
        int          kind = normal;
        int          count = 0;
    };

    using HotCues = std::array<double, 8>;

    class LibraryDatabase final
    {
    public:
        static constexpr int kSchemaVersion = 1;

        explicit LibraryDatabase (const juce::File& dbFile);
        ~LibraryDatabase();

        bool isOpen() const noexcept { return db != nullptr; }
        juce::String getLastError() const { return lastError; }

        // --- Pistas ------------------------------------------------------------------------
        /** Busca por ruta; la crea si no existe (fuera de la colección si inCollection = false). */
        juce::int64 ensureTrack (const juce::File& file, bool addToCollection);
        std::optional<TrackRecord> findByPath (const juce::File& file) const;
        std::optional<TrackRecord> findById (juce::int64 id) const;
        /** Pistas de la colección ("Todas las pistas"), en orden de agregado. */
        std::vector<TrackRecord> collection() const;
        std::vector<TrackRecord> tracksById (const std::vector<juce::int64>& ids) const;
        int collectionSize() const;

        void setInCollection (const std::vector<juce::int64>& ids, bool in);
        void clearCollection();

        void setTags (juce::int64 id, const juce::String& title, const juce::String& artist, const juce::String& album,
                      const juce::String& genre, int year, double tagBpm, double duration);
        /** Resultado del análisis (BPM, grilla, tonalidad) + tamaño/fecha del archivo analizado. */
        void setAnalysis (juce::int64 id, double bpm, double firstBeat, int key, double duration, const juce::String& format,
                          juce::int64 fileSize, juce::int64 modified);
        void setRating (juce::int64 id, int rating);
        /** Sonó: suma una reproducción y la agrega al historial de la sesión. */
        void markPlayed (juce::int64 id, juce::int64 session);

        // --- Hot cues ----------------------------------------------------------------------
        HotCues hotCues (juce::int64 trackId) const;
        void    setHotCues (juce::int64 trackId, const HotCues& cues);

        // --- Playlists ---------------------------------------------------------------------
        std::vector<Playlist> playlists() const;
        juce::int64 createPlaylist (const juce::String& name, int kind = Playlist::normal);
        void renamePlaylist (juce::int64 id, const juce::String& name);
        void deletePlaylist (juce::int64 id);
        std::vector<juce::int64> playlistTracks (juce::int64 playlistId) const;
        void addToPlaylist (juce::int64 playlistId, const std::vector<juce::int64>& trackIds);
        /** Quita las filas en esas posiciones (0..n-1) y renumera. */
        void removeFromPlaylist (juce::int64 playlistId, std::vector<int> positions);
        /** La lista "Preparación" (se crea la primera vez). */
        juce::int64 preparationPlaylist (const juce::String& defaultName);

        // --- Historial ---------------------------------------------------------------------
        juce::int64 newSession();
        std::vector<juce::int64> historyTracks (juce::int64 session) const;

        // --- Transacciones (importaciones grandes) ---------------------------------------
        void begin();
        void commit();

    private:
        bool exec (const char* sql);
        void createSchema();
        static TrackRecord readTrack (void* statement);

        sqlite3* db = nullptr;
        mutable juce::String lastError;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryDatabase)
    };
} // namespace kb::library

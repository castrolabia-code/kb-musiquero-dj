#include "library/LibraryDatabase.h"

#include <sqlite3.h>

#include <algorithm>

namespace kb::library
{
    namespace
    {
        /** Sentencia preparada con limpieza automática. */
        class Stmt
        {
        public:
            Stmt (sqlite3* db, const char* sql) { if (sqlite3_prepare_v2 (db, sql, -1, &st, nullptr) != SQLITE_OK) st = nullptr; }
            ~Stmt() { sqlite3_finalize (st); }
            explicit operator bool() const { return st != nullptr; }

            Stmt& bind (int i, juce::int64 v)          { sqlite3_bind_int64 (st, i, v); return *this; }
            Stmt& bind (int i, int v)                  { sqlite3_bind_int (st, i, v); return *this; }
            Stmt& bind (int i, double v)               { sqlite3_bind_double (st, i, v); return *this; }
            Stmt& bind (int i, const juce::String& v)  { sqlite3_bind_text (st, i, v.toRawUTF8(), -1, SQLITE_TRANSIENT); return *this; }

            bool row()  { return st != nullptr && sqlite3_step (st) == SQLITE_ROW; }
            bool done() { return st != nullptr && sqlite3_step (st) == SQLITE_DONE; }
            void reset() { sqlite3_reset (st); sqlite3_clear_bindings (st); }

            juce::int64  i64 (int c) const  { return sqlite3_column_int64 (st, c); }
            int          i32 (int c) const  { return sqlite3_column_int (st, c); }
            double       dbl (int c) const  { return sqlite3_column_double (st, c); }
            juce::String text (int c) const { return juce::String::fromUTF8 ((const char*) sqlite3_column_text (st, c)); }
            sqlite3_stmt* get() const { return st; }

        private:
            sqlite3_stmt* st = nullptr;
        };

        constexpr const char* kTrackColumns =
            "id, path, title, artist, album, genre, format, year, duration, bpm, first_beat, key, analysed, tags_read,"
            " rating, file_size, modified, added, last_played, play_count, in_collection";
    }

    LibraryDatabase::LibraryDatabase (const juce::File& file)
    {
        file.getParentDirectory().createDirectory();
        if (sqlite3_open_v2 (file.getFullPathName().toRawUTF8(), &db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK)
        {
            lastError = db != nullptr ? juce::String (sqlite3_errmsg (db)) : juce::String ("sqlite3_open");
            sqlite3_close (db);
            db = nullptr;
            return;
        }
        exec ("PRAGMA journal_mode=WAL;");     // un corte de luz no corrompe la base
        exec ("PRAGMA synchronous=NORMAL;");
        exec ("PRAGMA foreign_keys=ON;");
        createSchema();
    }

    LibraryDatabase::~LibraryDatabase()
    {
        sqlite3_close (db);
    }

    bool LibraryDatabase::exec (const char* sql)
    {
        if (db == nullptr)
            return false;
        char* err = nullptr;
        if (sqlite3_exec (db, sql, nullptr, nullptr, &err) != SQLITE_OK)
        {
            lastError = err != nullptr ? juce::String (err) : juce::String ("sqlite error");
            sqlite3_free (err);
            return false;
        }
        return true;
    }

    void LibraryDatabase::createSchema()
    {
        exec (R"sql(
            CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT);
            CREATE TABLE IF NOT EXISTS tracks (
                id INTEGER PRIMARY KEY,
                path TEXT NOT NULL UNIQUE,
                title TEXT DEFAULT '', artist TEXT DEFAULT '', album TEXT DEFAULT '', genre TEXT DEFAULT '',
                format TEXT DEFAULT '', year INTEGER DEFAULT 0, duration REAL DEFAULT 0,
                bpm REAL DEFAULT 0, first_beat REAL DEFAULT 0, key INTEGER DEFAULT -1,
                analysed INTEGER DEFAULT 0, tags_read INTEGER DEFAULT 0, rating INTEGER DEFAULT 0,
                file_size INTEGER DEFAULT 0, modified INTEGER DEFAULT 0, added INTEGER DEFAULT 0,
                last_played INTEGER DEFAULT 0, play_count INTEGER DEFAULT 0, in_collection INTEGER DEFAULT 1);
            CREATE INDEX IF NOT EXISTS tracks_collection ON tracks (in_collection, added);
            CREATE TABLE IF NOT EXISTS hot_cues (
                track_id INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,
                idx INTEGER NOT NULL, seconds REAL NOT NULL, PRIMARY KEY (track_id, idx));
            CREATE TABLE IF NOT EXISTS playlists (
                id INTEGER PRIMARY KEY, name TEXT NOT NULL, kind INTEGER DEFAULT 0, position INTEGER DEFAULT 0);
            CREATE TABLE IF NOT EXISTS playlist_tracks (
                playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,
                track_id INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,
                position INTEGER NOT NULL);
            CREATE INDEX IF NOT EXISTS playlist_order ON playlist_tracks (playlist_id, position);
            CREATE TABLE IF NOT EXISTS history (
                id INTEGER PRIMARY KEY, track_id INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,
                session INTEGER NOT NULL, played_at INTEGER NOT NULL);
            CREATE INDEX IF NOT EXISTS history_session ON history (session, id);
        )sql");
        Stmt st (db, "INSERT OR IGNORE INTO meta (key, value) VALUES ('schema', ?)");
        st.bind (1, juce::String (kSchemaVersion)).done();
    }

    void LibraryDatabase::begin()  { exec ("BEGIN"); }
    void LibraryDatabase::commit() { exec ("COMMIT"); }

    TrackRecord LibraryDatabase::readTrack (void* p)
    {
        auto* st = (sqlite3_stmt*) p;
        auto text = [st] (int c) { return juce::String::fromUTF8 ((const char*) sqlite3_column_text (st, c)); };
        TrackRecord t;
        t.id        = sqlite3_column_int64 (st, 0);
        t.file      = juce::File (text (1));
        t.title     = text (2);
        t.artist    = text (3);
        t.album     = text (4);
        t.genre     = text (5);
        t.format    = text (6);
        t.year      = sqlite3_column_int (st, 7);
        t.duration  = sqlite3_column_double (st, 8);
        t.bpm       = sqlite3_column_double (st, 9);
        t.firstBeat = sqlite3_column_double (st, 10);
        t.key       = sqlite3_column_int (st, 11);
        t.analysed  = sqlite3_column_int (st, 12) != 0;
        t.tagsRead  = sqlite3_column_int (st, 13) != 0;
        t.rating    = sqlite3_column_int (st, 14);
        t.fileSize  = sqlite3_column_int64 (st, 15);
        t.modified  = sqlite3_column_int64 (st, 16);
        t.added     = sqlite3_column_int64 (st, 17);
        t.lastPlayed = sqlite3_column_int64 (st, 18);
        t.playCount = sqlite3_column_int (st, 19);
        t.inCollection = sqlite3_column_int (st, 20) != 0;
        return t;
    }

    juce::int64 LibraryDatabase::ensureTrack (const juce::File& file, bool addToCollection)
    {
        if (db == nullptr)
            return 0;
        const auto path = file.getFullPathName();
        {
            Stmt st (db, "SELECT id, in_collection FROM tracks WHERE path = ?");
            st.bind (1, path);
            if (st.row())
            {
                const auto id = st.i64 (0);
                if (addToCollection && st.i32 (1) == 0)
                    setInCollection ({ id }, true);
                return id;
            }
        }
        Stmt ins (db, "INSERT INTO tracks (path, title, added, in_collection) VALUES (?, ?, ?, ?)");
        ins.bind (1, path).bind (2, file.getFileNameWithoutExtension())
           .bind (3, juce::Time::currentTimeMillis()).bind (4, addToCollection ? 1 : 0);
        if (! ins.done())
            return 0;
        return sqlite3_last_insert_rowid (db);
    }

    std::optional<TrackRecord> LibraryDatabase::findByPath (const juce::File& file) const
    {
        if (db == nullptr) return std::nullopt;
        Stmt st (db, (juce::String ("SELECT ") + kTrackColumns + " FROM tracks WHERE path = ?").toRawUTF8());
        st.bind (1, file.getFullPathName());
        if (st.row()) return readTrack (st.get());
        return std::nullopt;
    }

    std::optional<TrackRecord> LibraryDatabase::findById (juce::int64 id) const
    {
        if (db == nullptr) return std::nullopt;
        Stmt st (db, (juce::String ("SELECT ") + kTrackColumns + " FROM tracks WHERE id = ?").toRawUTF8());
        st.bind (1, id);
        if (st.row()) return readTrack (st.get());
        return std::nullopt;
    }

    std::vector<TrackRecord> LibraryDatabase::collection() const
    {
        std::vector<TrackRecord> out;
        if (db == nullptr) return out;
        Stmt st (db, (juce::String ("SELECT ") + kTrackColumns + " FROM tracks WHERE in_collection = 1 ORDER BY added, id").toRawUTF8());
        while (st.row())
            out.push_back (readTrack (st.get()));
        return out;
    }

    int LibraryDatabase::collectionSize() const
    {
        if (db == nullptr) return 0;
        Stmt st (db, "SELECT COUNT(*) FROM tracks WHERE in_collection = 1");
        return st.row() ? st.i32 (0) : 0;
    }

    std::vector<TrackRecord> LibraryDatabase::tracksById (const std::vector<juce::int64>& ids) const
    {
        std::vector<TrackRecord> out;
        out.reserve (ids.size());
        if (db == nullptr) return out;
        Stmt st (db, (juce::String ("SELECT ") + kTrackColumns + " FROM tracks WHERE id = ?").toRawUTF8());
        for (auto id : ids)
        {
            st.reset();
            st.bind (1, id);
            if (st.row())
                out.push_back (readTrack (st.get()));
        }
        return out;
    }

    void LibraryDatabase::setInCollection (const std::vector<juce::int64>& ids, bool in)
    {
        Stmt st (db, "UPDATE tracks SET in_collection = ?, added = CASE WHEN ? = 1 AND in_collection = 0 THEN ? ELSE added END WHERE id = ?");
        for (auto id : ids)
        {
            st.reset();
            st.bind (1, in ? 1 : 0).bind (2, in ? 1 : 0).bind (3, juce::Time::currentTimeMillis()).bind (4, id);
            st.done();
        }
    }

    void LibraryDatabase::clearCollection()
    {
        exec ("UPDATE tracks SET in_collection = 0");
    }

    void LibraryDatabase::setTags (juce::int64 id, const juce::String& title, const juce::String& artist, const juce::String& album,
                                   const juce::String& genre, int year, double tagBpm, double duration)
    {
        // El BPM del tag solo se usa si todavía no lo analizamos nosotros.
        Stmt st (db, "UPDATE tracks SET title = ?, artist = ?, album = ?, genre = ?, year = ?, tags_read = 1,"
                     " duration = CASE WHEN duration > 0 THEN duration ELSE ? END,"
                     " bpm = CASE WHEN analysed = 1 OR ? <= 0 THEN bpm ELSE ? END WHERE id = ?");
        st.bind (1, title).bind (2, artist).bind (3, album).bind (4, genre).bind (5, year)
          .bind (6, duration).bind (7, tagBpm).bind (8, tagBpm).bind (9, id);
        st.done();
    }

    void LibraryDatabase::setAnalysis (juce::int64 id, double bpm, double firstBeat, int key, double duration,
                                       const juce::String& format, juce::int64 fileSize, juce::int64 modified)
    {
        Stmt st (db, "UPDATE tracks SET bpm = ?, first_beat = ?, key = ?, duration = ?, format = ?, file_size = ?,"
                     " modified = ?, analysed = 1 WHERE id = ?");
        st.bind (1, bpm).bind (2, firstBeat).bind (3, key).bind (4, duration).bind (5, format)
          .bind (6, fileSize).bind (7, modified).bind (8, id);
        st.done();
    }

    void LibraryDatabase::setRating (juce::int64 id, int rating)
    {
        Stmt st (db, "UPDATE tracks SET rating = ? WHERE id = ?");
        st.bind (1, juce::jlimit (0, 5, rating)).bind (2, id).done();
    }

    void LibraryDatabase::markPlayed (juce::int64 id, juce::int64 session)
    {
        const auto now = juce::Time::currentTimeMillis();
        Stmt up (db, "UPDATE tracks SET play_count = play_count + 1, last_played = ? WHERE id = ?");
        up.bind (1, now).bind (2, id).done();
        Stmt ins (db, "INSERT INTO history (track_id, session, played_at) VALUES (?, ?, ?)");
        ins.bind (1, id).bind (2, session).bind (3, now).done();
    }

    HotCues LibraryDatabase::hotCues (juce::int64 trackId) const
    {
        HotCues c;
        c.fill (-1.0);
        if (db == nullptr) return c;
        Stmt st (db, "SELECT idx, seconds FROM hot_cues WHERE track_id = ?");
        st.bind (1, trackId);
        while (st.row())
            if (juce::isPositiveAndBelow (st.i32 (0), 8))
                c[(size_t) st.i32 (0)] = st.dbl (1);
        return c;
    }

    void LibraryDatabase::setHotCues (juce::int64 trackId, const HotCues& cues)
    {
        begin();
        Stmt del (db, "DELETE FROM hot_cues WHERE track_id = ?");
        del.bind (1, trackId).done();
        Stmt ins (db, "INSERT INTO hot_cues (track_id, idx, seconds) VALUES (?, ?, ?)");
        for (int i = 0; i < 8; ++i)
        {
            if (cues[(size_t) i] < 0.0)
                continue;
            ins.reset();
            ins.bind (1, trackId).bind (2, i).bind (3, cues[(size_t) i]).done();
        }
        commit();
    }

    std::vector<Playlist> LibraryDatabase::playlists() const
    {
        std::vector<Playlist> out;
        if (db == nullptr) return out;
        Stmt st (db, "SELECT p.id, p.name, p.kind, (SELECT COUNT(*) FROM playlist_tracks t WHERE t.playlist_id = p.id)"
                     " FROM playlists p ORDER BY p.kind DESC, p.position, p.id");
        while (st.row())
        {
            Playlist p;
            p.id = st.i64 (0);
            p.name = st.text (1);
            p.kind = st.i32 (2);
            p.count = st.i32 (3);
            out.push_back (p);
        }
        return out;
    }

    juce::int64 LibraryDatabase::createPlaylist (const juce::String& name, int kind)
    {
        Stmt st (db, "INSERT INTO playlists (name, kind, position) VALUES (?, ?, (SELECT COALESCE(MAX(position), 0) + 1 FROM playlists))");
        st.bind (1, name).bind (2, kind);
        return st.done() ? sqlite3_last_insert_rowid (db) : 0;
    }

    void LibraryDatabase::renamePlaylist (juce::int64 id, const juce::String& name)
    {
        Stmt st (db, "UPDATE playlists SET name = ? WHERE id = ?");
        st.bind (1, name).bind (2, id).done();
    }

    void LibraryDatabase::deletePlaylist (juce::int64 id)
    {
        Stmt st (db, "DELETE FROM playlists WHERE id = ?");
        st.bind (1, id).done();
    }

    std::vector<juce::int64> LibraryDatabase::playlistTracks (juce::int64 playlistId) const
    {
        std::vector<juce::int64> out;
        if (db == nullptr) return out;
        Stmt st (db, "SELECT track_id FROM playlist_tracks WHERE playlist_id = ? ORDER BY position");
        st.bind (1, playlistId);
        while (st.row())
            out.push_back (st.i64 (0));
        return out;
    }

    void LibraryDatabase::addToPlaylist (juce::int64 playlistId, const std::vector<juce::int64>& trackIds)
    {
        begin();
        Stmt next (db, "SELECT COALESCE(MAX(position), -1) + 1 FROM playlist_tracks WHERE playlist_id = ?");
        next.bind (1, playlistId);
        auto pos = next.row() ? next.i32 (0) : 0;
        Stmt ins (db, "INSERT INTO playlist_tracks (playlist_id, track_id, position) VALUES (?, ?, ?)");
        for (auto id : trackIds)
        {
            ins.reset();
            ins.bind (1, playlistId).bind (2, id).bind (3, pos++).done();
        }
        commit();
    }

    void LibraryDatabase::removeFromPlaylist (juce::int64 playlistId, std::vector<int> positions)
    {
        auto ids = playlistTracks (playlistId);
        std::sort (positions.rbegin(), positions.rend());
        for (auto p : positions)
            if (juce::isPositiveAndBelow (p, (int) ids.size()))
                ids.erase (ids.begin() + p);
        begin();
        Stmt del (db, "DELETE FROM playlist_tracks WHERE playlist_id = ?");
        del.bind (1, playlistId).done();
        Stmt ins (db, "INSERT INTO playlist_tracks (playlist_id, track_id, position) VALUES (?, ?, ?)");
        int pos = 0;
        for (auto id : ids)
        {
            ins.reset();
            ins.bind (1, playlistId).bind (2, id).bind (3, pos++).done();
        }
        commit();
    }

    juce::int64 LibraryDatabase::preparationPlaylist (const juce::String& defaultName)
    {
        Stmt st (db, "SELECT id FROM playlists WHERE kind = 1 LIMIT 1");
        if (st.row())
            return st.i64 (0);
        return createPlaylist (defaultName, Playlist::preparation);
    }

    juce::int64 LibraryDatabase::newSession()
    {
        Stmt st (db, "SELECT COALESCE(MAX(session), 0) + 1 FROM history");
        return st.row() ? st.i64 (0) : 1;
    }

    std::vector<juce::int64> LibraryDatabase::historyTracks (juce::int64 session) const
    {
        std::vector<juce::int64> out;
        if (db == nullptr) return out;
        Stmt st (db, "SELECT track_id FROM history WHERE session = ? ORDER BY id");
        st.bind (1, session);
        while (st.row())
            out.push_back (st.i64 (0));
        return out;
    }
} // namespace kb::library

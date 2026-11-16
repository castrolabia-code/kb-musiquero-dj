#include "app/LibraryView.h"

#include "analysis/KeyDetector.h"
#include "engine/AudioFileFormats.h"
#include "ui/DeckDisplay.h"
#include "ui/DrawPrimitives.h"
#include "ui/KbLookAndFeel.h"
#include "ui/Palette.h"

#include <algorithm>

namespace kb::app
{
    using namespace ui::palette;
    using Source = TrackLibrary::Source;
    using SourceKind = TrackLibrary::SourceKind;

    // =========================================================================
    //  Columna izquierda: biblioteca y playlists (también recibe filas arrastradas)
    // =========================================================================
    class LibraryView::Sidebar final : public juce::Component,
                                       public juce::DragAndDropTarget
    {
    public:
        explicit Sidebar (LibraryView& o) : owner (o)
        {
            addButton.setTextScale (0.7f);
            addButton.setTooltipKey ("tip.libraryAdd");
            addButton.onPress = [this] { owner.showAddMenu(); };
            addAndMakeVisible (addButton);

            newPlaylistButton.setTextScale (0.7f);
            newPlaylistButton.setTooltipKey ("tip.newPlaylist");
            newPlaylistButton.onPress = [this] { newPlaylist(); };
            addAndMakeVisible (newPlaylistButton);
        }

        struct Entry
        {
            enum Type { section, item } type = item;
            Source source;
            juce::String name;
            int count = -1;
            juce::Rectangle<float> area;
        };

        void rebuild()
        {
            auto& lib = owner.library;
            entries.clear();
            entries.push_back ({ Entry::section, {}, tr ("library.sectionLibrary"), -1, {} });
            entries.push_back ({ Entry::item, { SourceKind::collection, 0 }, tr ("library.allTracks"), lib.collectionSize(), {} });
            const auto prepId = lib.preparationId();
            int prepCount = 0;
            std::vector<library::Playlist> user;
            for (const auto& p : lib.playlists())
            {
                if (p.kind == library::Playlist::preparation) prepCount = p.count;
                else user.push_back (p);
            }
            juce::ignoreUnused (prepId);
            entries.push_back ({ Entry::item, { SourceKind::preparation, 0 }, tr ("library.preparation"), prepCount, {} });
            entries.push_back ({ Entry::item, { SourceKind::history, 0 }, tr ("library.history"), lib.historySize(), {} });
            entries.push_back ({ Entry::section, {}, tr ("library.sectionPlaylists"), -1, {} });
            for (const auto& p : user)
                entries.push_back ({ Entry::item, { SourceKind::playlist, p.id }, p.name, p.count, {} });
            layoutEntries();
            repaint();
        }

        void resized() override { layoutEntries(); }

        void paint (juce::Graphics& g) override
        {
            g.setColour (panel);
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
            const auto current = owner.library.getSource();
            for (const auto& e : entries)
            {
                if (e.area.getBottom() < 0.0f || e.area.getY() > (float) getHeight())
                    continue;
                const auto textH = rowH * 0.55f;
                if (e.type == Entry::section)
                {
                    ui::draw::text (g, e.name, e.area.withTrimmedRight (rowH + 6.0f), textH * 0.82f, textSecondary,
                                    juce::Justification::centredLeft);
                    continue;
                }
                const bool selected = e.source == current;
                const bool target = dropRow >= 0 && &entries[(size_t) dropRow] == &e;
                if (selected || target)
                {
                    g.setColour ((target ? play : cyan).withAlpha (target ? 0.3f : 0.15f));
                    g.fillRoundedRectangle (e.area, 3.0f);
                }
                auto row = e.area;
                auto icon = row.removeFromLeft (rowH).reduced (rowH * 0.32f);
                const auto c = e.source.kind == SourceKind::preparation ? orange
                             : e.source.kind == SourceKind::history     ? textSecondary
                             : e.source.kind == SourceKind::playlist    ? play : cyan;
                g.setColour (selected ? c : c.withAlpha (0.6f));
                g.fillRoundedRectangle (icon, 2.0f);
                auto label = e.name;
                if (e.count >= 0)
                    label << "  (" << e.count << ")";
                ui::draw::text (g, label, row, textH, selected ? textPrimary : textSecondary, juce::Justification::centredLeft, false);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto i = entryAt (e.position);
            if (i < 0 || entries[(size_t) i].type != Entry::item)
                return;
            const auto entry = entries[(size_t) i];
            if (e.mods.isPopupMenu())
            {
                if (entry.source.kind == SourceKind::playlist)
                    playlistMenu (entry);
                return;
            }
            owner.library.setSource (entry.source);
        }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            scrollY -= w.deltaY * rowH * 4.0f;
            layoutEntries();
            repaint();
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const auto i = entryAt (e.position);
            if (i >= 0 && entries[(size_t) i].source.kind == SourceKind::playlist && entries[(size_t) i].type == Entry::item)
                rename (entries[(size_t) i]);
        }

        // --- Arrastrar filas a una playlist -------------------------------------------------
        bool isInterestedInDragSource (const SourceDetails& d) override
        {
            return d.description.toString().startsWith (kDragPrefix);
        }
        void itemDragMove (const SourceDetails& d) override
        {
            auto i = entryAt (d.localPosition.toFloat());
            if (i >= 0 && ! canDropOn (entries[(size_t) i]))
                i = -1;
            if (i != dropRow) { dropRow = i; repaint(); }
        }
        void itemDragExit (const SourceDetails&) override { dropRow = -1; repaint(); }
        void itemDropped (const SourceDetails& d) override
        {
            const auto i = entryAt (d.localPosition.toFloat());
            dropRow = -1;
            repaint();
            if (i < 0 || ! canDropOn (entries[(size_t) i]))
                return;
            const auto ids = idsFromDrag (d.description.toString());
            const auto& e = entries[(size_t) i];
            owner.library.addToPlaylist (e.source.kind == SourceKind::preparation ? owner.library.preparationId() : e.source.playlistId, ids);
        }

        float rowH = 26.0f, footerH = 40.0f;
        ui::GlowButton addButton { "library.add", cyan, true };
        ui::GlowButton newPlaylistButton { "library.add", play, true };

    private:
        static bool canDropOn (const Entry& e)
        {
            return e.type == Entry::item && (e.source.kind == SourceKind::playlist || e.source.kind == SourceKind::preparation);
        }

        void layoutEntries()
        {
            const auto sc = ui::KbLookAndFeel::uiScale();
            rowH = juce::jlimit (20.0f * sc, 32.0f * sc, (float) getHeight() / 7.0f);
            footerH = 0.0f;
            // Con muchas playlists la columna se desplaza con la rueda del mouse.
            const auto contentH = rowH * (float) entries.size() + rowH * 0.6f;
            scrollY = juce::jlimit (0.0f, juce::jmax (0.0f, contentH - (float) getHeight()), scrollY);
            auto y = rowH * 0.1f - scrollY;
            const auto x = rowH * 0.3f;
            const auto w = (float) getWidth() - 2.0f * x;
            for (auto& e : entries)
            {
                e.area = { x, y, w, rowH };
                y += rowH;
                if (e.type == Entry::section && e.source.kind == SourceKind::collection && e.name == tr ("library.sectionLibrary"))
                    addButton.setBounds (juce::Rectangle<int> ((int) rowH, (int) rowH).withCentre ({ (int) (x + w - rowH * 0.5f), (int) e.area.getCentreY() }));
                if (e.type == Entry::section && e.name == tr ("library.sectionPlaylists"))
                    newPlaylistButton.setBounds (juce::Rectangle<int> ((int) rowH, (int) rowH).withCentre ({ (int) (x + w - rowH * 0.5f), (int) e.area.getCentreY() }));
            }
        }

        int entryAt (juce::Point<float> p) const
        {
            for (int i = 0; i < (int) entries.size(); ++i)
                if (entries[(size_t) i].area.contains (p))
                    return i;
            return -1;
        }

        void newPlaylist()
        {
            const auto n = (int) owner.library.playlists().size();
            juce::Component::SafePointer<Sidebar> safe (this);
            owner.askPlaylistName (tr ("library.newPlaylist"), tr ("library.defaultPlaylist", { { "n", juce::String (n) } }),
                                   [safe] (juce::String name)
                                   {
                                       if (safe == nullptr) return;
                                       const auto id = safe->owner.library.createPlaylist (name);
                                       safe->owner.library.setSource ({ SourceKind::playlist, id });
                                   });
        }

        void rename (const Entry& e)
        {
            juce::Component::SafePointer<Sidebar> safe (this);
            const auto id = e.source.playlistId;
            owner.askPlaylistName (tr ("library.renamePlaylist"), e.name, [safe, id] (juce::String name)
            {
                if (safe != nullptr) safe->owner.library.renamePlaylist (id, name);
            });
        }

        void playlistMenu (const Entry& e)
        {
            juce::PopupMenu m;
            juce::Component::SafePointer<Sidebar> safe (this);
            const auto entry = e;
            m.addItem (tr ("library.renamePlaylist"), [safe, entry] { if (safe != nullptr) safe->rename (entry); });
            m.addItem (tr ("library.deletePlaylist"), [safe, entry]
            {
                if (safe == nullptr) return;
                juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                                  .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                                  .withTitle (tr ("library.deletePlaylist"))
                                                  .withMessage (tr ("library.deletePlaylistConfirm", { { "name", entry.name } }))
                                                  .withButton (tr ("library.deletePlaylist"))
                                                  .withButton (tr ("common.cancel"))
                                                  .withAssociatedComponent (safe.getComponent()),
                                              [safe, entry] (int r)
                                              {
                                                  if (safe != nullptr && r == 1)
                                                      safe->owner.library.deletePlaylist (entry.source.playlistId);
                                              });
            });
            m.showMenuAsync (juce::PopupMenu::Options());
        }

        LibraryView& owner;
        std::vector<Entry> entries;
        int dropRow = -1;
        float scrollY = 0.0f;
    };

    // =========================================================================
    //  Descripción de arrastre: "kbtrack:<ruta>\n<id>,<id>,..."
    // =========================================================================
    juce::File LibraryView::fileFromDrag (const juce::String& d)
    {
        return juce::File (d.fromFirstOccurrenceOf (kDragPrefix, false, false).upToFirstOccurrenceOf ("\n", false, false));
    }

    std::vector<juce::int64> LibraryView::idsFromDrag (const juce::String& d)
    {
        std::vector<juce::int64> ids;
        for (const auto& t : juce::StringArray::fromTokens (d.fromFirstOccurrenceOf ("\n", false, false), ",", ""))
            if (t.getLargeIntValue() > 0)
                ids.push_back (t.getLargeIntValue());
        return ids;
    }

    // =========================================================================
    //  Construcción
    // =========================================================================
    LibraryView::LibraryView (TrackLibrary& lib) : library (lib)
    {
        setOpaque (true);

        table.setColour (juce::ListBox::backgroundColourId, panel);
        table.setColour (juce::ListBox::outlineColourId, panelEdge);
        table.setRowHeight (26);
        table.setMultipleSelectionEnabled (true);
        auto& header = table.getHeader();
        header.setColour (juce::TableHeaderComponent::backgroundColourId, panelLight);
        header.setColour (juce::TableHeaderComponent::textColourId, textSecondary);
        header.setColour (juce::TableHeaderComponent::outlineColourId, panelEdge);
        header.setColour (juce::TableHeaderComponent::highlightColourId, cyan.withAlpha (0.2f));
        header.setPopupMenuActive (true);   // click derecho: mostrar / ocultar columnas

        const auto colFlags = juce::TableHeaderComponent::defaultFlags;
        header.addColumn ("#",  colNumber,   44,  30, 80, colFlags);
        header.addColumn ("[]", colArt,      34,  26, 60, colFlags & ~juce::TableHeaderComponent::sortable);
        header.addColumn ("t",  colTitle,    300, 80, 1200, colFlags);
        header.addColumn ("a",  colArtist,   200, 60, 800, colFlags);
        header.addColumn ("b",  colBpm,      76,  50, 140, colFlags);
        header.addColumn ("k",  colKey,      86,  50, 160, colFlags);
        header.addColumn ("d",  colDuration, 80,  60, 140, colFlags);
        header.addColumn ("g",  colGenre,    130, 60, 400, colFlags);
        header.addColumn ("r",  colRating,   96,  70, 160, colFlags);
        header.addColumn ("f",  colFormat,   70,  50, 200, colFlags);
        headerWatcher = std::make_unique<HeaderWatcher> (*this);
        header.addListener (headerWatcher.get());
        updateHeaderTexts();
        addAndMakeVisible (table);

        searchBox.setTextToShowWhenEmpty (tr ("library.search"), textSecondary);
        searchBox.setTooltip (tr ("tip.search"));
        searchBox.setColour (juce::TextEditor::backgroundColourId, panelDark);
        searchBox.setColour (juce::TextEditor::outlineColourId, panelEdge);
        searchBox.setColour (juce::TextEditor::focusedOutlineColourId, cyan);
        searchBox.setColour (juce::TextEditor::textColourId, textPrimary);
        searchBox.onTextChange = [this] { library.setSearch (searchBox.getText()); };
        searchBox.onEscapeKey  = [this] { searchBox.clear(); library.setSearch ({}); };
        addAndMakeVisible (searchBox);

        sidebar = std::make_unique<Sidebar> (*this);
        addAndMakeVisible (*sidebar);

        clearButton.setTooltipKey ("tip.libraryClear");
        clearButton.setTextScale (0.5f);
        clearButton.onPress = [this] { confirmClear(); };
        addAndMakeVisible (clearButton);

        library.onChanged = [this] { refresh(); };
        I18n::getInstance().addListener (this);
        rebuildOrder();
        sidebar->rebuild();
    }

    LibraryView::~LibraryView()
    {
        table.getHeader().removeListener (headerWatcher.get());
        library.onChanged = nullptr;
        if (artwork != nullptr)
            artwork->onArtworkArrived = nullptr;
        I18n::getInstance().removeListener (this);
    }

    void LibraryView::setArtworkCache (library::ArtworkCache* cache)
    {
        artwork = cache;
        if (artwork != nullptr)
        {
            juce::Component::SafePointer<LibraryView> safe (this);
            artwork->onArtworkArrived = [safe] { if (safe != nullptr) safe->table.repaint(); };
        }
    }

    void LibraryView::restoreColumns (const juce::String& state)
    {
        if (state.isNotEmpty())
        {
            table.getHeader().restoreFromString (state);
            columnScale = 1.0f;   // lo guardado está a escala 1
            resized();
        }
    }

    juce::String LibraryView::getColumnsState() const
    {
        // Guardar los anchos divididos por la escala actual.
        auto xml = juce::parseXML (table.getHeader().toString());
        if (xml != nullptr && columnScale > 0.0f)
            for (auto* col : xml->getChildIterator())
                col->setAttribute ("width", juce::roundToInt (col->getDoubleAttribute ("width") / columnScale));
        return xml != nullptr ? xml->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader()) : table.getHeader().toString();
    }

    void LibraryView::updateHeaderTexts()
    {
        auto& h = table.getHeader();
        h.setColumnName (colArt,      tr ("library.col.art"));
        h.setColumnName (colTitle,    tr ("library.col.title"));
        h.setColumnName (colArtist,   tr ("library.col.artist"));
        h.setColumnName (colBpm,      tr ("library.col.bpm"));
        h.setColumnName (colKey,      tr ("library.col.key"));
        h.setColumnName (colDuration, tr ("library.col.duration"));
        h.setColumnName (colGenre,    tr ("library.col.genre"));
        h.setColumnName (colRating,   tr ("library.col.rating"));
        h.setColumnName (colFormat,   tr ("library.col.format"));
    }

    void LibraryView::languageChanged()
    {
        updateHeaderTexts();
        searchBox.setTextToShowWhenEmpty (tr ("library.search"), textSecondary);
        sidebar->rebuild();
        repaint();
    }

    // =========================================================================
    //  Agregar canciones
    // =========================================================================
    void LibraryView::showAddMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, tr ("library.addFiles"));
        menu.addItem (2, tr ("library.addFolder"));

        juce::Component::SafePointer<LibraryView> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sidebar->addButton), [safe] (int choice)
        {
            if (safe == nullptr || choice == 0)
                return;

            const auto files = choice == 1;
            safe->chooser = std::make_unique<juce::FileChooser> (tr (files ? "library.addFiles" : "library.addFolder"),
                                                                 juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                                 files ? engine::formats::fileWildcard() : juce::String());
            const auto chooserFlags = juce::FileBrowserComponent::openMode
                               | (files ? juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems
                                        : juce::FileBrowserComponent::canSelectDirectories);

            safe->chooser->launchAsync (chooserFlags, [safe, files] (const juce::FileChooser& fc)
            {
                if (safe == nullptr)
                    return;
                if (files)
                    safe->library.addFiles (fc.getResults());
                else if (fc.getResult().isDirectory())
                    safe->library.addFolder (fc.getResult());
            });
        });
    }

    void LibraryView::askPlaylistName (const juce::String& title, const juce::String& initial, std::function<void (juce::String)> done)
    {
        auto* w = new juce::AlertWindow (title, tr ("library.playlistName"), juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", initial);
        w->addButton (tr ("common.ok"), 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton (tr ("common.cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([w, done] (int result)
        {
            const auto name = w->getTextEditorContents ("name").trim();
            if (result == 1 && name.isNotEmpty() && done)
                done (name);
        }), true);
    }

    // =========================================================================
    //  Orden y datos
    // =========================================================================
    void LibraryView::setReferenceKeys (const std::vector<int>& keys)
    {
        if (keys != referenceKeys)
        {
            referenceKeys = keys;
            table.repaint();
        }
    }

    void LibraryView::refresh()
    {
        rebuildOrder();
        table.updateContent();
        sidebar->rebuild();
        repaint();
    }

    juce::String LibraryView::cellText (int item, int column) const
    {
        const auto& t = library[item];
        switch (column)
        {
            case colNumber:   return juce::String (library.sourceIndex (item) + 1);
            case colTitle:    return t.title.isNotEmpty() ? t.title : t.file.getFileNameWithoutExtension();
            case colArtist:   return t.artist;
            case colGenre:    return t.genre;
            case colBpm:
                if (t.bpm > 0.0) return ui::DeckDisplay::formatBpm (t.bpm);
                return library.isAnalysing (item) ? "..." : "--";
            case colDuration:
                if (t.duration > 0.0)
                    return ui::DeckDisplay::formatTime (t.duration).upToLastOccurrenceOf (".", false, false);
                return library.isAnalysing (item) ? "..." : "--";
            case colFormat:   return t.file.getFileExtension().trimCharactersAtStart (".").toUpperCase();
            case colKey:
                if (t.key >= 0)
                    return juce::String (analysis::MusicalKey::camelot (t.key)) + "  " + juce::String (analysis::MusicalKey::name (t.key));
                return library.isAnalysing (item) ? "..." : "--";
            default:          break;
        }
        return {};
    }

    void LibraryView::rebuildOrder()
    {
        order.resize ((size_t) library.size());
        for (int i = 0; i < library.size(); ++i)
            order[(size_t) i] = i;

        if (sortColumn <= 0 || sortColumn == colNumber || sortColumn == colArt)
        {
            if (! sortForwards)
                std::reverse (order.begin(), order.end());
            return;
        }

        auto key = [this] (int i) -> std::pair<double, juce::String>
        {
            const auto& t = library[i];
            switch (sortColumn)
            {
                case colBpm:      return { t.bpm, {} };
                case colDuration: return { t.duration, {} };
                case colRating:   return { (double) t.rating, {} };
                case colKey:
                    // Orden de la rueda de Camelot: 1A, 1B, 2A, 2B...
                    return { t.key < 0 ? 99.0 : analysis::MusicalKey::camelotNumber (t.key) * 2.0 + (analysis::MusicalKey::isMinor (t.key) ? 0.0 : 1.0), {} };
                default:          return { 0.0, cellText (i, sortColumn).toLowerCase() };
            }
        };

        std::stable_sort (order.begin(), order.end(), [&] (int a, int b)
        {
            const auto ka = key (a), kb = key (b);
            return sortForwards ? ka < kb : kb < ka;
        });
    }

    int LibraryView::itemForRow (int row) const
    {
        return juce::isPositiveAndBelow (row, (int) order.size()) ? order[(size_t) row] : -1;
    }

    int LibraryView::getNumRows() { return (int) order.size(); }

    void LibraryView::sortOrderChanged (int column, bool forwards)
    {
        sortColumn = column;
        sortForwards = forwards;
        rebuildOrder();
        table.updateContent();
        table.repaint();
    }

    void LibraryView::moveSelection (int delta)
    {
        if (order.empty())
            return;
        const auto current = table.getSelectedRow (0);
        const auto next = juce::jlimit (0, (int) order.size() - 1, current < 0 ? 0 : current + delta);
        table.selectRow (next);
        table.scrollToEnsureRowIsOnscreen (next);
    }

    juce::File LibraryView::selectedFile() const
    {
        const auto item = itemForRow (table.getSelectedRow (0));
        return item >= 0 ? library[item].file : juce::File();
    }

    std::vector<juce::int64> LibraryView::selectedIds() const
    {
        std::vector<juce::int64> ids;
        const auto rows = table.getSelectedRows();
        for (int i = 0; i < rows.size(); ++i)
            if (const auto item = itemForRow (rows[i]); item >= 0)
                ids.push_back (library[item].id);
        return ids;
    }

    // =========================================================================
    //  Pintado de filas
    // =========================================================================
    void LibraryView::paintRowBackground (juce::Graphics& g, int row, int, int, bool selected)
    {
        if (selected)
            g.fillAll (cyan.withAlpha (0.22f));
        else if (row % 2 == 1)
            g.fillAll (juce::Colours::white.withAlpha (0.025f));
    }

    void LibraryView::paintCell (juce::Graphics& g, int row, int column, int w, int h, bool selected)
    {
        const auto item = itemForRow (row);
        if (item < 0)
            return;
        const auto& t = library[item];

        if (column == colArt)
        {
            const auto size = juce::jmin (w, h) - 4;
            const auto box = juce::Rectangle<int> (size, size).withCentre ({ w / 2, h / 2 });
            juce::Image thumb;
            if (artwork != nullptr && size > 4)
                thumb = artwork->getThumbnail (t.file, size * 2);
            if (thumb.isValid())
            {
                g.setOpacity (1.0f);
                g.drawImage (thumb, box.toFloat());
            }
            else
            {
                g.setColour (panelEdge);
                g.fillRoundedRectangle (box.toFloat(), 2.0f);
            }
            return;
        }

        if (column == colRating)
        {
            // 5 estrellas: click para calificar.
            const auto starW = juce::jmin ((float) (w - 8) / 5.0f, (float) h * 0.8f);
            for (int s = 0; s < 5; ++s)
            {
                juce::Path star;
                const auto c = juce::Point<float> (4.0f + starW * ((float) s + 0.5f), (float) h * 0.5f);
                star.addStar (c, 5, starW * 0.2f, starW * 0.45f, 0.0f);
                g.setColour (s < t.rating ? orange : textSecondary.withAlpha (0.25f));
                g.fillPath (star);
            }
            return;
        }

        auto colour = column == colTitle ? textPrimary : textSecondary;
        if (column == colBpm)
            colour = cyan;
        if (column == colKey)
        {
            const auto k = t.key;
            const auto compatible = std::any_of (referenceKeys.begin(), referenceKeys.end(),
                                                 [k] (int ref) { return analysis::MusicalKey::compatible (k, ref); });
            colour = compatible ? play : orange.withAlpha (0.9f);
            if (compatible)
            {
                g.setColour (play.withAlpha (0.12f));
                g.fillRect (0, 0, w, h);
            }
        }
        if (selected)
            colour = colour.brighter (0.3f);

        g.setColour (colour);
        g.setFont (ui::KbLookAndFeel::labelFont ((float) h * 0.52f, column == colTitle || column == colBpm || column == colKey));
        const auto just = (column == colBpm || column == colDuration || column == colNumber)
                              ? juce::Justification::centredRight : juce::Justification::centredLeft;
        g.drawText (cellText (item, column), 6, 0, w - 12, h, just, true);
    }

    // =========================================================================
    //  Interacción
    // =========================================================================
    void LibraryView::cellDoubleClicked (int row, int, const juce::MouseEvent&)
    {
        const auto item = itemForRow (row);
        if (item >= 0 && onLoadRequest)
            onLoadRequest (-1, library[item].file);
    }

    void LibraryView::cellClicked (int row, int column, const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            if (! table.isRowSelected (row))
                table.selectRow (row);
            showTrackMenu (row);
            return;
        }
        if (column == colRating)
        {
            const auto item = itemForRow (row);
            const auto w = table.getHeader().getColumnWidth (colRating);
            const auto starW = juce::jmin ((float) (w - 8) / 5.0f, (float) table.getRowHeight() * 0.8f);
            // Posición dentro de la celda (el evento viene relativo a la fila).
            const auto cellX = (float) e.x - (float) table.getHeader().getColumnPosition (table.getHeader().getIndexOfColumnId (colRating, true)).getX();
            const auto stars = juce::jlimit (0, 5, (int) std::ceil ((cellX - 4.0f) / starW));
            if (item >= 0)
                library.setRating (item, library[item].rating == stars ? 0 : stars);   // mismo valor: borra
            table.repaintRow (row);
        }
    }

    void LibraryView::showTrackMenu (int row)
    {
        const auto item = itemForRow (row);
        if (item < 0)
            return;
        const auto file = library[item].file;
        const auto ids = selectedIds();

        juce::PopupMenu menu;
        juce::Component::SafePointer<LibraryView> safe (this);
        menu.addItem (tr ("menu.file.loadDeck", { { "n", "1" } }).trimCharactersAtEnd ("."),
                      [safe, file] { if (safe != nullptr && safe->onLoadRequest) safe->onLoadRequest (0, file); });
        menu.addItem (tr ("menu.file.loadDeck", { { "n", "2" } }).trimCharactersAtEnd ("."),
                      [safe, file] { if (safe != nullptr && safe->onLoadRequest) safe->onLoadRequest (1, file); });
        menu.addSeparator();
        menu.addItem (tr ("library.addToPrep"), [safe, ids]
        {
            if (safe != nullptr) safe->library.addToPlaylist (safe->library.preparationId(), ids);
        });
        juce::PopupMenu lists;
        for (const auto& p : library.playlists())
        {
            if (p.kind != library::Playlist::normal)
                continue;
            const auto pid = p.id;
            lists.addItem (p.name, [safe, pid, ids] { if (safe != nullptr) safe->library.addToPlaylist (pid, ids); });
        }
        lists.addSeparator();
        lists.addItem (tr ("library.newPlaylist"), [safe, ids]
        {
            if (safe == nullptr) return;
            const auto n = (int) safe->library.playlists().size();
            safe->askPlaylistName (tr ("library.newPlaylist"), tr ("library.defaultPlaylist", { { "n", juce::String (n) } }),
                                   [safe, ids] (juce::String name)
                                   {
                                       if (safe == nullptr) return;
                                       safe->library.addToPlaylist (safe->library.createPlaylist (name), ids);
                                   });
        });
        menu.addSubMenu (tr ("library.addToPlaylist"), lists);

        juce::PopupMenu rating;
        for (int s = 0; s <= 5; ++s)
            rating.addItem (s == 0 ? juce::String ("-") : juce::String::repeatedString (juce::String::fromUTF8 ("\xe2\x98\x85"), s),
                            [safe, item, s] { if (safe != nullptr) safe->library.setRating (item, s); });
        menu.addSubMenu (tr ("library.col.rating"), rating);
        menu.addSeparator();
        menu.addItem (tr (library.getSource().kind == SourceKind::collection ? "library.remove" : "library.removeFromList"),
                      library.getSource().kind != SourceKind::history,
                      false, [safe] { if (safe != nullptr) safe->deleteKeyPressed (0); });
        menu.showMenuAsync (juce::PopupMenu::Options());
    }

    void LibraryView::deleteKeyPressed (int)
    {
        if (library.getSource().kind == SourceKind::history)
            return;
        std::vector<int> items;
        const auto rows = table.getSelectedRows();
        for (int i = 0; i < rows.size(); ++i)
            if (const auto item = itemForRow (rows[i]); item >= 0)
                items.push_back (item);
        table.deselectAllRows();
        library.remove (items);
    }

    juce::var LibraryView::getDragSourceDescription (const juce::SparseSet<int>& rows)
    {
        if (rows.isEmpty())
            return {};
        const auto first = itemForRow (rows[0]);
        if (first < 0)
            return {};
        juce::StringArray ids;
        for (int i = 0; i < rows.size(); ++i)
            if (const auto item = itemForRow (rows[i]); item >= 0)
                ids.add (juce::String (library[item].id));
        return juce::var (juce::String (kDragPrefix) + library[first].file.getFullPathName() + "\n" + ids.joinIntoString (","));
    }

    void LibraryView::confirmClear()
    {
        if (library.size() == 0 || library.getSource().kind == SourceKind::history)
            return;

        const bool isList = library.getSource().kind != SourceKind::collection;
        juce::Component::SafePointer<LibraryView> safe (this);
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                          .withTitle (tr ("library.clearConfirmTitle"))
                                          .withMessage (tr (isList ? "library.clearPlaylistConfirmBody" : "library.clearConfirmBody",
                                                            { { "n", juce::String (library.size()) } }))
                                          .withButton (tr ("library.clear"))
                                          .withButton (tr ("common.cancel"))
                                          .withAssociatedComponent (this),
                                      [safe] (int result)
                                      {
                                          if (safe != nullptr && result == 1)
                                          {
                                              safe->table.deselectAllRows();
                                              safe->library.clear();
                                          }
                                      });
    }

    // =========================================================================
    //  Layout
    // =========================================================================
    void LibraryView::resized()
    {
        const auto sc = ui::KbLookAndFeel::uiScale();
        auto b = getLocalBounds().reduced (2);
        sidebarArea = b.removeFromLeft (juce::jmax (juce::roundToInt (200 * sc), b.getWidth() / 5));
        b.removeFromLeft (4);
        auto searchRow = b.removeFromTop (juce::roundToInt (juce::jlimit (24.0f * sc, 34.0f * sc, (float) getHeight() / 8.0f)));
        searchBox.setBounds (searchRow.reduced (0, 2));
        searchBox.setFont (ui::KbLookAndFeel::labelFont ((float) searchRow.getHeight() * 0.62f, false));
        searchBox.applyFontToAllText (ui::KbLookAndFeel::labelFont ((float) searchRow.getHeight() * 0.62f, false));
        b.removeFromTop (3);
        table.setBounds (b);
        table.setRowHeight (juce::roundToInt (juce::jlimit (24.0f * sc, 34.0f * sc, (float) getHeight() / 9.0f)));
        table.setHeaderHeight (juce::roundToInt (26.0f * sc));

        // Los anchos de columna se guardan "a escala 1": al cambiar la escala se ajustan.
        if (std::abs (sc - columnScale) > 0.01f)
        {
            auto& header = table.getHeader();
            for (int i = 0; i < header.getNumColumns (false); ++i)
            {
                const auto id = header.getColumnIdOfIndex (i, false);
                header.setColumnWidth (id, juce::roundToInt ((float) header.getColumnWidth (id) * sc / columnScale));
            }
            columnScale = sc;
        }

        auto side = sidebarArea;
        const auto clearH = juce::roundToInt (juce::jlimit (26.0f * sc, 36.0f * sc, (float) getHeight() / 9.0f));
        clearButton.setBounds (side.removeFromBottom (clearH + 8).reduced (8, 4));
        sidebar->setBounds (side);
    }

    void LibraryView::paint (juce::Graphics& g)
    {
        g.fillAll (panelDark);
    }

    void LibraryView::paintOverChildren (juce::Graphics& g)
    {
        if (library.size() > 0)
            return;
        const auto hint = library.getSearch().isNotEmpty() ? tr ("library.noResults")
                        : library.getSource().kind == SourceKind::collection ? tr ("library.emptyHint")
                                                                              : tr ("library.emptyList");
        auto area = table.getBounds().toFloat().withTrimmedTop ((float) table.getHeader().getHeight());
        ui::draw::text (g, hint, area.reduced (20.0f), 15.0f * ui::KbLookAndFeel::uiScale(), textSecondary.withAlpha (0.85f), juce::Justification::centred, false);
    }
} // namespace kb::app

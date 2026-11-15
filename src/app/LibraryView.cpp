#include "app/LibraryView.h"

#include "engine/AudioFileFormats.h"
#include "ui/DeckDisplay.h"
#include "ui/DrawPrimitives.h"
#include "ui/KbLookAndFeel.h"
#include "ui/Palette.h"

#include <algorithm>

namespace kb::app
{
    using namespace ui::palette;

    LibraryView::LibraryView (TrackLibrary& lib) : library (lib)
    {
        setOpaque (true);

        table.setColour (juce::ListBox::backgroundColourId, panel);
        table.setColour (juce::ListBox::outlineColourId, panelEdge);
        table.setRowHeight (24);
        table.setMultipleSelectionEnabled (true);
        auto& header = table.getHeader();
        header.setColour (juce::TableHeaderComponent::backgroundColourId, panelLight);
        header.setColour (juce::TableHeaderComponent::textColourId, textSecondary);
        header.setColour (juce::TableHeaderComponent::outlineColourId, panelEdge);
        header.setColour (juce::TableHeaderComponent::highlightColourId, cyan.withAlpha (0.2f));

        const auto colFlags = juce::TableHeaderComponent::defaultFlags;
        header.addColumn ("#",  colNumber,   44,  30, 80, colFlags);
        header.addColumn ("t",  colTitle,    320, 80, 1200, colFlags);
        header.addColumn ("a",  colArtist,   220, 60, 800, colFlags);
        header.addColumn ("b",  colBpm,      80,  50, 140, colFlags);
        header.addColumn ("d",  colDuration, 90,  60, 140, colFlags);
        header.addColumn ("f",  colFormat,   110, 60, 200, colFlags);
        updateHeaderTexts();
        addAndMakeVisible (table);

        addButton.setTextScale (0.7f);
        addButton.onPress = [this] { showAddMenu(); };
        addAndMakeVisible (addButton);

        library.onChanged = [this] { refresh(); };
        I18n::getInstance().addListener (this);
        rebuildOrder();
    }

    LibraryView::~LibraryView()
    {
        library.onChanged = nullptr;
        I18n::getInstance().removeListener (this);
    }

    void LibraryView::updateHeaderTexts()
    {
        auto& h = table.getHeader();
        h.setColumnName (colTitle,    tr ("library.col.title"));
        h.setColumnName (colArtist,   tr ("library.col.artist"));
        h.setColumnName (colBpm,      tr ("library.col.bpm"));
        h.setColumnName (colDuration, tr ("library.col.duration"));
        h.setColumnName (colFormat,   tr ("library.col.format"));
    }

    void LibraryView::languageChanged()
    {
        updateHeaderTexts();
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
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addButton), [safe] (int choice)
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

    // =========================================================================
    //  Orden y datos
    // =========================================================================
    void LibraryView::refresh()
    {
        rebuildOrder();
        table.updateContent();
        repaint();
    }

    juce::String LibraryView::cellText (int item, int column) const
    {
        const auto& it = library[item];
        const auto a = library.analysisFor (item);
        switch (column)
        {
            case colNumber:   return juce::String (item + 1);
            case colTitle:    return it.title;
            case colArtist:   return it.artist;
            case colBpm:
                if (a.has_value() && a->bpm > 0.0) return ui::DeckDisplay::formatBpm (a->bpm);
                return library.isAnalysing (item) ? "..." : "--";
            case colDuration:
                if (a.has_value() && a->durationSeconds > 0.0)
                    return ui::DeckDisplay::formatTime (a->durationSeconds).upToLastOccurrenceOf (".", false, false);
                return library.isAnalysing (item) ? "..." : "--";
            case colFormat:   return it.file.getFileExtension().trimCharactersAtStart (".").toUpperCase();
            default:          break;
        }
        return {};
    }

    void LibraryView::rebuildOrder()
    {
        order.resize ((size_t) library.size());
        for (int i = 0; i < library.size(); ++i)
            order[(size_t) i] = i;

        if (sortColumn <= 0 || sortColumn == colNumber)
        {
            if (! sortForwards)
                std::reverse (order.begin(), order.end());
            return;
        }

        auto key = [this] (int i) -> std::pair<double, juce::String>
        {
            if (sortColumn == colBpm)
            {
                const auto a = library.analysisFor (i);
                return { a.has_value() ? a->bpm : 0.0, {} };
            }
            if (sortColumn == colDuration)
            {
                const auto a = library.analysisFor (i);
                return { a.has_value() ? a->durationSeconds : 0.0, {} };
            }
            return { 0.0, cellText (i, sortColumn).toLowerCase() };
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
        refresh();
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

        auto colour = column == colTitle ? textPrimary : textSecondary;
        if (column == colBpm)
            colour = cyan;
        if (selected)
            colour = colour.brighter (0.3f);

        g.setColour (colour);
        g.setFont (ui::KbLookAndFeel::labelFont ((float) h * 0.52f, column == colTitle || column == colBpm));
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

    void LibraryView::cellClicked (int row, int, const juce::MouseEvent& e)
    {
        if (! e.mods.isPopupMenu())
            return;

        if (! table.isRowSelected (row))
            table.selectRow (row);

        const auto item = itemForRow (row);
        if (item < 0)
            return;

        juce::PopupMenu menu;
        menu.addItem (1, tr ("menu.file.loadDeck", { { "n", "1" } }).trimCharactersAtEnd ("."));
        menu.addItem (2, tr ("menu.file.loadDeck", { { "n", "2" } }).trimCharactersAtEnd ("."));
        menu.addSeparator();
        menu.addItem (3, tr ("library.remove"));

        const auto file = library[item].file;
        juce::Component::SafePointer<LibraryView> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options(), [safe, file] (int choice)
        {
            if (safe == nullptr)
                return;
            if ((choice == 1 || choice == 2) && safe->onLoadRequest)
                safe->onLoadRequest (choice - 1, file);
            else if (choice == 3)
                safe->deleteKeyPressed (0);
        });
    }

    void LibraryView::deleteKeyPressed (int)
    {
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
        const auto item = itemForRow (rows[0]);
        return item >= 0 ? juce::var (juce::String (kDragPrefix) + library[item].file.getFullPathName()) : juce::var();
    }

    // =========================================================================
    //  Layout
    // =========================================================================
    void LibraryView::resized()
    {
        auto b = getLocalBounds().reduced (2);
        sidebar = b.removeFromLeft (juce::jmax (200, b.getWidth() / 5));
        b.removeFromLeft (4);
        table.setBounds (b);

        const auto rowH = juce::jlimit (18, 40, getHeight() / 10);
        addButton.setBounds (juce::Rectangle<int> (rowH + 6, rowH + 6)
                                 .withCentre ({ sidebar.getRight() - rowH / 2 - 8, sidebar.getY() + rowH / 2 + 6 }));
    }

    void LibraryView::paint (juce::Graphics& g)
    {
        g.fillAll (panelDark);

        auto side = sidebar.toFloat();
        g.setColour (panel);
        g.fillRoundedRectangle (side, 4.0f);

        const auto rowH = (float) juce::jlimit (18, 40, getHeight() / 10);
        const auto textH = rowH * 0.55f;
        side = side.reduced (rowH * 0.4f, rowH * 0.2f);
        auto header = side.removeFromTop (rowH + 4.0f);
        header.removeFromRight (rowH + 10.0f);   // botón "+"
        ui::draw::text (g, tr ("library.sidebar"), header, textH * 0.85f, textSecondary, juce::Justification::centredLeft);

        const juce::String items[] = {
            tr ("library.allTracks") + "  (" + juce::String (library.size()) + ")",
            tr ("library.playlists"),
            tr ("library.history") };
        for (int i = 0; i < 3; ++i)
        {
            auto row = side.removeFromTop (rowH);
            if (i == 0)
            {
                g.setColour (cyan.withAlpha (0.15f));
                g.fillRoundedRectangle (row, 3.0f);
            }
            auto icon = row.removeFromLeft (rowH).reduced (rowH * 0.3f);
            g.setColour (i == 0 ? cyan : textSecondary);
            g.fillRoundedRectangle (icon, 2.0f);
            ui::draw::text (g, items[i], row, textH, i == 0 ? textPrimary : textSecondary.withAlpha (0.6f),
                            juce::Justification::centredLeft, false);
        }

        if (library.getNumPendingAnalyses() > 0)
            ui::draw::text (g, tr ("library.analysing", { { "n", juce::String (library.getNumPendingAnalyses()) } }),
                            side.removeFromBottom (rowH), textH * 0.85f, cyan.withAlpha (0.8f),
                            juce::Justification::centredLeft, false);
    }

    void LibraryView::paintOverChildren (juce::Graphics& g)
    {
        if (library.size() > 0)
            return;
        auto area = table.getBounds().toFloat().withTrimmedTop ((float) table.getHeader().getHeight());
        ui::draw::text (g, tr ("library.emptyHint"), area.reduced (20.0f), 14.0f, textSecondary.withAlpha (0.85f),
                        juce::Justification::centred, false);
    }
} // namespace kb::app

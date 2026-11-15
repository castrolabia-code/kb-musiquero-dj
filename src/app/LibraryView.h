#pragma once
// =============================================================================
//  LibraryView: zona de biblioteca (sección 6, punto 4).
//
//    [CARPETAS Y PLAYLISTS  (+)]  |  # | Título | Artista | BPM | Duración | Formato
//
//  * "+" agrega canciones o una carpeta entera a la lista.
//  * Arrastrar una fila sobre un deck, doble click (al deck libre) o click
//    derecho (Deck 1 / Deck 2 / Quitar) para cargar.
//  * Columnas ordenables con click en el encabezado. Supr quita de la lista.
//  El BPM se calcula en segundo plano ("..." mientras tanto).
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "app/I18n.h"
#include "app/TrackLibrary.h"
#include "ui/Controls.h"

#include <functional>
#include <vector>

namespace kb::app
{
    class LibraryView final : public juce::Component,
                              private juce::TableListBoxModel,
                              private I18n::Listener
    {
    public:
        static constexpr const char* kDragPrefix = "kbtrack:";

        explicit LibraryView (TrackLibrary& library);
        ~LibraryView() override;

        /** Pedido de cargar un archivo en un deck (0/1) o en el deck libre (-1). */
        std::function<void (int deck, const juce::File& file)> onLoadRequest;

        /** Tonalidades que están sonando: las compatibles se marcan en verde en la columna Key. */
        void setReferenceKeys (const std::vector<int>& keys);

        /** Abre el selector para agregar canciones o una carpeta. */
        void showAddMenu();

        void paint (juce::Graphics& g) override;
        void paintOverChildren (juce::Graphics& g) override;
        void resized() override;

    private:
        enum Column { colNumber = 1, colTitle, colArtist, colBpm, colDuration, colFormat, colKey };

        // juce::TableListBoxModel
        int  getNumRows() override;
        void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
        void paintCell (juce::Graphics&, int row, int column, int w, int h, bool selected) override;
        void sortOrderChanged (int newSortColumnId, bool isForwards) override;
        void cellDoubleClicked (int row, int column, const juce::MouseEvent&) override;
        void cellClicked (int row, int column, const juce::MouseEvent&) override;
        void deleteKeyPressed (int lastRowSelected) override;
        juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override;

        void languageChanged() override;
        void refresh();
        void rebuildOrder();
        void updateHeaderTexts();
        int  itemForRow (int row) const;
        juce::String cellText (int item, int column) const;

        TrackLibrary& library;
        juce::TableListBox table { "tracks", this };
        ui::GlowButton addButton { "library.add", juce::Colour (0xff19E3F5), true };
        std::vector<int> order;   // fila visible -> índice en la biblioteca
        std::vector<int> referenceKeys;
        int sortColumn = 0;
        bool sortForwards = true;
        juce::Rectangle<int> sidebar;
        std::unique_ptr<juce::FileChooser> chooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryView)
    };
} // namespace kb::app

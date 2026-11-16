#pragma once
// =============================================================================
//  LibraryView (Fase 6): panel inferior de la biblioteca.
//
//    [BIBLIOTECA        +]   [ Buscar...                                   ]
//      Todas las pistas      #  [tapa]  Título  Artista  BPM  Key  Duración  Género  Rating  Formato
//      Preparación
//      Historial
//    [PLAYLISTS         +]
//      Fiesta sábado
//    [BORRAR LISTA]
//
//  Arrastrar filas: a un deck (carga) o a una playlist de la izquierda (agrega).
//  Columnas: ordenar (click), redimensionar, mostrar/ocultar (click derecho en
//  el encabezado). La configuración de columnas se guarda.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "app/I18n.h"
#include "app/TrackLibrary.h"
#include "library/ArtworkCache.h"
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

        /** Ruta del primer archivo de una descripción de arrastre. */
        static juce::File fileFromDrag (const juce::String& description);
        /** IDs de pistas de una descripción de arrastre. */
        static std::vector<juce::int64> idsFromDrag (const juce::String& description);

        explicit LibraryView (TrackLibrary& library);
        ~LibraryView() override;

        /** Pedido de cargar un archivo en un deck (0/1) o en el deck libre (-1). */
        std::function<void (int deck, const juce::File& file)> onLoadRequest;
        /** Se movieron/ocultaron columnas: estado para guardar. */
        std::function<void (const juce::String& state)> onColumnsChanged;

        void setArtworkCache (library::ArtworkCache* cache);
        void restoreColumns (const juce::String& state);
        /** Estado de columnas a escala 1 (independiente del tamaño de la pantalla). */
        juce::String getColumnsState() const;

        /** Tonalidades que están sonando: las compatibles se marcan en verde en la columna Key. */
        void setReferenceKeys (const std::vector<int>& keys);

        /** Navegación desde un controlador MIDI: mueve la selección 'delta' filas. */
        void moveSelection (int delta);
        /** Archivo de la fila seleccionada (o inválido). */
        juce::File selectedFile() const;

        /** Abre el selector para agregar canciones o una carpeta. */
        void showAddMenu();
        /** Android: pide permiso y agrega la música de las carpetas del dispositivo. */
        void scanDeviceMusic();

        void paint (juce::Graphics& g) override;
        void paintOverChildren (juce::Graphics& g) override;
        void resized() override;

    private:
        enum Column { colNumber = 1, colTitle, colArtist, colBpm, colDuration, colFormat, colKey, colArt, colGenre, colRating };

        class Sidebar;

        /** Avisa cuando se mueven, ocultan o redimensionan columnas (para guardarlas). */
        struct HeaderWatcher final : juce::TableHeaderComponent::Listener
        {
            explicit HeaderWatcher (LibraryView& o) : owner (o) {}
            void tableColumnsChanged (juce::TableHeaderComponent*) override { notify(); }
            void tableColumnsResized (juce::TableHeaderComponent*) override { notify(); }
            void tableSortOrderChanged (juce::TableHeaderComponent*) override {}
            void notify() { if (owner.onColumnsChanged) owner.onColumnsChanged (owner.getColumnsState()); }
            LibraryView& owner;
        };
        std::unique_ptr<HeaderWatcher> headerWatcher;

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
        std::vector<juce::int64> selectedIds() const;
        void showTrackMenu (int row);
        void confirmClear();
        void askPlaylistName (const juce::String& title, const juce::String& initial, std::function<void (juce::String)> done);

        TrackLibrary& library;
        library::ArtworkCache* artwork = nullptr;
        juce::TableListBox table { "tracks", this };
        juce::TextEditor searchBox;
        std::unique_ptr<Sidebar> sidebar;
        ui::GlowButton clearButton { "library.clear", juce::Colour (0xffFF4A3D), false };
        std::vector<int> order;   // fila visible -> índice en la vista de la biblioteca
        std::vector<int> referenceKeys;
        int sortColumn = 0;
        bool sortForwards = true;
        juce::Rectangle<int> sidebarArea;
        float columnScale = 1.0f;
        std::unique_ptr<juce::FileChooser> chooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryView)
    };
} // namespace kb::app

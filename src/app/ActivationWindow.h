#pragma once
// =============================================================================
//  ActivationWindow (Fase 9): pide la clave de producto. Sin licencia válida el
//  programa no arranca (sin modo de prueba). Muestra el ID de esta PC, por si
//  la licencia se emite atada a la máquina.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "licensing/License.h"

#include <functional>

namespace kb::app
{
    class ActivationWindow final : public juce::DocumentWindow
    {
    public:
        /** onActivated recibe la clave válida; onQuit si se cierra sin activar. */
        ActivationWindow (const juce::String& previousKey, licensing::Status previousStatus,
                          std::function<void (const juce::String& key, const licensing::Result&)> onActivated,
                          std::function<void()> onQuit);
        void closeButtonPressed() override;

    private:
        std::function<void()> quit;
    };
} // namespace kb::app

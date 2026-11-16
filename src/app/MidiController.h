#pragma once
// =============================================================================
//  MidiController (Fase 8): controladores MIDI conectados a la PC.
//
//  * Detecta solo los dispositivos (cada 2 s): abre las entradas nuevas y las
//    salidas con el mismo nombre (para las luces / LEDs).
//  * Los mensajes llegan en el hilo MIDI; se pasan al hilo de mensajes y ahí se
//    traducen con el mapa (midi::MidiMap) a eventos para la app (onEvent).
//  * MIDI Learn: startLearn (función) y el próximo control que se mueve queda asignado.
//  * LEDs: updateLeds() pregunta el estado de cada función con luz (ledState) y
//    manda solo los cambios.
//  * El mapeo se guarda en midi_mapping.json; se puede importar y exportar.
// =============================================================================
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>

#include "midi/MidiMapping.h"

#include <functional>
#include <map>
#include <set>
#include <memory>
#include <vector>

namespace kb::app
{
    class MidiController final : private juce::MidiInputCallback,
                                 private juce::AsyncUpdater,
                                 private juce::Timer
    {
    public:
        explicit MidiController (juce::File mappingFile);
        ~MidiController() override;

        midi::MidiMap& getMap() noexcept { return map; }
        juce::StringArray getConnectedDevices() const;

        void startLearn (const juce::String& targetId);
        void cancelLearn();
        bool isLearning() const noexcept { return map.isLearning(); }
        juce::String getLearnTarget() const { return map.getLearnTarget(); }
        void removeTarget (const juce::String& targetId);
        void removeAt (int index);
        void clearAll();

        bool save();
        bool importFrom (const juce::File& file);
        bool exportTo (const juce::File& file) const;

        /** Hilo de mensajes. */
        std::function<void (const midi::Event&)> onEvent;
        std::function<void (const juce::String& target, const midi::Binding&)> onLearned;
        std::function<void()> onMappingChanged, onDevicesChanged;
        /** Estado de la luz de una función (lo pregunta updateLeds()). */
        std::function<bool (const juce::String& target)> ledState;

        /** Llamar periódicamente (p. ej. 30 Hz) desde la UI: manda los cambios de LEDs. */
        void updateLeds();

        /** Solo para pruebas / simulación: inyecta un mensaje como si viniera de un dispositivo. */
        void injectForTest (const juce::String& device, const juce::MidiMessage& m);

    private:
        void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;
        void handleAsyncUpdate() override;
        void timerCallback() override;
        void scanDevices();
        void dispatch (const juce::String& device, const juce::MidiMessage& m);
        void send (const juce::String& device, const juce::MidiMessage& m);

        juce::File storage;
        midi::MidiMap map;

        std::vector<std::unique_ptr<juce::MidiInput>> inputs;
        std::map<juce::String, std::unique_ptr<juce::MidiOutput>> outputs;   // por nombre
        juce::StringArray deviceNames;

        juce::CriticalSection pendingLock;
        std::vector<std::pair<juce::String, juce::MidiMessage>> pending;
        std::map<juce::String, bool> ledCache;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiController)
    };
} // namespace kb::app

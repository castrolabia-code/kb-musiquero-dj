#pragma once
// =============================================================================
//  MidiMapping (Fase 8): asignaciones MIDI -> funciones del programa.
//
//  * Target: una función mapeable ("deck1.play", "mixer.crossfader"...), con su
//    tipo: botón, valor absoluto (fader / perilla) o relativo (jog / encoder).
//  * Binding: mensaje MIDI (dispositivo, canal, nota/CC/pitch bend, número) ->
//    target, con su modo de valor.
//  * MIDI Learn: startLearn (target) y el próximo mensaje que llega queda asignado.
//  * JSON: guardar, importar y exportar.
//  * LEDs: el mensaje que hay que mandar al controlador para prender/apagar la
//    luz de un botón.
//
//  Sin dispositivos ni UI: todo se puede probar sin hardware.
// =============================================================================
#include <juce_audio_basics/juce_audio_basics.h>

#include <optional>
#include <vector>

namespace kb::midi
{
    enum class TargetKind { button, absolute, relative };

    struct Target
    {
        juce::String id;
        TargetKind   kind = TargetKind::button;
        bool         hasLed = false;
    };

    /** Todas las funciones mapeables del programa. */
    const std::vector<Target>& allTargets();
    std::optional<Target> findTarget (const juce::String& id);

    enum class MessageType { note, cc, pitchBend };

    enum class ValueMode
    {
        button,           // nota o CC: valor > 0 = apretado
        absolute,         // 0..127 (o 0..16383 en pitch bend) -> 0..1
        relativeSigned,   // CC relativo con centro en 64 (65 = +1, 63 = -1)
        relativeTwos      // CC relativo en complemento a 2 (1 = +1, 127 = -1)
    };

    struct Binding
    {
        juce::String target;
        juce::String device;          // vacío = cualquier dispositivo
        int          channel = 1;     // 1..16
        MessageType  type = MessageType::cc;
        int          number = 0;      // nota o número de CC (no se usa en pitch bend)
        ValueMode    mode = ValueMode::absolute;
        double       sensitivity = 1.0;

        bool matches (const juce::String& dev, const juce::MidiMessage& m) const;
        juce::String describe() const;   // "Ch 1 · CC 7"
    };

    /** Lo que produce un mensaje MIDI para un target. */
    struct Event
    {
        juce::String target;
        TargetKind   kind = TargetKind::button;
        bool         pressed = false;   // botones
        double       value = 0.0;       // absolutos: 0..1
        double       delta = 0.0;       // relativos: pasos (con sensibilidad)
    };

    class MidiMap
    {
    public:
        /** Traduce un mensaje entrante a eventos (puede haber varios targets por mensaje). */
        std::vector<Event> process (const juce::String& device, const juce::MidiMessage& m) const;

        // --- MIDI Learn ------------------------------------------------------------------
        void startLearn (const juce::String& targetId) { learning = targetId; }
        void cancelLearn() { learning.clear(); }
        bool isLearning() const noexcept { return learning.isNotEmpty(); }
        juce::String getLearnTarget() const { return learning; }
        /** Si se está aprendiendo, usa este mensaje para crear la asignación. true si la creó. */
        bool learnFrom (const juce::String& device, const juce::MidiMessage& m);

        // --- Asignaciones -----------------------------------------------------------------
        const std::vector<Binding>& getBindings() const noexcept { return bindings; }
        void add (const Binding& b);
        void removeTarget (const juce::String& targetId);
        void removeAt (int index);
        void clear() { bindings.clear(); }
        std::vector<Binding> bindingsFor (const juce::String& targetId) const;

        // --- LEDs -------------------------------------------------------------------------
        /** Mensajes para prender/apagar las luces de un target (uno por asignación). */
        std::vector<std::pair<juce::String, juce::MidiMessage>> ledMessages (const juce::String& targetId, bool on) const;

        // --- JSON -------------------------------------------------------------------------
        juce::var toJson() const;
        bool fromJson (const juce::var& json);   // reemplaza todo; false si el formato no sirve

    private:
        std::vector<Binding> bindings;
        juce::String learning;
    };
} // namespace kb::midi

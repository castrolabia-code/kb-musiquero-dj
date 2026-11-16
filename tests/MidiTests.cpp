// Pruebas de la Fase 8: asignaciones MIDI, MIDI Learn, valores y LEDs.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "midi/MidiMapping.h"

#include <set>

using namespace kb::midi;
using Catch::Approx;

TEST_CASE ("MIDI Learn: boton, fader y jog quedan asignados con su modo", "[midi]")
{
    MidiMap map;
    map.startLearn ("deck1.play");
    CHECK (map.isLearning());
    CHECK_FALSE (map.learnFrom ("DDJ", juce::MidiMessage::noteOff (1, 11)));   // note off: no sirve para aprender
    REQUIRE (map.learnFrom ("DDJ", juce::MidiMessage::noteOn (1, 11, (juce::uint8) 127)));
    CHECK_FALSE (map.isLearning());

    map.startLearn ("mixer.crossfader");
    REQUIRE (map.learnFrom ("DDJ", juce::MidiMessage::controllerEvent (7, 31, 64)));
    map.startLearn ("deck1.jog");
    REQUIRE (map.learnFrom ("DDJ", juce::MidiMessage::controllerEvent (1, 33, 65)));
    map.startLearn ("deck2.jog");
    REQUIRE (map.learnFrom ("DDJ", juce::MidiMessage::controllerEvent (2, 33, 1)));

    const auto& b = map.getBindings();
    REQUIRE (b.size() == 4);
    CHECK (b[0].mode == ValueMode::button);
    CHECK (b[1].mode == ValueMode::absolute);
    CHECK (b[2].mode == ValueMode::relativeSigned);
    CHECK (b[3].mode == ValueMode::relativeTwos);
}

TEST_CASE ("MIDI: los mensajes producen eventos con el valor correcto", "[midi]")
{
    MidiMap map;
    map.add ({ "deck1.cue", "DDJ", 1, MessageType::note, 12, ValueMode::button, 1.0 });
    map.add ({ "mixer.ch1.volume", "DDJ", 1, MessageType::cc, 19, ValueMode::absolute, 1.0 });
    map.add ({ "deck1.tempo", "DDJ", 1, MessageType::pitchBend, 0, ValueMode::absolute, 1.0 });
    map.add ({ "deck1.jog", "DDJ", 1, MessageType::cc, 33, ValueMode::relativeSigned, 0.5 });
    map.add ({ "library.browse", "DDJ", 7, MessageType::cc, 64, ValueMode::relativeTwos, 1.0 });

    auto e = map.process ("DDJ", juce::MidiMessage::noteOn (1, 12, (juce::uint8) 100));
    REQUIRE (e.size() == 1);
    CHECK (e[0].pressed);
    e = map.process ("DDJ", juce::MidiMessage::noteOff (1, 12));
    REQUIRE (e.size() == 1);
    CHECK_FALSE (e[0].pressed);
    e = map.process ("DDJ", juce::MidiMessage::noteOn (1, 12, (juce::uint8) 0));   // note on con velocidad 0 = soltar
    REQUIRE (e.size() == 1);
    CHECK_FALSE (e[0].pressed);

    e = map.process ("DDJ", juce::MidiMessage::controllerEvent (1, 19, 127));
    REQUIRE (e.size() == 1);
    CHECK (e[0].value == Approx (1.0));
    e = map.process ("DDJ", juce::MidiMessage::pitchWheel (1, 8192));
    REQUIRE (e.size() == 1);
    CHECK (e[0].value == Approx (0.5).margin (0.001));

    e = map.process ("DDJ", juce::MidiMessage::controllerEvent (1, 33, 68));
    REQUIRE (e.size() == 1);
    CHECK (e[0].delta == Approx (2.0));     // (68-64) * 0.5
    e = map.process ("DDJ", juce::MidiMessage::controllerEvent (7, 64, 126));
    REQUIRE (e.size() == 1);
    CHECK (e[0].delta == Approx (-2.0));    // complemento a 2

    // Otro dispositivo, otro canal u otro número: nada.
    CHECK (map.process ("Otro", juce::MidiMessage::controllerEvent (1, 19, 10)).empty());
    CHECK (map.process ("DDJ", juce::MidiMessage::controllerEvent (2, 19, 10)).empty());
    CHECK (map.process ("DDJ", juce::MidiMessage::controllerEvent (1, 20, 10)).empty());
}

TEST_CASE ("MIDI: un mensaje maneja una sola funcion (reasignar reemplaza)", "[midi]")
{
    MidiMap map;
    map.add ({ "deck1.play", "DDJ", 1, MessageType::note, 11, ValueMode::button, 1.0 });
    map.add ({ "deck1.cue", "DDJ", 1, MessageType::note, 11, ValueMode::button, 1.0 });    // mismo mensaje
    REQUIRE (map.getBindings().size() == 1);
    CHECK (map.getBindings()[0].target == "deck1.cue");
    map.add ({ "deck1.cue", "DDJ", 1, MessageType::note, 12, ValueMode::button, 1.0 });    // misma función
    REQUIRE (map.getBindings().size() == 1);
    CHECK (map.getBindings()[0].number == 12);
    map.removeTarget ("deck1.cue");
    CHECK (map.getBindings().empty());
}

TEST_CASE ("MIDI: LEDs con el mismo mensaje que el boton", "[midi]")
{
    MidiMap map;
    map.add ({ "deck1.play", "DDJ", 1, MessageType::note, 11, ValueMode::button, 1.0 });
    map.add ({ "deck2.sync", "DDJ", 2, MessageType::cc, 58, ValueMode::button, 1.0 });
    auto on = map.ledMessages ("deck1.play", true);
    REQUIRE (on.size() == 1);
    CHECK (on[0].first == "DDJ");
    CHECK (on[0].second.isNoteOn());
    CHECK (on[0].second.getNoteNumber() == 11);
    CHECK (on[0].second.getVelocity() == 127);
    auto off = map.ledMessages ("deck2.sync", false);
    REQUIRE (off.size() == 1);
    CHECK (off[0].second.isController());
    CHECK (off[0].second.getControllerValue() == 0);
    CHECK (map.ledMessages ("deck1.cue", true).empty());
}

TEST_CASE ("MIDI: guardar y cargar el mapeo en JSON", "[midi]")
{
    MidiMap map;
    map.add ({ "deck1.play", "DDJ-400", 1, MessageType::note, 11, ValueMode::button, 1.0 });
    map.add ({ "deck2.jog", "DDJ-400", 2, MessageType::cc, 33, ValueMode::relativeSigned, 0.75 });
    map.add ({ "deck1.tempo", "", 1, MessageType::pitchBend, 0, ValueMode::absolute, 1.0 });
    const auto text = juce::JSON::toString (map.toJson());

    MidiMap loaded;
    REQUIRE (loaded.fromJson (juce::JSON::parse (text)));
    REQUIRE (loaded.getBindings().size() == 3);
    CHECK (loaded.getBindings()[1].target == "deck2.jog");
    CHECK (loaded.getBindings()[1].mode == ValueMode::relativeSigned);
    CHECK (loaded.getBindings()[1].sensitivity == Approx (0.75));
    CHECK (loaded.getBindings()[2].type == MessageType::pitchBend);
    CHECK (loaded.getBindings()[2].device.isEmpty());

    CHECK_FALSE (loaded.fromJson (juce::JSON::parse ("{\"algo\": 1}")));   // formato ajeno: no toca nada
    CHECK (loaded.getBindings().size() == 3);
}

TEST_CASE ("MIDI: todas las funciones tienen id unico", "[midi]")
{
    std::set<juce::String> ids;
    for (const auto& t : allTargets())
        CHECK (ids.insert (t.id).second);
    CHECK (allTargets().size() > 80);
}

#include "app/MidiController.h"

TEST_CASE ("MIDI: el controlador aprende, despacha eventos y guarda el mapeo", "[midi][app]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("kb_midi", "");
    dir.createDirectory();
    const auto file = dir.getChildFile ("midi_mapping.json");
    {
        kb::app::MidiController midi (file);
        std::vector<Event> got;
        juce::String learned;
        midi.onEvent = [&got] (const Event& e) { got.push_back (e); };
        midi.onLearned = [&learned] (const juce::String& t, const Binding&) { learned = t; };

        midi.startLearn ("deck2.sync");
        midi.injectForTest ("Controlador", juce::MidiMessage::noteOn (2, 60, (juce::uint8) 127));
        CHECK (learned == "deck2.sync");
        CHECK (got.empty());   // mientras aprende no dispara la función

        midi.injectForTest ("Controlador", juce::MidiMessage::noteOn (2, 60, (juce::uint8) 127));
        midi.injectForTest ("Controlador", juce::MidiMessage::noteOff (2, 60));
        REQUIRE (got.size() == 2);
        CHECK (got[0].target == "deck2.sync");
        CHECK (got[0].pressed);
        CHECK_FALSE (got[1].pressed);

        midi.ledState = [] (const juce::String&) { return true; };
        midi.updateLeds();   // sin salidas abiertas: no debe fallar
    }
    {
        kb::app::MidiController reloaded (file);   // el mapeo quedó guardado
        REQUIRE (reloaded.getMap().getBindings().size() == 1);
        CHECK (reloaded.getMap().getBindings()[0].target == "deck2.sync");
        const auto exported = dir.getChildFile ("export.json");
        CHECK (reloaded.exportTo (exported));
        reloaded.clearAll();
        CHECK (reloaded.getMap().getBindings().empty());
        CHECK (reloaded.importFrom (exported));
        CHECK (reloaded.getMap().getBindings().size() == 1);
    }
    dir.deleteRecursively();
}

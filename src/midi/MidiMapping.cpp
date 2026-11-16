#include "midi/MidiMapping.h"

#include <algorithm>

namespace kb::midi
{
    // =========================================================================
    //  Funciones mapeables
    // =========================================================================
    const std::vector<Target>& allTargets()
    {
        static const std::vector<Target> targets = []
        {
            std::vector<Target> t;
            auto btn = [&t] (const juce::String& id, bool led = true) { t.push_back ({ id, TargetKind::button, led }); };
            auto abs = [&t] (const juce::String& id) { t.push_back ({ id, TargetKind::absolute, false }); };
            auto rel = [&t] (const juce::String& id) { t.push_back ({ id, TargetKind::relative, false }); };

            for (int d = 1; d <= 2; ++d)
            {
                const auto p = "deck" + juce::String (d) + ".";
                for (const auto* b : { "play", "pause", "playPause", "cue", "sync", "keyLock", "keyDown", "keyUp", "slip",
                                       "vinyl", "shift", "modeHotCue", "modeFx", "modeSampler", "modeLoop",
                                       "loopIn", "loopOut", "loopHalve", "loopDouble", "pad1", "pad2", "pad3", "pad4",
                                       "jogTouch", "load", "tempoRange" })
                    btn (p + b, juce::String (b) != "load" && juce::String (b) != "jogTouch");
                abs (p + "tempo");
                rel (p + "jog");
                rel (p + "tempoNudge");
            }
            for (int c = 1; c <= 2; ++c)
            {
                const auto p = "mixer.ch" + juce::String (c) + ".";
                for (const auto* a : { "volume", "gain", "high", "mid", "low", "filter" })
                    abs (p + a);
                btn (p + "cue");
            }
            for (const auto* a : { "mixer.crossfader", "mixer.master", "mixer.phones", "mixer.cueMix", "mixer.mixTime" })
                abs (a);
            btn ("mixer.split");
            btn ("mixer.autoMix");
            for (int f = 1; f <= 2; ++f)
            {
                const auto p = "fx" + juce::String (f) + ".";
                btn (p + "on");
                abs (p + "wet");
                abs (p + "param");
                btn (p + "beatsDown", false);
                btn (p + "beatsUp", false);
            }
            rel ("library.browse");
            btn ("library.largeView");
            return t;
        }();
        return targets;
    }

    std::optional<Target> findTarget (const juce::String& id)
    {
        for (const auto& t : allTargets())
            if (t.id == id)
                return t;
        return std::nullopt;
    }

    // =========================================================================
    //  Binding
    // =========================================================================
    bool Binding::matches (const juce::String& dev, const juce::MidiMessage& m) const
    {
        if (device.isNotEmpty() && device != dev)
            return false;
        if (m.getChannel() != channel)
            return false;
        switch (type)
        {
            case MessageType::note:      return (m.isNoteOn (true) || m.isNoteOff()) && m.getNoteNumber() == number;
            case MessageType::cc:        return m.isController() && m.getControllerNumber() == number;
            case MessageType::pitchBend: return m.isPitchWheel();
        }
        return false;
    }

    juce::String Binding::describe() const
    {
        juce::String s = "Ch " + juce::String (channel) + " · ";
        switch (type)
        {
            case MessageType::note:      s << "Note " << juce::MidiMessage::getMidiNoteName (number, true, true, 3); break;
            case MessageType::cc:        s << "CC " << number; break;
            case MessageType::pitchBend: s << "Pitch bend"; break;
        }
        return s;
    }

    // =========================================================================
    //  Entrada
    // =========================================================================
    std::vector<Event> MidiMap::process (const juce::String& device, const juce::MidiMessage& m) const
    {
        std::vector<Event> out;
        for (const auto& b : bindings)
        {
            if (! b.matches (device, m))
                continue;
            const auto target = findTarget (b.target);
            if (! target.has_value())
                continue;

            Event e;
            e.target = b.target;
            e.kind = target->kind;
            const auto raw = m.isPitchWheel() ? m.getPitchWheelValue()
                           : m.isController() ? m.getControllerValue()
                           : m.isNoteOn (false) ? (int) m.getVelocity() : 0;
            switch (b.mode)
            {
                case ValueMode::button:
                    e.pressed = raw > 0;
                    e.value = e.pressed ? 1.0 : 0.0;
                    break;
                case ValueMode::absolute:
                    e.value = m.isPitchWheel() ? raw / 16383.0 : raw / 127.0;
                    e.pressed = e.value > 0.5;
                    break;
                case ValueMode::relativeSigned:
                    e.delta = (raw - 64) * b.sensitivity;
                    break;
                case ValueMode::relativeTwos:
                    e.delta = (raw < 64 ? raw : raw - 128) * b.sensitivity;
                    break;
            }
            out.push_back (e);
        }
        return out;
    }

    bool MidiMap::learnFrom (const juce::String& device, const juce::MidiMessage& m)
    {
        if (! isLearning())
            return false;
        const auto target = findTarget (learning);
        if (! target.has_value())
        {
            learning.clear();
            return false;
        }
        // Solo mensajes que sirven: nota encendida, CC o pitch bend.
        if (! (m.isNoteOn (false) || m.isController() || m.isPitchWheel()))
            return false;

        Binding b;
        b.target = learning;
        b.device = device;
        b.channel = m.getChannel();
        if (m.isNoteOn (false))      { b.type = MessageType::note; b.number = m.getNoteNumber(); }
        else if (m.isController())   { b.type = MessageType::cc;   b.number = m.getControllerNumber(); }
        else                         { b.type = MessageType::pitchBend; }

        switch (target->kind)
        {
            case TargetKind::button:   b.mode = ValueMode::button; break;
            case TargetKind::absolute: b.mode = ValueMode::absolute; break;
            case TargetKind::relative:
                // Jog / encoder: casi todos los controladores usan "centro en 64".
                // Un primer valor chico (1, 2...) indica complemento a 2.
                b.mode = (m.isController() && m.getControllerValue() < 16) ? ValueMode::relativeTwos : ValueMode::relativeSigned;
                break;
        }
        add (b);
        learning.clear();
        return true;
    }

    // =========================================================================
    //  Asignaciones
    // =========================================================================
    void MidiMap::add (const Binding& b)
    {
        // Un mensaje maneja una sola función; una función puede tener un solo mensaje por dispositivo.
        bindings.erase (std::remove_if (bindings.begin(), bindings.end(), [&b] (const Binding& o)
        {
            const bool sameMessage = o.device == b.device && o.channel == b.channel && o.type == b.type
                                     && (o.type == MessageType::pitchBend || o.number == b.number);
            const bool sameTarget = o.target == b.target && o.device == b.device;
            return sameMessage || sameTarget;
        }), bindings.end());
        bindings.push_back (b);
    }

    void MidiMap::removeTarget (const juce::String& id)
    {
        bindings.erase (std::remove_if (bindings.begin(), bindings.end(), [&id] (const Binding& b) { return b.target == id; }),
                        bindings.end());
    }

    void MidiMap::removeAt (int index)
    {
        if (juce::isPositiveAndBelow (index, (int) bindings.size()))
            bindings.erase (bindings.begin() + index);
    }

    std::vector<Binding> MidiMap::bindingsFor (const juce::String& id) const
    {
        std::vector<Binding> out;
        for (const auto& b : bindings)
            if (b.target == id)
                out.push_back (b);
        return out;
    }

    // =========================================================================
    //  LEDs
    // =========================================================================
    std::vector<std::pair<juce::String, juce::MidiMessage>> MidiMap::ledMessages (const juce::String& id, bool on) const
    {
        std::vector<std::pair<juce::String, juce::MidiMessage>> out;
        for (const auto& b : bindings)
        {
            if (b.target != id || b.mode != ValueMode::button)
                continue;
            const auto v = (juce::uint8) (on ? 127 : 0);
            if (b.type == MessageType::note)
                out.emplace_back (b.device, juce::MidiMessage::noteOn (b.channel, b.number, v));
            else if (b.type == MessageType::cc)
                out.emplace_back (b.device, juce::MidiMessage::controllerEvent (b.channel, b.number, v));
        }
        return out;
    }

    // =========================================================================
    //  JSON
    // =========================================================================
    static const char* typeName (MessageType t)
    {
        switch (t) { case MessageType::note: return "note"; case MessageType::cc: return "cc"; case MessageType::pitchBend: return "pitchbend"; }
        return "cc";
    }
    static const char* modeName (ValueMode m)
    {
        switch (m)
        {
            case ValueMode::button: return "button";
            case ValueMode::absolute: return "absolute";
            case ValueMode::relativeSigned: return "relative64";
            case ValueMode::relativeTwos: return "relative2s";
        }
        return "absolute";
    }

    juce::var MidiMap::toJson() const
    {
        juce::Array<juce::var> list;
        for (const auto& b : bindings)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("target", b.target);
            o->setProperty ("device", b.device);
            o->setProperty ("channel", b.channel);
            o->setProperty ("type", typeName (b.type));
            o->setProperty ("number", b.number);
            o->setProperty ("mode", modeName (b.mode));
            o->setProperty ("sensitivity", b.sensitivity);
            list.add (juce::var (o));
        }
        auto* root = new juce::DynamicObject();
        root->setProperty ("format", "kb-musiquero-midi");
        root->setProperty ("version", 1);
        root->setProperty ("bindings", list);
        return juce::var (root);
    }

    bool MidiMap::fromJson (const juce::var& json)
    {
        const auto* list = json.getProperty ("bindings", {}).getArray();
        if (list == nullptr || json.getProperty ("format", {}).toString() != "kb-musiquero-midi")
            return false;

        std::vector<Binding> loaded;
        for (const auto& v : *list)
        {
            Binding b;
            b.target = v.getProperty ("target", {}).toString();
            if (! findTarget (b.target).has_value())
                continue;   // función que ya no existe: se ignora
            b.device = v.getProperty ("device", {}).toString();
            b.channel = juce::jlimit (1, 16, (int) v.getProperty ("channel", 1));
            const auto type = v.getProperty ("type", "cc").toString();
            b.type = type == "note" ? MessageType::note : type == "pitchbend" ? MessageType::pitchBend : MessageType::cc;
            b.number = juce::jlimit (0, 127, (int) v.getProperty ("number", 0));
            const auto mode = v.getProperty ("mode", "absolute").toString();
            b.mode = mode == "button" ? ValueMode::button : mode == "relative64" ? ValueMode::relativeSigned
                   : mode == "relative2s" ? ValueMode::relativeTwos : ValueMode::absolute;
            b.sensitivity = (double) v.getProperty ("sensitivity", 1.0);
            loaded.push_back (b);
        }
        bindings = std::move (loaded);
        return true;
    }
} // namespace kb::midi

#include "app/MidiController.h"

namespace kb::app
{
    MidiController::MidiController (juce::File f) : storage (std::move (f))
    {
        if (storage.existsAsFile())
            map.fromJson (juce::JSON::parse (storage));
        scanDevices();
        startTimer (2000);
    }

    MidiController::~MidiController()
    {
        stopTimer();
        cancelPendingUpdate();
        for (auto& in : inputs)
            in->stop();
        // Apagar las luces al cerrar.
        for (const auto& t : midi::allTargets())
            if (t.hasLed)
                for (const auto& [dev, msg] : map.ledMessages (t.id, false))
                    send (dev, msg);
        save();
    }

    // =========================================================================
    //  Dispositivos
    // =========================================================================
    void MidiController::timerCallback() { scanDevices(); }

    void MidiController::scanDevices()
    {
        const auto available = juce::MidiInput::getAvailableDevices();
        juce::StringArray names;
        for (const auto& d : available)
            names.add (d.name);
        if (names == deviceNames)
            return;

        // Cambió la lista: cerrar lo que ya no está y abrir lo nuevo.
        for (auto& in : inputs)
            in->stop();
        inputs.clear();
        outputs.clear();
        for (const auto& d : available)
        {
            if (auto in = juce::MidiInput::openDevice (d.identifier, this))
            {
                in->start();
                inputs.push_back (std::move (in));
            }
        }
        for (const auto& d : juce::MidiOutput::getAvailableDevices())
            if (names.contains (d.name))
                if (auto out = juce::MidiOutput::openDevice (d.identifier))
                    outputs[d.name] = std::move (out);

        deviceNames = names;
        ledCache.clear();   // reenviar el estado de las luces al controlador nuevo
        juce::Logger::writeToLog ("MIDI: " + (names.isEmpty() ? juce::String ("sin dispositivos") : names.joinIntoString (", ")));
        if (onDevicesChanged)
            onDevicesChanged();
    }

    juce::StringArray MidiController::getConnectedDevices() const { return deviceNames; }

    // =========================================================================
    //  Entrada (hilo MIDI -> hilo de mensajes)
    // =========================================================================
    void MidiController::handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& m)
    {
        if (m.isActiveSense() || m.isMidiClock() || m.isSysEx())
            return;
        {
            const juce::ScopedLock sl (pendingLock);
            if (pending.size() < 4096)
                pending.emplace_back (source != nullptr ? source->getName() : juce::String(), m);
        }
        triggerAsyncUpdate();
    }

    void MidiController::injectForTest (const juce::String& device, const juce::MidiMessage& m)
    {
        dispatch (device, m);
    }

    void MidiController::handleAsyncUpdate()
    {
        std::vector<std::pair<juce::String, juce::MidiMessage>> now;
        {
            const juce::ScopedLock sl (pendingLock);
            now.swap (pending);
        }
        for (const auto& [dev, msg] : now)
            dispatch (dev, msg);
    }

    void MidiController::dispatch (const juce::String& device, const juce::MidiMessage& m)
    {
        if (map.isLearning())
        {
            const auto target = map.getLearnTarget();
            if (map.learnFrom (device, m))
            {
                save();
                ledCache.erase (target);
                if (onLearned)
                    if (const auto b = map.bindingsFor (target); ! b.empty())
                        onLearned (target, b.back());
                if (onMappingChanged)
                    onMappingChanged();
            }
            return;   // mientras se aprende, el control no hace nada
        }
        for (const auto& e : map.process (device, m))
            if (onEvent)
                onEvent (e);
    }

    // =========================================================================
    //  Learn y mapeo
    // =========================================================================
    void MidiController::startLearn (const juce::String& id) { map.startLearn (id); if (onMappingChanged) onMappingChanged(); }
    void MidiController::cancelLearn() { map.cancelLearn(); if (onMappingChanged) onMappingChanged(); }

    void MidiController::removeTarget (const juce::String& id)
    {
        for (const auto& [dev, msg] : map.ledMessages (id, false))
            send (dev, msg);
        map.removeTarget (id);
        save();
        if (onMappingChanged) onMappingChanged();
    }

    void MidiController::removeAt (int index)
    {
        map.removeAt (index);
        save();
        if (onMappingChanged) onMappingChanged();
    }

    void MidiController::clearAll()
    {
        map.clear();
        ledCache.clear();
        save();
        if (onMappingChanged) onMappingChanged();
    }

    bool MidiController::save()
    {
        storage.getParentDirectory().createDirectory();
        return storage.replaceWithText (juce::JSON::toString (map.toJson()));
    }

    bool MidiController::importFrom (const juce::File& file)
    {
        midi::MidiMap loaded;
        if (! loaded.fromJson (juce::JSON::parse (file)))
            return false;
        map = loaded;
        ledCache.clear();
        save();
        if (onMappingChanged) onMappingChanged();
        return true;
    }

    bool MidiController::exportTo (const juce::File& file) const
    {
        return file.replaceWithText (juce::JSON::toString (map.toJson()));
    }

    // =========================================================================
    //  LEDs
    // =========================================================================
    void MidiController::send (const juce::String& device, const juce::MidiMessage& m)
    {
        if (device.isEmpty())
        {
            for (auto& [name, out] : outputs)
                out->sendMessageNow (m);
            return;
        }
        if (const auto it = outputs.find (device); it != outputs.end())
            it->second->sendMessageNow (m);
    }

    void MidiController::updateLeds()
    {
        if (! ledState || map.getBindings().empty())
            return;
        std::set<juce::String> done;
        for (const auto& b : map.getBindings())
        {
            if (! done.insert (b.target).second)
                continue;
            const auto target = midi::findTarget (b.target);
            if (! target.has_value() || ! target->hasLed)
                continue;
            const auto on = ledState (b.target);
            const auto it = ledCache.find (b.target);
            if (it != ledCache.end() && it->second == on)
                continue;
            ledCache[b.target] = on;
            for (const auto& [dev, msg] : map.ledMessages (b.target, on))
                send (dev, msg);
        }
    }
} // namespace kb::app

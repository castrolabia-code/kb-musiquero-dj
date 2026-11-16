# =============================================================================
#  kb_midi: asignaciones MIDI, MIDI Learn y LEDs (Fase 8). Sin dispositivos ni
#  UI (los abre la app): se prueba sin hardware.
# =============================================================================
add_library(kb_midi STATIC
    MidiMapping.h  MidiMapping.cpp)
add_library(kb::midi ALIAS kb_midi)

target_include_directories(kb_midi PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(kb_midi PUBLIC kb::juce)
set_target_properties(kb_midi PROPERTIES FOLDER "KB")

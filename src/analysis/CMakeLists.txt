# =============================================================================
#  kb_analysis: análisis de pistas (BPM y grilla; luego Key, waveform RGB y
#  ganancia automática). Sin UI ni dispositivos: corre en hilos de fondo y es
#  portable a Android, igual que el motor.
# =============================================================================
add_library(kb_analysis STATIC
    BpmDetector.h    BpmDetector.cpp
    KeyDetector.h    KeyDetector.cpp
    TrackAnalyser.h  TrackAnalyser.cpp)
add_library(kb::analysis ALIAS kb_analysis)

target_include_directories(kb_analysis PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(kb_analysis PUBLIC kb::juce kb::engine)
set_target_properties(kb_analysis PROPERTIES FOLDER "KB")

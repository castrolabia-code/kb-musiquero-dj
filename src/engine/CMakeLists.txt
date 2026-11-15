# =============================================================================
#  kb_engine: motor de audio en tiempo real.
#  Sin dependencias de UI ni de dispositivos (ver cmake/CheckEngineIsolation.cmake)
#  para poder reutilizarlo en Android.
# =============================================================================
add_library(kb_engine STATIC
    AudioEngine.h
    AudioEngine.cpp
    RealtimeChecks.h)
add_library(kb::engine ALIAS kb_engine)

target_include_directories(kb_engine PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(kb_engine PUBLIC kb::juce)
set_target_properties(kb_engine PROPERTIES FOLDER "KB")

# =============================================================================
#  kb_engine: motor de audio en tiempo real + carga de pistas.
#  Sin dependencias de UI ni de dispositivos (ver cmake/CheckEngineIsolation.cmake)
#  para poder reutilizarlo en Android.
# =============================================================================
add_library(kb_engine STATIC
    AudioEngine.h       AudioEngine.cpp
    Deck.h              Deck.cpp
    TrackBuffer.h       TrackBuffer.cpp
    TrackLoader.h       TrackLoader.cpp
    AudioFileFormats.h  AudioFileFormats.cpp
    LockFreeQueue.h
    RealtimeChecks.h)
add_library(kb::engine ALIAS kb_engine)

if(WIN32)
    # M4A/AAC con Windows Media Foundation (componentes del sistema operativo).
    target_sources(kb_engine PRIVATE
        formats/MediaFoundationReader.h
        formats/MediaFoundationReader.cpp)
    target_link_libraries(kb_engine PRIVATE mfplat mfreadwrite mfuuid ole32)
endif()

target_include_directories(kb_engine PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(kb_engine
    PUBLIC  kb::juce
    PRIVATE kb::r8brain)
set_target_properties(kb_engine PROPERTIES FOLDER "KB")

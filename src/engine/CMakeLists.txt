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
    ChannelStrip.h      ChannelStrip.cpp
    MeterSource.h
    MeterBallistics.h
    dsp/Biquad.h
    dsp/ThreeBandIsolator.h  dsp/ThreeBandIsolator.cpp
    dsp/DjFilter.h           dsp/DjFilter.cpp
    dsp/Limiter.h            dsp/Limiter.cpp
    LockFreeQueue.h
    RealtimeChecks.h)
add_library(kb::engine ALIAS kb_engine)

if(WIN32)
    # M4A/AAC con Windows Media Foundation (componentes del sistema operativo).
    target_sources(kb_engine PRIVATE
        formats/MediaFoundationReader.h
        formats/MediaFoundationReader.cpp)
    target_link_libraries(kb_engine PRIVATE mfplat mfreadwrite mfuuid ole32)
    # r8brain incluye <windows.h>: sin NOMINMAX sus macros min/max rompen std::min.
    target_compile_definitions(kb_engine PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

target_include_directories(kb_engine PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(kb_engine
    PUBLIC  kb::juce
    PRIVATE kb::r8brain)
set_target_properties(kb_engine PROPERTIES FOLDER "KB")

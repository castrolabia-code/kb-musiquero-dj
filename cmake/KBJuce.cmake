# =============================================================================
#  kb_juce: los módulos de JUCE compilados UNA sola vez en una librería
#  estática, compartida por la app, el motor y las pruebas.
#
#  Los módulos de JUCE son targets INTERFACE que compilan sus fuentes dentro de
#  cada consumidor. Si dos librerías propias los enlazaran por separado, los
#  símbolos se duplicarían. Por eso se centralizan acá y se reexportan sus
#  definiciones e include dirs.
#
#  Regla de arquitectura: src/engine y src/analysis solo pueden incluir módulos
#  de audio "puros" (core, audio_basics, audio_formats, dsp...). Lo verifica la
#  prueba 'engine_isolation' (cmake/CheckEngineIsolation.cmake), para que el
#  motor se pueda portar a Android sin arrastrar la interfaz.
# =============================================================================
add_library(kb_juce STATIC)
add_library(kb::juce ALIAS kb_juce)

target_link_libraries(kb_juce
    PRIVATE
        juce::juce_core
        juce::juce_events
        juce::juce_data_structures
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

target_compile_definitions(kb_juce
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_DISABLE_JUCE_VERSION_PRINTING=1
        JUCE_USE_MP3AUDIOFORMAT=1        # patentes de MP3 vencidas (2017)
        JUCE_USE_WINDOWS_MEDIA_FORMAT=0  # AAC/M4A irá por Media Foundation propio (Fase 1)
        JUCE_DIRECTSOUND=0               # solo WASAPI (y ASIO opcional) en Windows
        JUCE_WASAPI=1
        JUCE_JACK=0
        JUCE_PLUGINHOST_VST3=0
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_LADSPA=0
    INTERFACE
        $<TARGET_PROPERTY:kb_juce,COMPILE_DEFINITIONS>)

target_include_directories(kb_juce
    INTERFACE
        $<TARGET_PROPERTY:kb_juce,INCLUDE_DIRECTORIES>)

set_target_properties(kb_juce PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
    VISIBILITY_INLINES_HIDDEN TRUE
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    FOLDER "ThirdParty")

# Opciones de compilador comunes a todo el proyecto.
if(MSVC)
    # /utf-8: las fuentes son UTF-8.  /permissive-: C++ estándar estricto.
    target_compile_options(kb_juce PUBLIC /utf-8 /permissive- /Zc:__cplusplus)
endif()

# --- ASIO (opcional) ------------------------------------------------------------
# El SDK de ASIO NO se incluye en el repositorio. Para uso comercial cerrado hay
# que firmar el acuerdo de licencia propietario de Steinberg (gratuito); la otra
# opción del SDK es GPLv3, incompatible con un producto cerrado.
if(KB_ENABLE_ASIO)
    if(NOT WIN32)
        message(FATAL_ERROR "KB_ENABLE_ASIO solo aplica a Windows.")
    endif()
    if(NOT EXISTS "${KB_ASIO_SDK_DIR}/common/iasiodrv.h")
        message(FATAL_ERROR
            "KB_ENABLE_ASIO=ON pero no se encontró el ASIO SDK.\n"
            "Pasá -DKB_ASIO_SDK_DIR=<ruta> apuntando a la carpeta que contiene 'common/iasiodrv.h'.")
    endif()
    target_compile_definitions(kb_juce PUBLIC JUCE_ASIO=1)
    target_include_directories(kb_juce PUBLIC "${KB_ASIO_SDK_DIR}/common")
    message(STATUS "KB: ASIO habilitado (${KB_ASIO_SDK_DIR})")
else()
    target_compile_definitions(kb_juce PUBLIC JUCE_ASIO=0)
endif()

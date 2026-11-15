# =============================================================================
#  Prueba de arquitectura: el motor de audio (src/engine) y el análisis
#  (src/analysis) no pueden depender de la interfaz ni de dispositivos.
#  Así el motor se reutiliza tal cual en Android (Fase 10).
#
#  Uso: cmake -DKB_SRC_DIR=<repo>/src -P CheckEngineIsolation.cmake
# =============================================================================
if(NOT KB_SRC_DIR)
    message(FATAL_ERROR "Falta -DKB_SRC_DIR")
endif()

set(_forbidden "juce_gui_basics|juce_gui_extra|juce_graphics|juce_opengl|juce_audio_devices|juce_audio_utils|juce_audio_processors")
set(_violations "")

foreach(_dir engine analysis)
    file(GLOB_RECURSE _files
        "${KB_SRC_DIR}/${_dir}/*.h" "${KB_SRC_DIR}/${_dir}/*.hpp"
        "${KB_SRC_DIR}/${_dir}/*.cpp")
    foreach(_f IN LISTS _files)
        file(STRINGS "${_f}" _lines REGEX "^[ \t]*#[ \t]*include")
        foreach(_l IN LISTS _lines)
            if(_l MATCHES "(${_forbidden})" OR _l MATCHES "\"\\.\\./(ui|app|midi|library)/")
                string(APPEND _violations "  ${_f}: ${_l}\n")
            endif()
        endforeach()
    endforeach()
endforeach()

if(_violations)
    message(FATAL_ERROR "El motor/análisis incluye dependencias prohibidas:\n${_violations}")
endif()
message(STATUS "engine_isolation: OK")

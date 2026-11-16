# =============================================================================
#  KB - MUSIQUERO DJ  ·  by Castro Labs
#  Build raíz (CMake + JUCE 8, sin Projucer)
# =============================================================================
cmake_minimum_required(VERSION 3.25)

# La versión del producto vive SOLO acá. La app, el título de la ventana y
# (en la Fase 9) el instalador la leen desde este valor.
project(KBMusiqueroDJ
    VERSION 0.12.0
    DESCRIPTION "KB - MUSIQUERO DJ by Castro Labs"
    LANGUAGES C CXX)

set(KB_PRODUCT_NAME   "KB Musiquero DJ")
set(KB_COMPANY_NAME   "Castro Labs")
set(KB_BUNDLE_ID      "com.castrolabs.kbmusiquerodj")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# --- Opciones -----------------------------------------------------------------
option(KB_BUILD_TESTS  "Compilar las pruebas unitarias (Catch2)" ON)
option(KB_ENABLE_ASIO  "Habilitar ASIO en Windows (requiere el SDK de Steinberg y su acuerdo de licencia)" OFF)
set(KB_ASIO_SDK_DIR "" CACHE PATH "Ruta al ASIO SDK de Steinberg (la carpeta que contiene 'common/')")

# --- Dependencias y librería compartida de JUCE --------------------------------
include(cmake/Dependencies.cmake)
include(cmake/KBJuce.cmake)

# --- Recursos embebidos (traducciones; más adelante fuentes e imágenes) --------
add_subdirectory(assets)

# --- Módulos ------------------------------------------------------------------
add_subdirectory(src/engine)     # motor de audio: SIN dependencias de UI
add_subdirectory(src/analysis)   # BPM, grilla (y luego Key, waveform): SIN UI
add_subdirectory(src/library)    # biblioteca: tags, carátulas, SQLite (Fase 6)
add_subdirectory(src/midi)       # MIDI: asignaciones, learn, LEDs (Fase 8)
add_subdirectory(src/licensing)  # licencias (Fase 9)
add_subdirectory(src/app)        # aplicación de escritorio

# --- Pruebas ------------------------------------------------------------------
if(KB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

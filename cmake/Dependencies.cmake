# =============================================================================
#  Dependencias externas (descargadas con FetchContent, versiones fijas).
#  Cada dependencia está registrada con su licencia en docs/LICENCIAS.md.
#
#  Para trabajar sin red o con un clon local, se puede apuntar a una copia:
#     -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/src/JUCE
#     -DFETCHCONTENT_SOURCE_DIR_CATCH2=C:/src/Catch2
#     -DFETCHCONTENT_SOURCE_DIR_R8BRAIN=C:/src/r8brain-free-src
# =============================================================================
include(FetchContent)

# --- JUCE 8 -------------------------------------------------------------------
# Licencia: AGPLv3 / comercial. Para vender se necesita una licencia comercial
# de JUCE (ver docs/LICENCIAS.md).
# Tag 8.0.15 = commit 91ad83ae34a81e0833b1a2b0866f54846370ae53
set(KB_JUCE_VERSION "8.0.15")
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${KB_JUCE_VERSION}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)

set(JUCE_BUILD_EXTRAS   OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(JUCE)

# --- r8brain-free-src (conversión de sample rate de alta calidad) --------------
# Licencia: MIT. Tag version-6.5 = commit 3c930bf6825c0cfea4a813210c83b1a650c820b5
# Se usa en el hilo de carga (nunca en el callback de audio): cada pista se
# convierte al sample rate del dispositivo al cargarla.
FetchContent_Declare(r8brain
    GIT_REPOSITORY https://github.com/avaneev/r8brain-free-src.git
    GIT_TAG        version-6.5
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)
FetchContent_MakeAvailable(r8brain)

add_library(kb_r8brain STATIC "${r8brain_SOURCE_DIR}/r8bbase.cpp")
add_library(kb::r8brain ALIAS kb_r8brain)
target_include_directories(kb_r8brain SYSTEM PUBLIC "${r8brain_SOURCE_DIR}")
set_target_properties(kb_r8brain PROPERTIES POSITION_INDEPENDENT_CODE TRUE FOLDER "ThirdParty")
if(WIN32)
    target_compile_definitions(kb_r8brain PUBLIC NOMINMAX)
endif()
if(MSVC)
    target_compile_options(kb_r8brain PRIVATE /W0)
else()
    target_compile_options(kb_r8brain PRIVATE -w)
endif()

# --- Catch2 v3 (solo pruebas, no se distribuye) --------------------------------
# Licencia: BSL-1.0
if(KB_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.16.1
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()

# =============================================================================
#  Dependencias externas (descargadas con FetchContent, versiones fijas).
#  Cada dependencia está registrada con su licencia en docs/LICENCIAS.md.
#
#  Para trabajar sin red o con un clon local, se puede apuntar a una copia:
#     -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/src/JUCE
#     -DFETCHCONTENT_SOURCE_DIR_CATCH2=C:/src/Catch2
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

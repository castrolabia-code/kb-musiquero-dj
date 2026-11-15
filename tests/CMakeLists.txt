# =============================================================================
#  Pruebas unitarias (Catch2 v3). Se ejecutan con: ctest --preset <preset>
# =============================================================================
add_executable(kb_tests
    TestMain.cpp
    TestHelpers.h
    AllocationCounter.cpp
    EngineTests.cpp
    LoaderTests.cpp
    MixerTests.cpp
    AnalysisTests.cpp
    SyncTests.cpp
    AppCoreTests.cpp
    I18nTests.cpp)

target_link_libraries(kb_tests PRIVATE
    kb::engine
    kb::analysis
    kb::app_core
    kb::juce
    Catch2::Catch2)

target_compile_definitions(kb_tests PRIVATE
    KB_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

set_target_properties(kb_tests PROPERTIES FOLDER "KB")

include(Catch)
catch_discover_tests(kb_tests DISCOVERY_MODE PRE_TEST)

# Prueba de arquitectura: el motor no depende de la UI.
add_test(NAME engine_isolation
    COMMAND "${CMAKE_COMMAND}" -DKB_SRC_DIR=${PROJECT_SOURCE_DIR}/src
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckEngineIsolation.cmake")

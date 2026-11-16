# =============================================================================
#  kb_licensing: verificación de licencias (Fase 9) + herramienta para emitirlas.
# =============================================================================
add_library(kb_licensing STATIC
    License.h  License.cpp  PublicKey.h)
add_library(kb::licensing ALIAS kb_licensing)
target_include_directories(kb_licensing PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(kb_licensing PUBLIC kb::juce)
set_target_properties(kb_licensing PROPERTIES FOLDER "KB")

# Herramienta de Castro Labs (no se distribuye a los clientes).
add_executable(kb_license_tool "${PROJECT_SOURCE_DIR}/tools/license_tool/main.cpp")
target_link_libraries(kb_license_tool PRIVATE kb::licensing)
set_target_properties(kb_license_tool PROPERTIES FOLDER "Herramientas")

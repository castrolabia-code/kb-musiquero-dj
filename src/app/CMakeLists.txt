# =============================================================================
#  kb_app_core: piezas de la app que no son la ventana (i18n, ajustes).
#  Separadas en una librería para poder probarlas en kb_tests.
# =============================================================================
add_library(kb_app_core STATIC
    I18n.h
    I18n.cpp
    AppSettings.h
    AppSettings.cpp
    AnalysisCache.h
    AnalysisCache.cpp)
add_library(kb::app_core ALIAS kb_app_core)

target_include_directories(kb_app_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(kb_app_core PUBLIC kb::juce kb::library kb_assets)
set_target_properties(kb_app_core PROPERTIES FOLDER "KB")

# =============================================================================
#  Aplicación de escritorio
# =============================================================================
juce_add_gui_app(KBMusiqueroDJ
    PRODUCT_NAME      "${KB_PRODUCT_NAME}"
    COMPANY_NAME      "${KB_COMPANY_NAME}"
    BUNDLE_ID         "${KB_BUNDLE_ID}"
    VERSION           "${PROJECT_VERSION}"
    COMPANY_COPYRIGHT "Copyright (c) Castro Labs. Todos los derechos reservados."
    ICON_BIG          "${PROJECT_SOURCE_DIR}/assets/icon/icon_1024.png"   # ícono de la app (Fase 7)
    NEEDS_CURL        FALSE
    NEEDS_WEB_BROWSER FALSE)

target_sources(KBMusiqueroDJ PRIVATE
    Main.cpp
    MainWindow.h            MainWindow.cpp
    MainComponent.h         MainComponent.cpp
    OptionsComponent.h      OptionsComponent.cpp
    AudioDeviceService.h    AudioDeviceService.cpp
    PlayerController.h      PlayerController.cpp
    PerformanceController.h PerformanceController.cpp
    TrackLibrary.h          TrackLibrary.cpp
    LibraryView.h           LibraryView.cpp
    ../ui/Palette.h
    ../ui/DrawPrimitives.h  ../ui/DrawPrimitives.cpp
    ../ui/Controls.h        ../ui/Controls.cpp
    ../ui/ControllerView.h  ../ui/ControllerView.cpp
    ../ui/DeckDisplay.h     ../ui/DeckDisplay.cpp
    ../ui/LevelMeter.h      ../ui/LevelMeter.cpp
    ../ui/TopBar.h          ../ui/TopBar.cpp
    ../ui/FxPanel.h         ../ui/FxPanel.cpp
    ../ui/SkinAssets.h      ../ui/SkinAssets.cpp
    ../ui/PlatterView.h     ../ui/PlatterView.cpp
    ../ui/TribalArt.h       ../ui/TribalArt.cpp
    ../ui/KbLookAndFeel.h   ../ui/KbLookAndFeel.cpp
    ../ui/SkinPlaceholders.h ../ui/SkinPlaceholders.cpp)

target_compile_definitions(KBMusiqueroDJ PRIVATE
    KB_PRODUCT_NAME="KB - MUSIQUERO DJ"
    KB_VERSION_STRING="${PROJECT_VERSION}")

target_link_libraries(KBMusiqueroDJ PRIVATE
    kb::app_core
    kb::library
    kb::analysis
    kb::engine
    kb::juce)

set_target_properties(KBMusiqueroDJ PROPERTIES FOLDER "KB")

if(MSVC)
    # Proyecto de inicio en Visual Studio.
    set_property(DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT KBMusiqueroDJ)
endif()

# =============================================================================
#  Instalación / empaquetado: `cmake --install <build> --component app --prefix <carpeta>`
#  deja el ejecutable y, en Windows, las DLL del runtime de Visual C++
#  (vcruntime140, msvcp140...) al lado, así corre en cualquier PC sin instalar
#  el "VC++ Redistributable". El instalador de la Fase 9 parte de esta carpeta.
# =============================================================================
if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
    set(CMAKE_INSTALL_UCRT_LIBRARIES FALSE)   # el UCRT ya viene con Windows 10/11
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT app)
    include(InstallRequiredSystemLibraries)
endif()

# TagLib (LGPL) es una DLL al lado del ejecutable: copiarla al compilar y al instalar.
if(WIN32)
    add_custom_command(TARGET KBMusiqueroDJ POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:tag>" "$<TARGET_FILE_DIR:KBMusiqueroDJ>")
    install(FILES "$<TARGET_FILE:tag>" DESTINATION . COMPONENT app)
endif()
install(FILES "${PROJECT_SOURCE_DIR}/third_party/licenses/TagLib-LGPL-2.1.txt"
              "${PROJECT_SOURCE_DIR}/third_party/licenses/utfcpp-BSL-1.0.txt"
        DESTINATION licenses COMPONENT app)

# Componente "app": deja afuera los archivos que instalan JUCE y Catch2.
install(TARGETS KBMusiqueroDJ
    RUNTIME DESTINATION . COMPONENT app
    BUNDLE  DESTINATION . COMPONENT app)

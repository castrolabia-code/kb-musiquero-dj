# Recursos embebidos en el ejecutable.
# Traducciones y fuentes (Orbitron y Rajdhani, licencia SIL OFL 1.1: assets/fonts/OFL-*.txt).
# Fase 7: renders del skin.
juce_add_binary_data(kb_assets
    NAMESPACE   KbBinaryData
    HEADER_NAME KbBinaryData.h
    SOURCES
        i18n/es.txt
        i18n/en.txt
        fonts/Orbitron-Black.ttf
        fonts/Orbitron-Bold.ttf
        fonts/Rajdhani-SemiBold.ttf
        fonts/Rajdhani-Bold.ttf)

set_target_properties(kb_assets PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
    FOLDER "KB")

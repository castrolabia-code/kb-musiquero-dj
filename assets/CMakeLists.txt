# Recursos embebidos en el ejecutable.
# Fase 0: traducciones. Fase 7: fuentes (Orbitron/Rajdhani, OFL) y renders del skin.
juce_add_binary_data(kb_assets
    NAMESPACE   KbBinaryData
    HEADER_NAME KbBinaryData.h
    SOURCES
        i18n/es.txt
        i18n/en.txt)

set_target_properties(kb_assets PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
    FOLDER "KB")

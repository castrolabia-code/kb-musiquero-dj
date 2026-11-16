# =============================================================================
#  kb_library: biblioteca de música (Fase 6): tags y carátulas (TagLib), base
#  de datos (SQLite), playlists y búsqueda. Sin UI: portable a Android.
# =============================================================================
add_library(kb_library STATIC
    TagReader.h        TagReader.cpp
    ArtworkCache.h     ArtworkCache.cpp
    LibraryDatabase.h  LibraryDatabase.cpp)
add_library(kb::library ALIAS kb_library)

target_include_directories(kb_library PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(kb_library PUBLIC kb::juce kb::engine kb::sqlite kb::taglib)
set_target_properties(kb_library PROPERTIES FOLDER "KB")

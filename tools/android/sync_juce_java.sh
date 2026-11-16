#!/usr/bin/env bash
# Copia las clases Java de JUCE que necesita la app de Android a android/juce-java.
# Correr cuando se actualice JUCE:  tools/android/sync_juce_java.sh <ruta a JUCE/modules>
set -euo pipefail
MOD="${1:?Uso: sync_juce_java.sh <JUCE/modules>}"
DEST="$(cd "$(dirname "$0")/../.." && pwd)/android/juce-java"
rm -rf "$DEST/com"
mkdir -p "$DEST"
for d in juce_core/native/javacore/init juce_core/native/javacore/app juce_core/native/java/app \
         juce_gui_basics/native/java/app juce_gui_basics/native/javaopt/app \
         juce_audio_devices/native/java/app juce_opengl/native/java/app; do
    cp -r "$MOD/$d/." "$DEST/"
done
echo "Listo: $(find "$DEST" -name '*.java' | wc -l) clases Java de JUCE en $DEST"

#!/usr/bin/env bash
# Prueba de humo en el emulador (CI): instala el APK, lo abre, toma capturas y
# muestra los errores. Uso: emulator_smoke_test.sh <carpeta-con-apk> <nombre>
set -uo pipefail
APK_DIR="$1"; NAME="$2"
APK=$(ls "$APK_DIR"/*.apk | head -1)
PKG=com.castrolabs.kbmusiquerodj
OUT=/tmp/kb-shots; mkdir -p "$OUT"

adb wait-for-device
adb shell getprop ro.build.version.release
adb shell wm size; adb shell wm density
adb install -r -g "$APK" || exit 1
adb logcat -c

# Música de prueba en el dispositivo (para la biblioteca).
if command -v ffmpeg > /dev/null; then
    ffmpeg -hide_banner -loglevel error -y -f lavfi -i "sine=frequency=110:duration=40" -ac 2 -c:a libmp3lame /tmp/prueba.mp3 && \
        adb push /tmp/prueba.mp3 /sdcard/Music/prueba.mp3 > /dev/null
fi

adb shell am start -W -n $PKG/com.castrolabs.kbmusiquerodj.KbActivity
sleep 25

shot() {
    adb exec-out screencap -p > "$OUT/$1.png"
    echo "=== CAPTURA $NAME-$1 ($(identify -format '%wx%h' "$OUT/$1.png" 2>/dev/null)) ==="
    convert "$OUT/$1.png" -resize '1400x1400>' -quality 72 "$OUT/$1.jpg"
    echo "KBSHOT-BEGIN $NAME-$1"
    base64 -w 0 "$OUT/$1.jpg" | fold -w 4000
    echo
    echo "KBSHOT-END $NAME-$1"
}
shot inicio

# Vista de biblioteca (Ctrl+B) y vuelta.
adb shell input keycombination KEYCODE_CTRL_LEFT KEYCODE_B || true
sleep 3
shot biblioteca
adb shell input keycombination KEYCODE_CTRL_LEFT KEYCODE_B || true
sleep 2

# ¿Sigue vivo?
if adb shell pidof $PKG > /dev/null; then echo "APP VIVA"; else echo "APP CERRADA"; fi

echo "=== LOGCAT (errores) ==="
adb logcat -d | grep -E "AndroidRuntime|FATAL|DEBUG  :|libc   :|JUCE|juce|KBMusiquero|kbmusiquero" | tail -150
adb shell pidof $PKG > /dev/null

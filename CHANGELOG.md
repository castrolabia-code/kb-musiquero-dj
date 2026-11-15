# Changelog

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).
Versionado: `MAYOR.MENOR.PARCHE` (la versión vive solo en el `CMakeLists.txt` raíz).

## [0.1.0] — Fase 0: Base del proyecto

### Agregado
- Proyecto CMake (sin Projucer) con JUCE 8.0.15 y C++20, presets para Windows x64 (Visual Studio) y Linux (CI).
- Estructura de carpetas según el documento maestro (`src/engine`, `analysis`, `library`, `midi`, `ui`, `licensing`, `app`, `installer`).
- `kb_juce`: módulos de JUCE compilados una sola vez y compartidos.
- `kb_engine`: motor de audio sin dependencias de UI, con `prepare / process / release` en tiempo real (por ahora genera silencio).
- Ventana principal "KB - MUSIQUERO DJ": redimensionable (mínimo 1280×720), DPI-aware, recuerda tamaño y posición.
- Layout placeholder vectorial con la paleta oficial: barra superior, deck 1 / mixer / deck 2, waveforms, biblioteca y barra de estado.
- Opciones → Audio: WASAPI compartido, WASAPI exclusivo y WASAPI de baja latencia; ASIO opcional (`KB_ENABLE_ASIO`). Dispositivo, sample rate, buffer, hasta 4 salidas y latencia estimada. La configuración se guarda y se restaura.
- Barra de estado con driver, dispositivo, sample rate, buffer, latencia, salidas y CPU.
- Idiomas español/inglés en `assets/i18n/*.txt`, con cambio en vivo desde Opciones o el menú. También se traducen los textos internos de JUCE.
- Ajustes en `%APPDATA%\Castro Labs\KB Musiquero DJ\settings.xml` y log en `...\logs\kb-musiquero-dj.log`.
- Una sola instancia: si se abre otra, se trae la ventana existente al frente.
- Pruebas (Catch2): contrato del motor, paridad de claves ES/EN, traducción en vivo, claves usadas en el código y aislamiento del motor respecto de la UI.
- CI en GitHub Actions: compila en Windows y Linux, corre las pruebas y publica el `.exe`.
- Documentación: `docs/ARQUITECTURA.md` y `docs/LICENCIAS.md`.

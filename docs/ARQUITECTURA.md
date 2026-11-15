# Arquitectura — KB - MUSIQUERO DJ

Documento vivo: se actualiza al cerrar cada fase. Última actualización: **Fase 0**.

## 1. Vista general

```
                ┌───────────────────────────── app (escritorio) ─────────────────────────────┐
                │  Main.cpp ─ MainWindow ─ MainComponent ─ OptionsComponent                   │
                │       │            │              │                                         │
                │  AppSettings    I18n (ES/EN)   ui/ (skin, placeholders, LookAndFeel)         │
                │       │                                                                     │
                │  AudioDeviceService  ── WASAPI / ASIO (juce_audio_devices)                  │
                └───────┬─────────────────────────────────────────────────────────────────────┘
                        │ prepare / process / release  (solo buffers de float)
                ┌───────▼──────── engine (portable, sin UI) ───────┐   ┌── analysis (Fase 3) ──┐
                │ AudioEngine → Decks, Mixer, FX, Routing (F1–F5)  │   │ BPM, Key, waveform     │
                └──────────────────────────────────────────────────┘   └────────────────────────┘
                library (F6, SQLite) · midi (F8) · licensing (F9)
```

### Targets de CMake

| Target | Tipo | Contenido | Depende de |
|---|---|---|---|
| `kb_juce` | estática | Módulos de JUCE compilados una sola vez | JUCE |
| `kb_assets` | estática | Recursos embebidos (traducciones; luego fuentes y renders) | — |
| `kb_engine` | estática | Motor de audio | `kb_juce` (solo headers de audio) |
| `kb_app_core` | estática | I18n y ajustes (testeables sin ventana) | `kb_juce`, `kb_assets` |
| `KBMusiqueroDJ` | ejecutable | Ventana, UI y dispositivo de audio | todo lo anterior |
| `kb_tests` | ejecutable | Pruebas Catch2 | `kb_engine`, `kb_app_core` |

**¿Por qué `kb_juce`?** Los módulos de JUCE son targets INTERFACE que compilan su código dentro de
cada consumidor. Si el motor y la app los enlazaran por separado, los símbolos se duplicarían. Por
eso se compilan una vez y se reexportan sus definiciones e includes.

**Aislamiento del motor.** `src/engine` y `src/analysis` solo pueden incluir módulos de audio puros
(`juce_core`, `juce_audio_basics`, `juce_audio_formats`, `juce_dsp`…). La prueba `engine_isolation`
(`cmake/CheckEngineIsolation.cmake`) falla si aparece un include de `juce_gui_*`, `juce_graphics`,
`juce_opengl`, `juce_audio_devices`/`utils`/`processors` o de las carpetas `ui/app/midi/library`.
Así el motor se reutiliza tal cual en Android (Fase 10), donde solo cambia la capa de dispositivo
(Oboe/AAudio) y la UI.

## 2. Hilos

| Hilo | Qué corre | Puede |
|---|---|---|
| **Mensajes (UI)** | Ventanas, menús, ajustes, I18n, `AudioDeviceService::getStatus()` | Todo, salvo bloquear mucho tiempo |
| **Audio** (driver) | `AudioDeviceService::audioDeviceIOCallbackWithContext` → `AudioEngine::process` | **Solo** matemática sobre memoria pre-reservada y atómicos |
| **Carga/decodificación** (Fase 1) | Lectura de archivos y buffering anticipado | I/O, allocations |
| **Análisis** (Fase 3) | BPM, Key, waveform | CPU pesada, baja prioridad |

### Reglas de tiempo real (innegociables)

1. En `process()`: **cero** allocations, locks, I/O de disco, logs, excepciones o llamadas al sistema.
2. UI → motor: parámetros `std::atomic` y colas lock-free SPSC (Fase 1).
3. Motor → UI: atómicos para medidores y posición; la UI los lee con un timer de 60 Hz (Fase 2).
4. `prepare()` reserva toda la memoria; JUCE lo llama con el audio detenido (`audioDeviceAboutToStart`).
5. `RealtimeChecks.h` exige con `static_assert` que los atómicos usados sean lock-free.
6. `audioDeviceError()` puede llegar desde el hilo de audio: solo incrementa un contador.
7. Procesamiento interno en `float` de 32 bits, con `ScopedNoDenormals`.

## 3. Audio y dispositivos (Windows)

- Tipos de driver: **Windows Audio** (WASAPI compartido), **Windows Audio (Exclusive Mode)**,
  **Windows Audio (Low Latency Mode)** y **ASIO** (solo con `KB_ENABLE_ASIO=ON`). DirectSound está
  desactivado a propósito porque su latencia no sirve para DJ.
- Se abren hasta **4 salidas**: 1-2 para el master y 3-4 para el cue de auriculares (el ruteo llega en la Fase 2).
- Latencia mostrada = (buffer + latencia informada por el driver) / sample rate.
- El estado del dispositivo se guarda como XML en los ajustes en cada cambio. Si al arrancar el
  dispositivo guardado no está, se abre el predeterminado.

## 4. Internacionalización

- `assets/i18n/es.txt` y `en.txt` (formato `LocalisedStrings` de JUCE), embebidos con `juce_add_binary_data`.
- El código usa **claves** (`kb::tr ("menu.options")`), nunca textos. Variables: `{nombre}`.
- Las traducciones se instalan también como mapeo global de JUCE, así se traducen los textos
  internos de JUCE (selector de audio, errores de WASAPI). Esas claves son el texto en inglés original.
- Cambio de idioma **en vivo**: `I18n::Listener::languageChanged()`; los componentes que pintan
  texto lo traducen al pintar, por lo que alcanza con repintar.
- Pruebas: misma cantidad y nombre de claves en ES y EN, y toda clave usada en el código existe.
- La C++ no contiene literales no-ASCII visibles al usuario (todo texto con tildes está en los `.txt`).

## 5. Persistencia

| Qué | Dónde (Windows) |
|---|---|
| Ajustes (idioma, ventana, audio) | `%APPDATA%\Castro Labs\KB Musiquero DJ\settings.xml` |
| Log (máx. 1 MB, se recorta al arrancar) | `%APPDATA%\Castro Labs\KB Musiquero DJ\logs\kb-musiquero-dj.log` |
| Biblioteca SQLite (Fase 6) | misma carpeta |

`settings.version` permite migrar el formato en el futuro.

## 6. UI y escalado

- JUCE gestiona la conciencia de DPI por monitor en Windows: todo se dibuja en coordenadas lógicas
  y se rasteriza a la resolución física, sin pixelado en 1080p, 1440p o 4K.
- El layout se calcula en **proporciones** del área (unidad = alto/100), no en píxeles fijos.
- Fase 0: el skin es un **placeholder vectorial** (`ui/SkinPlaceholders`) con el layout definitivo y la paleta oficial (`ui/Palette.h`).
- Fase 7: chasis y platos pasan a renders PNG @1x/@2x/@4x; lo dinámico sigue siendo vectorial; waveforms y animaciones por OpenGL.

## 7. Build y CI

- `CMakePresets.json`: `windows-x64-debug/release` (Visual Studio, x64) y `linux-debug/release` (Ninja).
- GitHub Actions (`.github/workflows/build.yml`) compila en Windows y Linux, corre `ctest` y publica el ejecutable de Windows.
- Runtime de MSVC: dinámico (`/MD`). El instalador (Fase 9) incluirá el VC++ Redistributable.
  Se eligió dinámico para compartir el runtime con la DLL de TagLib (Fase 6) sin problemas de ABI.

## 8. Decisiones registradas

| # | Decisión | Motivo |
|---|---|---|
| D1 | JUCE vía `FetchContent` con tag fijo | Builds reproducibles sin submódulos. |
| D2 | Módulos de JUCE en una estática compartida (`kb_juce`) | Evitar símbolos duplicados entre motor, app y pruebas. |
| D3 | Linux como plataforma de CI además de Windows | Pruebas rápidas y verificación del aislamiento del motor (portabilidad). |
| D4 | DirectSound desactivado | Latencia inaceptable para DJ; WASAPI la reemplaza. |
| D5 | ASIO desactivado por defecto | Requiere el acuerdo propietario de Steinberg para uso comercial. |
| D6 | Idioma por defecto: el del SO si es inglés, si no español | Mercado principal hispanohablante. |
| D7 | Una sola instancia | Dos instancias pelearían por el dispositivo de audio exclusivo. |

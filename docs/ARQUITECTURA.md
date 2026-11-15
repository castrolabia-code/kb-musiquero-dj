# Arquitectura — KB - MUSIQUERO DJ

Documento vivo: se actualiza al cerrar cada fase. Última actualización: **Fase 3**.

## 1. Vista general

```
                ┌───────────────────────────── app (escritorio) ─────────────────────────────┐
                │  Main.cpp ─ MainWindow ─ MainComponent ─ OptionsComponent                   │
                │       │            │              │  (drag & drop, menú, refresco 30 Hz)    │
                │  AppSettings    I18n (ES/EN)   ui/ ControllerView, DeckDisplay, Controls   │
                │       │                                                                     │
                │  PlayerController ── carga pistas y las entrega al motor                    │
                │  AudioDeviceService  ── WASAPI / ASIO (juce_audio_devices)                  │
                └───────┬─────────────────────────────────────────────────────────────────────┘
                        │ prepare / process / release  (solo buffers de float)
                ┌───────▼──────── engine (portable, sin UI) ────────────┐   ┌── analysis (F3) ──┐
                │ AudioEngine → 2 × Deck, mixer (vol, xfader, master)    │   │ BPM, Key, wave RGB │
                │ TrackLoader (hilos) → TrackBuffer (float, en memoria)  │   └────────────────────┘
                │ AudioFileFormats (JUCE + Media Foundation en Windows)  │
                └────────────────────────────────────────────────────────┘
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
| **Carga** (`TrackLoader`, 1 hilo por deck) | Decodificación + conversión de sample rate (r8brain) | I/O, allocations, COM/Media Foundation |
| **Análisis** (Fase 3) | BPM, Key, waveform | CPU pesada, baja prioridad |

### Reglas de tiempo real (innegociables)

1. En `process()`: **cero** allocations, locks, I/O de disco, logs, excepciones o llamadas al sistema.
2. UI → motor: comandos por cola lock-free SPSC (`LockFreeQueue.h`) y parámetros continuos `std::atomic`, suavizados en el audio (`SmoothedValue`, 20 ms).
3. Motor → UI: atómicos para medidores y posición; la UI los lee con un timer de 60 Hz (Fase 2).
4. `prepare()` reserva toda la memoria; JUCE lo llama con el audio detenido (`audioDeviceAboutToStart`).
5. `RealtimeChecks.h` exige con `static_assert` que los atómicos usados sean lock-free.
6. `audioDeviceError()` puede llegar desde el hilo de audio: solo incrementa un contador.
7. Procesamiento interno en `float` de 32 bits, con `ScopedNoDenormals`.

## 3. Reproducción (Fase 1)

### Ciclo de vida de una pista

```
 UI: loadFile(deck, archivo)
   └─▶ TrackLoader (hilo de fondo)
         1. abre el lector (JUCE o Media Foundation)
         2. reserva TrackBuffer completo (float estéreo, al sample rate del motor)
         3. onReady ─▶ (callAsync) ─▶ PlayerController ─▶ AudioEngine::loadTrack
         4. sigue decodificando por bloques + r8brain, publica 'decodedFrames' (release)
   AudioEngine (hilo de audio)
         Deck lee solo frames < decodedFrames (acquire). Si alcanza el borde, espera.
         Al reemplazarla, la pista vieja queda "retirada"; cuando el fundido termina
         vuelve por la cola 'released' ─▶ collectGarbage() (hilo de mensajes) la libera.
```

- Memoria: ~10 MB por minuto a 44.1/48 kHz. Límite de 1 GiB por pista (~45 min). Para sets largos
  se evaluará streaming desde disco en una fase posterior.
- Cada pista tiene un id único; la UI compara `Deck::Status::trackId` para no mostrar el estado de
  una pista vieja durante un reemplazo.
- La "vista general" (4096 picos) se calcula mientras decodifica.

### Deck

- Posición en frames (`double`). Si la pista se cargó al sample rate del motor, la lectura es
  exacta; si no (cambio de dispositivo después de cargar), interpolación Hermite de 4 puntos.
- **Anti-click**: todo cambio brusco crea un "ghost" (segundo cabezal) que se apaga en 3 ms mientras
  el principal entra desde 0. Probado midiendo el mayor salto entre muestras.
- **CUE estilo CDJ**: ver comentario de `engine/Deck.h` y pruebas `[cue]`.

### Mixer (Fase 2)

```
 Deck ─▶ ChannelStrip: GAIN (±12 dB) ─▶ EQ isolator ─▶ FILTER ─▶ vúmetro de canal
                                                       │
                       ┌───────────────────────────────┴─────────────┐
                       ▼ fader(vol) · crossfader                     ▼ CUE (pre-fader, 0/1 suavizado)
                    MASTER bus ─▶ · fader(master) ─▶ Limiter ─▶ vúmetros master ─▶ par del master
                       │                                             │
                       └────────── (antes del volumen MASTER) ──▶ CUE MIX ─▶ · fader(phones) ─▶ Limiter ─▶ par de auriculares
```

- `fader(p) = p²` (0,5 → −12 dB). Todas las ganancias se suavizan 20 ms.
- **EQ isolator** (`dsp/ThreeBandIsolator`): LR4 en 250 Hz y 3 kHz + pasa-todo de compensación en
  los graves: la suma es plana. Perilla: 0 = kill, 0,5 = 0 dB, 1 = +6 dB.
- **Filtro** (`dsp/DjFilter`): SVF TPT (Zavalishin/Simper), Q = 1, coeficientes cada 16 muestras,
  fundido con la señal seca en la zona central.
- **Limitador** (`dsp/Limiter`): mínimo deslizante + promedio en la ventana de anticipación (2 ms) ⇒
  la ganancia aplicada a cada muestra es siempre ≤ la necesaria: nunca supera −0,3 dBFS.
- **Crossfader**: suave (potencia constante), corte (10 %), scratch (2 %), inversión.
- **Ruteo**: `masterPair` 0/1 elige 1-2 o 3-4; auriculares al otro. SPLIT: L = master mono,
  R = auriculares mono en el par del master. Sin salidas 3-4, el master cae a 1-2.

### Medición

- `MeterSource` (motor, lock-free): el audio acumula pico, suma de cuadrados y clip; la UI lee y
  reinicia a 60 Hz (`VBlankAttachment`), sin perder picos entre cuadros.
- `MeterBallistics` (lógica pura, probada): pico instantáneo con caída de 18 dB/s, RMS con ataque de
  50 ms, retención 1,5 s, clip 2 s.
- `ui/LevelMeter`: escala no lineal en dBFS, solo repinta cuando cambia algún LED.
- Clip del master = el bus habría pasado 0 dBFS antes del limitador.

### Tempo y SYNC (Fase 3)

- Velocidad del deck = `ratio (sample rate) × (1 + tempo%)`.
- Sin Key Lock ni key shift: lectura directa con interpolación Hermite (el tono cambia con la velocidad).
- **Key Lock / key shift** (`Deck::renderStretched`, Signalsmith Stretch, preset por defecto):
  - Cada bloque lee `n × velocidad` muestras de entrada y el stretch devuelve `n` de salida.
  - Tono: `keyShift` con Key Lock; `keyShift + 12·log2(velocidad)` sin Key Lock (emula el vinilo).
  - **Posición publicada = lo que se escucha**: `lectura − (latenciaEntrada + velocidad × latenciaSalida)`.
    El SYNC compara posiciones escuchadas, así alinea el audio real aunque un deck use stretch y el otro no.
  - Tras cada discontinuidad (play, salto, cue, carga, cambio de modo) se "ceba" con `outputSeek()`
    desde la posición escuchada, y se entra con el fundido de 3 ms de siempre.
  - Toda la memoria del stretch se reserva en `prepare()` (y se "calienta" una vez): la prueba de
    allocations lo verifica con Key Lock prendiéndose y apagándose.
- **SYNC** se resuelve en el hilo de audio (necesita posiciones exactas):
  1. `syncRatio(masterBPM·tempoMaster, slaveBPM)` elige ×1, ×½ o ×2, lo que quede más cerca de 1.
  2. Fase: beats transcurridos desde la grilla de cada pista; el esclavo salta (con fundido) a la
     misma fase del master al activar SYNC o al dar PLAY con SYNC encendido.
  3. Mientras está encendido: sigue el tempo del master cada bloque y corrige la deriva de fase con
     un ajuste de velocidad de hasta ±1 % (converge en ~2 s).
  4. Activar SYNC en un deck lo apaga en el otro; mover el tempo a mano lo suelta.

## 4. Análisis (Fase 3)

```
 PlayerController ──(pista cargada)──▶ AnalysisCache.find(archivo) ──sí──▶ setBeatInfo()
                                           │ no
                                           ▼
                           TrackAnalyser "deck" (1 hilo, prioridad baja)
                           espera a que termine de decodificar ─▶ BpmDetector ─▶ setBeatInfo() + cache

 TrackLibrary (+) ──▶ TrackAnalyser "biblioteca" (otro hilo) ─▶ decodifica a 22 kHz ─▶ BpmDetector ─▶ cache
```

- **BpmDetector** (`src/analysis`): (1) flujo espectral a ~11 kHz, FFT 512, salto 64 (~180 Hz de
  envolvente); (2) autocorrelación con refuerzo de 2× y 4× el período, rango de una octava
  (78–156); (3) ajuste fino plegando toda la envolvente en un beat (pasos de 0,01 y 0,001 BPM);
  redondeo a entero si está a < 0,03. La fase de la grilla sale del pliegue de la banda de graves
  (< 200 Hz) cuando es clara.
- `TrackBuffer::setBeatInfo` usa atómicos: el audio (SYNC) y la UI los leen sin locks.
- **KeyDetector**: cromagrama (FFT 4096 a ~11 kHz, 55 Hz–2 kHz, A4 = 440 Hz, cada cuadro normalizado
  y comprimido) correlacionado con los 24 perfiles de Krumhansl-Kessler. Codificación 0..11 mayores,
  12..23 menores; Camelot y compatibilidad (misma, ±1, relativa) en `MusicalKey`.
- **Waveform**: al decodificar, `TrackBuffer` separa graves (< 200 Hz), medios y agudos (> 2,5 kHz)
  con filtros de un polo y guarda picos por banda: 4096 bins para la vista general y 150 por segundo
  para la ampliada. La UI los dibuja en imágenes cacheadas (la ampliada cubre 3 ventanas de zoom y
  se re-dibuja solo al salir de ese rango).
- `AnalysisCache` (JSON en `%APPDATA%`) invalida una entrada si cambian el tamaño o la fecha del archivo;
  se guarda cada 5 s y al cerrar.

## 4b. Audio y dispositivos (Windows)

- Tipos de driver: **Windows Audio** (WASAPI compartido), **Windows Audio (Exclusive Mode)**,
  **Windows Audio (Low Latency Mode)** y **ASIO** (solo con `KB_ENABLE_ASIO=ON`). DirectSound está
  desactivado a propósito porque su latencia no sirve para DJ.
- Se abren hasta **4 salidas**: 1-2 para el master y 3-4 para el cue de auriculares (el ruteo llega en la Fase 2).
- Latencia mostrada = (buffer + latencia informada por el driver) / sample rate.
- El estado del dispositivo se guarda como XML en los ajustes en cada cambio. Si al arrancar el
  dispositivo guardado no está, se abre el predeterminado.

## 5. Internacionalización

- `assets/i18n/es.txt` y `en.txt` (formato `LocalisedStrings` de JUCE), embebidos con `juce_add_binary_data`.
- El código usa **claves** (`kb::tr ("menu.options")`), nunca textos. Variables: `{nombre}`.
- Las traducciones se instalan también como mapeo global de JUCE, así se traducen los textos
  internos de JUCE (selector de audio, errores de WASAPI). Esas claves son el texto en inglés original.
- Cambio de idioma **en vivo**: `I18n::Listener::languageChanged()`; los componentes que pintan
  texto lo traducen al pintar, por lo que alcanza con repintar.
- Pruebas: misma cantidad y nombre de claves en ES y EN, y toda clave usada en el código existe.
- La C++ no contiene literales no-ASCII visibles al usuario (todo texto con tildes está en los `.txt`).

## 6. Persistencia

| Qué | Dónde (Windows) |
|---|---|
| Ajustes (idioma, ventana, audio) | `%APPDATA%\Castro Labs\KB Musiquero DJ\settings.xml` |
| Log (máx. 1 MB, se recorta al arrancar) | `%APPDATA%\Castro Labs\KB Musiquero DJ\logs\kb-musiquero-dj.log` |
| Biblioteca SQLite (Fase 6) | misma carpeta |

`settings.version` permite migrar el formato en el futuro.

## 7. UI y escalado

- JUCE gestiona la conciencia de DPI por monitor en Windows: todo se dibuja en coordenadas lógicas
  y se rasteriza a la resolución física, sin pixelado en 1080p, 1440p o 4K.
- El layout se calcula en **proporciones** del área (unidad = alto/100), no en píxeles fijos.
- Fase 0: el skin es un **placeholder vectorial** (`ui/SkinPlaceholders`) con el layout definitivo y la paleta oficial (`ui/Palette.h`).
- Fase 7: chasis y platos pasan a renders PNG @1x/@2x/@4x; lo dinámico sigue siendo vectorial; waveforms y animaciones por OpenGL.

## 8. Build y CI

- `CMakePresets.json`: `windows-x64-debug/release` (Visual Studio, x64) y `linux-debug/release` (Ninja).
- GitHub Actions (`.github/workflows/build.yml`) compila en Windows y Linux, corre `ctest` y publica el ejecutable de Windows.
- Runtime de MSVC: dinámico (`/MD`). El instalador (Fase 9) incluirá el VC++ Redistributable.
  Se eligió dinámico para compartir el runtime con la DLL de TagLib (Fase 6) sin problemas de ABI.

## 9. Decisiones registradas

| # | Decisión | Motivo |
|---|---|---|
| D1 | JUCE vía `FetchContent` con tag fijo | Builds reproducibles sin submódulos. |
| D2 | Módulos de JUCE en una estática compartida (`kb_juce`) | Evitar símbolos duplicados entre motor, app y pruebas. |
| D3 | Linux como plataforma de CI además de Windows | Pruebas rápidas y verificación del aislamiento del motor (portabilidad). |
| D4 | DirectSound desactivado | Latencia inaceptable para DJ; WASAPI la reemplaza. |
| D5 | ASIO desactivado por defecto | Requiere el acuerdo propietario de Steinberg para uso comercial. |
| D6 | Idioma por defecto: el del SO si es inglés, si no español | Mercado principal hispanohablante. |
| D7 | Una sola instancia | Dos instancias pelearían por el dispositivo de audio exclusivo. |
| D8 | Pistas decodificadas completas en memoria (float) | Saltos, cues, loops y scratch (Fase 4) instantáneos y sin I/O en el audio. Costo: ~10 MB/min. |
| D9 | Conversión de sample rate al cargar (r8brain), no en tiempo real | Máxima calidad sin costo en el callback. El tempo/pitch (Fase 3) se hará sobre la pista ya convertida. |
| D10 | Fundidos de 3 ms con cabezal "ghost" | Elimina clicks en todos los cambios sin perder respuesta (3 ms es inaudible como demora). |
| D11 | M4A con Media Foundation propio (no `WindowsMediaAudioFormat` de JUCE) | API moderna, AAC confiable, salida float directa. |
| D12 | EQ tipo isolator (LR4) en vez de shelving/peaking | Permite kill real de cada banda, como los mixers de club. |
| D13 | Limitador brickwall siempre activo en master y auriculares | Prioridad: nunca saturar parlantes ni oídos. Costo: 2 ms de latencia. |
| D14 | El CUE MIX toma el master antes del volumen MASTER | Bajar los parlantes no apaga el master en los auriculares (comportamiento de mixer de club). |
| D15 | Kill = perilla toda a la izquierda (sin botones de kill aparte) | El layout de referencia no tiene botones de kill; MIDI (Fase 8) podrá mapear kills a botones. |
| D16 | Rango de BPM de una octava (78–156) | Elimina la ambigüedad mitad/doble: cada tempo tiene una única lectura. Cubre house, techno, cumbia, reggaetón y hip-hop. |
| D17 | SYNC con corrección continua de fase (±1 % máx.) | Mantiene los beats alineados aunque haya pequeños errores numéricos, sin saltos audibles. |
| D18 | Lista de canciones simple en JSON antes de la biblioteca SQLite | Lo pidió el usuario antes de la Fase 6; se migra a SQLite sin cambiar la UI. |
| D19 | Fondo del controlador cacheado en imagen | Animar los platos a 60 fps sin redibujar sombras ni degradés. |
| D20 | Key Lock con Signalsmith Stretch (preset por defecto) | Calidad alta con ~1,7 % de CPU por deck; MIT. El stretch se usa solo con Key Lock o key shift: sin ellos el audio sale sin procesar. |
| D21 | Posición publicada = posición escuchada | Con latencias distintas entre decks (stretch vs. directo), el SYNC tiene que alinear lo que se oye. |
| D22 | Waveform de 3 bandas en colores del deck (no RGB clásico) | Respeta la paleta de la referencia (cyan / naranja) y mantiene la lectura por frecuencia. |
| D23 | AUTO MIX dentro del motor (hilo de audio), no en la UI | El crossfader avanza muestra a muestra: el pase es exacto y no depende de que la ventana esté libre. La UI solo lo pide/cancela y muestra el progreso (atómico). Mover el crossfader a mano lo cancela. |
| D24 | AUTO MIX: curva suave forzada + bass swap a mitad del pase | Potencia constante (sin "pozo" en el medio) y sin dos bombos a la vez; los graves vuelven a su lugar al terminar. Si los BPM difieren más de ±12 % no se toca el tempo. |
| D25 | Tribales y marco amarillo dibujados por código (Paths) y cacheados | Nítidos en 1080p/4K sin PNG; costo cero por cuadro. Los renders finales (Fase 7) pueden reemplazarlos. |

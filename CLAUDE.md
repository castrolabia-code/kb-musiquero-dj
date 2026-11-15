# PROMPT MAESTRO — KB - MUSIQUERO DJ · by Castro Labs

> Pegá este documento completo en Claude Code (o guardalo como `CLAUDE.md` en la raíz del proyecto).
> Copiá las dos imágenes de referencia en `docs/referencias/` antes de empezar.

---

## 1. TU ROL

Actuás como un equipo senior compuesto por:
- **Ingeniero de audio en tiempo real (C++/DSP)** con experiencia en software DJ comercial (Serato, Rekordbox, Traktor, VirtualDJ).
- **Arquitecto de software** especializado en JUCE, CMake y aplicaciones de escritorio multiplataforma.
- **Diseñador UI/UX y gráfico** experto en interfaces de audio fotorrealistas, skeuomórficas y con estética neón.

Escribís código de nivel profesional, comentado, mantenible y probado. Antes de cada fase explicás brevemente el plan, y al terminarla te detenés para que yo pruebe antes de seguir.

---

## 2. EL PRODUCTO

**Nombre:** KB - MUSIQUERO DJ
**Marca:** by Castro Labs
**Tipo:** Software DJ comercial de 2 decks, standalone, con todas las funciones de un programa DJ profesional.
**Plataforma inicial:** Windows 10/11 (x64), instalable.
**Plataforma futura:** Android (tablets). Toda la arquitectura debe quedar preparada para portarlo sin reescribir el motor.
**Uso:** comercial (se va a vender).
**Idioma de la interfaz:** Español e Inglés, seleccionable desde Opciones (todos los textos en archivos de traducción, nada hardcodeado).

---

## 3. STACK TECNOLÓGICO (obligatorio)

- **JUCE 8** + **C++20** + **CMake** (sin Projucer).
- **Audio en Windows:** WASAPI (exclusivo y compartido) y ASIO (opcional, con selector de dispositivo y tamaño de buffer). En Android, más adelante: Oboe/AAudio vía JUCE.
- **Time-stretch / Key Lock:** **Signalsmith Stretch** (licencia MIT, compatible con venta comercial). No usar Rubber Band ni librerías GPL/AGPL sin consultarme.
- **Lectura de tags (artista, título, carátula, BPM, key):** TagLib (LGPL, enlazada dinámicamente).
- **Base de datos de la biblioteca:** SQLite.
- **Detección de BPM y tonalidad (Key):** algoritmos propios (onset detection + autocorrelación para BPM; cromagrama + perfiles Krumhansl-Schmuckler para Key), con notación musical y Camelot (8A, 5B…).
- **Instalador:** Inno Setup (con firma de código preparada).
- Antes de incorporar cualquier dependencia, verificá y anotá su licencia en `docs/LICENCIAS.md`. Recordame revisar las condiciones de la licencia de JUCE y del SDK de ASIO para uso comercial.

---

## 4. ARQUITECTURA

Separación estricta en módulos:

```
KB-Musiquero-DJ/
├─ CMakeLists.txt
├─ docs/               (referencias, licencias, manual)
├─ assets/             (skins renderizadas, fuentes, íconos, traducciones)
├─ src/
│  ├─ engine/          motor de audio (sin dependencias de UI)
│  │   ├─ Deck, Mixer, Crossfader, EQ, Filter, FX, Sampler
│  │   ├─ TimeStretch, BeatGrid, Sync, Loops, HotCues
│  │   └─ AudioRouting (master / cue auriculares)
│  ├─ analysis/        BPM, Key, waveform RGB, ganancia automática
│  ├─ library/         SQLite, importador, playlists, búsqueda
│  ├─ midi/            MIDI Learn, mapeos, feedback LED
│  ├─ ui/              componentes visuales y skin
│  ├─ licensing/       activación comercial
│  └─ app/             ventana principal, ajustes, i18n
└─ installer/          script Inno Setup
```

**Reglas de audio en tiempo real (innegociables):**
- Cero allocations, locks, I/O de disco o logs dentro del callback de audio.
- Comunicación UI ↔ motor mediante colas lock-free y parámetros atómicos.
- Carga y decodificación de pistas en hilos de fondo, con buffering anticipado.
- Latencia objetivo: ≤ 10 ms con ASIO/WASAPI exclusivo.
- Procesamiento interno en float 32 bits, con conversión de sample rate de calidad.

---

## 5. FUNCIONES

### 5.1 Reproducción y archivos
- Formatos: **MP3, WAV, AIFF, FLAC, OGG, M4A/AAC** (AAC mediante Windows Media Foundation).
- **Arrastrar y soltar** canciones desde el Explorador de Windows directamente a un deck o a la biblioteca, y desde la biblioteca a cada deck.
- Carga instantánea con análisis en segundo plano.

### 5.2 Decks (x2) — idénticos en espejo
- **Jog wheel / plato** animado que gira con la reproducción: scratch en modo **VINYL**, pitch bend en modo normal (FWD+ / REV-).
- **Play/Pause, CUE** (con comportamiento CDJ estándar) y **SYNC** (sincronía de tempo y fase, con modo master).
- **Fader de TEMPO** con rangos de ±8 %, ±16 % y ±50 %, y **Key Lock** (master tempo).
- **Key shift** (±12 semitonos) y matching de tonalidad.
- **Beatgrid** automático y editable.
- **Loops:** auto-loop (1/32 a 32 beats), loop manual in/out, duplicar y dividir, loop roll.
- **8 Hot Cues** por pista, con colores y guardado en la base de datos.
- Slip mode, censor/reverse, brake y backspin.
- Display por deck: carátula, título, artista, BPM, tonalidad, tiempo transcurrido y restante, y pitch %.

### 5.3 Performance Pads (4 por deck, como en las referencias)
Modos seleccionables con los botones **HOT CUE / FX / SAMPLER / LOOP** (SHIFT accede al modo secundario de cada botón):
- **HOT CUE:** dispara los cues 1–4 (con SHIFT, los 5–8).
- **SAMPLER:** banco de samples asignables por arrastrar y soltar, con volumen propio y modo one-shot o loop.
- **FX:** disparo momentáneo de efectos.
- **LOOP:** auto-loops de 1, 2, 4 y 8 beats.

### 5.4 Mixer central
- **Volumen** por canal (faders), **GAIN/TRIM** con auto-gain.
- **EQ de 3 bandas** (HIGH / MID / LOW) con kill, y perilla **FILTER/BASS** combinada (LPF a la izquierda, HPF a la derecha).
- **Crossfader** con curva ajustable (suave / cut / scratch) e inversión.
- **MASTER** con limitador de seguridad.
- **HEADPHONES:** volumen de auriculares, **CUE** por canal y **CUE MASTER** (mezcla cue/master).
- **SPLIT ON/OFF:** modo split para usar el cue en auriculares sin una placa de 4 salidas (master en el canal izquierdo y cue en el derecho, en mono).
- **Ruteo de salidas:** master a los parlantes y cue a los auriculares, seleccionando las salidas 1-2 / 3-4 de la placa de audio.

### 5.5 Vúmetros digitales realistas
- Vúmetro por canal y master estéreo (como la barra superior de la referencia).
- Medición **peak + RMS**, peak hold con caída lenta, indicador de clip, escala en dBFS calibrada y degradé verde → amarillo → naranja → rojo.
- Segmentos LED con glow, animados a 60 fps, con balística realista (ataque rápido y caída suave).

### 5.6 Efectos (FX)
- 2 unidades de FX, asignables a cada deck o al master, con controles DRY/WET, parámetro principal y sincronía al BPM.
- Efectos: **Echo, Delay, Reverb, Flanger, Phaser, Filter, Bitcrusher, Gater, Roll, Brake, Backspin, Noise sweep**.

### 5.7 Waveforms
- Waveform **RGB por frecuencia** (graves / medios / agudos), con color cyan en el deck 1 y naranja en el deck 2 como en las referencias.
- Vista ampliada scrolleable (zoom ajustable) y vista general de la pista completa con click para saltar.
- Marcas de beatgrid, cues, loops y playhead central.

### 5.8 Biblioteca
- Panel inferior con árbol de **carpetas y playlists** a la izquierda y lista de pistas en el centro.
- Columnas: #, Título, Artista, BPM, Key (Camelot), Duración, Género, Rating y Carátula. Se pueden ordenar, redimensionar y mostrar u ocultar.
- **Búsqueda instantánea**, crear, renombrar y borrar playlists, historial de sesión, cola de análisis y modo "preparación".
- Colores de compatibilidad armónica respecto al deck que está sonando.

### 5.9 MIDI
- Detección automática de controladores MIDI.
- **MIDI Learn**: click derecho sobre cualquier control → mover el control físico → queda asignado.
- Mapeos guardables, importables y exportables (JSON). Soporte para jog wheels (relativo/absoluto), shift y **feedback de LEDs** hacia el controlador.

### 5.10 Funciones comerciales
- **Activación por licencia** (clave de producto con validación offline firmada; dejar preparada la validación online).
- Pantalla "Acerca de" con versión y marca Castro Labs, EULA en el instalador y en la app.
- Ajustes persistentes, autoguardado de la sesión, logs de errores y reporte de crash.
- Atajos de teclado configurables (Q/W play, A/S cue, etc.).

---

## 6. DISEÑO VISUAL

Inspirate fielmente en las imágenes de `docs/referencias/` (layout, proporciones y colores), mejorando la claridad y la legibilidad.

**Enfoque híbrido (obligatorio):**
- **Imágenes renderizadas en alta resolución** (PNG en @1x, @2x y @4x) para el chasis amarillo, los platos, los fondos del mixer y las sombras ambientales. Hasta que yo genere los renders finales, dibujá placeholders por código con el mismo layout.
- **Dibujado vectorial en tiempo real** para todo lo dinámico: perillas (rotación y arco de valor naranja), LEDs, botones con glow, vúmetros, waveforms y el anillo luminoso de los platos.
- La interfaz debe **escalar perfecto en 1080p, 1440p y 4K** (DPI-aware), sin pixelado.
- **GPU** (OpenGL vía JUCE) para waveforms y animaciones, a 60 fps fluidos.

**Paleta:**
| Uso | Color |
|---|---|
| Chasis | Amarillo `#F5D90A` con sombreado 3D |
| Paneles y mixer | Negro / gris carbón `#121417` – `#1E2126` |
| Pads y botones de acento | Cyan neón `#19E3F5` con glow |
| Perillas, arcos y anillos de plato | Naranja `#FF8A00` |
| PLAY | Verde neón `#2BEA5A` |
| CUE / SYNC alerta | Rojo-naranja `#FF4A3D` |
| Waveform deck 1 / deck 2 | Cyan / Naranja |

**Tipografía:** sans geométrica bold, tipo Orbitron, Rajdhani o Eurostile (verificá que la licencia permita incluirla), para el logo "KB MUSIQUERO DJ" y las etiquetas.

**Detalles de calidad:** sombras suaves, reflejos sutiles en los platos, bordes biselados, estados hover y pressed en cada botón, microanimaciones y transiciones suaves. El resultado debe sentirse premium, como hardware real.

**Layout (de arriba hacia abajo):**
1. Barra superior con el logo "KB MUSIQUERO DJ", el vúmetro master central y "BY CASTRO LABS".
2. Cuerpo del controlador con DECK 1 (fader de tempo, plato, pads, SYNC/CUE/PLAY), el MIXER central y DECK 2 en espejo.
3. Zona de waveforms con la info de cada deck.
4. Biblioteca con las carpetas y playlists a la izquierda y las pistas al centro.

Debe existir un modo de vista alternativo **"Biblioteca grande"** que achique el controlador.

---

## 7. PLAN DE TRABAJO POR FASES

Trabajá **una fase por vez**. Al terminar cada una: compilá, verificá los criterios, resumí lo hecho y **esperá mi aprobación**.

**Fase 0 — Base del proyecto**
CMake + JUCE, ventana vacía con el nombre de la app, configuración de dispositivo de audio, i18n ES/EN y estructura de carpetas.
✅ Compila en Windows y abre una ventana que se puede redimensionar.

**Fase 1 — Motor de audio mínimo**
2 decks que cargan los 6 formatos, play/pause/cue, volumen, crossfader y master. Arrastrar y soltar desde el Explorador.
✅ Mezclo dos MP3/FLAC sin clicks ni cortes, con la CPU estable.

**Fase 2 — Mixer completo y salidas**
EQ 3 bandas, FILTER/BASS, gain, limitador, cue de auriculares, CUE MASTER, SPLIT y ruteo de 4 salidas. Vúmetros funcionales.
✅ Escucho el deck en cue por auriculares mientras el master sale por los parlantes.

**Fase 3 — Análisis y sincronía**
BPM, Key, beatgrid, waveform RGB, SYNC, fader de tempo, Key Lock y key shift.
✅ El BPM detectado es correcto en ±0,1 en temas de house y cumbia de prueba, y SYNC mantiene la fase durante 5 minutos.

**Fase 4 — Performance**
Jog wheels (scratch y bend), hot cues, loops, pads en sus 4 modos y sampler.
✅ Todos los pads funcionan y los cues se guardan al reiniciar.

**Fase 5 — FX**
Las 2 unidades de FX y la lista completa de efectos, sincronizados al BPM.

**Fase 6 — Biblioteca**
SQLite, importación de carpetas, playlists, búsqueda, columnas y carátulas.

**Fase 7 — Diseño final**
Skin híbrido completo según la sección 6, animaciones, escalado 4K y modo "Biblioteca grande".

**Fase 8 — MIDI**
MIDI Learn, mapeos y feedback de LEDs.

**Fase 9 — Comercial**
Licencias, instalador Inno Setup, EULA, crash reporter, manual de usuario básico (ES/EN) y optimización final.

**Fase 10 (futura) — Android**
Port con JUCE/Oboe y una interfaz táctil adaptada.

---

## 8. REGLAS DE TRABAJO PARA CLAUDE CODE

1. No avances de fase sin mi aprobación.
2. Si algo es ambiguo, preguntame antes de asumir.
3. Cada vez que agregues una dependencia, explicá por qué y cuál es su licencia.
4. Incluí pruebas unitarias para el DSP y el análisis (BPM, Key, EQ).
5. Mantené un `CHANGELOG.md` y un `docs/ARQUITECTURA.md` actualizados.
6. Si una función es muy pesada o arriesgada, proponé una alternativa más simple primero.
7. Prioridad absoluta: **el audio nunca se corta**. La estética nunca debe comprometer el rendimiento.

**Empezá ahora por la Fase 0.** Primero mostrame el plan detallado y la lista de dependencias con sus licencias.

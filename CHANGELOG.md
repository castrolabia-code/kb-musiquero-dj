# Changelog

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).
Versionado: `MAYOR.MENOR.PARCHE` (la versión vive solo en el `CMakeLists.txt` raíz).

## [0.6.0] — Pedidos del usuario: AUTO MIX, Borrar lista y nuevo diseño

### Agregado
- **AUTO MIX** (en el mixer, a la izquierda del crossfader): pasa sola y de forma pareja a la otra bandeja. Arranca la pista del otro deck, la sincroniza (si los BPM están a menos de ±12 %), cambia los graves a mitad de camino (*bass swap*) y mueve el crossfader con curva de potencia constante. Al terminar, el deck que salió queda en pausa. Click de nuevo o mover el crossfader a mano: cancela.
- **Perilla MIX** (a la derecha del crossfader): velocidad de la mezcla automática, de 2 a 60 s (doble click: 10 s). Se guarda entre sesiones.
- **Botón "BORRAR LISTA"** en la columna de carpetas: vacía la lista con confirmación (los archivos no se borran de la PC).
- Pruebas del AUTO MIX: pase completo en ambos sentidos, nivel parejo durante todo el pase, cancelación, sin pista en el otro deck y BPM demasiado distintos.

### Cambiado
- **Nuevo diseño**: recuadro amarillo redondeado alrededor de todo el programa con **tribales negros tipo grafiti** (asimétricos) en las partes amarillas; logo **KB MUSIQUERO DJ** en neón 3D (Orbitron); títulos **DECK 1 / DECK 2** grandes en neón celeste con sombreado.
- **Letras más grandes** en toda la interfaz (botones, perillas, menús, lista y ayudas) con las fuentes Rajdhani y Orbitron (licencia OFL, incluidas).
- El display de cada deck muestra la información en dos columnas, más grande y legible.

## [0.5.0] — Fase 3 completa: tonalidad, Key Lock, key shift y waveform por frecuencia

### Agregado
- **Detección de tonalidad** (cromagrama + perfiles de Krumhansl-Schmuckler) con notación musical y **Camelot** (8A, 9B…). Se muestra en el display del deck (con el corrimiento aplicado) y en la nueva columna **Key** de la lista.
- **Colores de compatibilidad armónica** en la lista: las tonalidades que combinan con lo que está sonando (misma, ±1 en la rueda o relativa) se marcan en verde.
- **KEY LOCK** por deck (Signalsmith Stretch, MIT): cambiar el tempo no cambia el tono. La latencia del stretch se compensa: la posición mostrada y el SYNC usan lo que realmente se escucha.
- **Key shift** ±12 semitonos con **KEY -** / **KEY +** (doble click: tono original), sin cambiar la velocidad.
- **Waveform por frecuencia** (graves oscuros, medios y agudos claros, en cyan para el deck 1 y naranja para el deck 2):
  - **Vista ampliada** que avanza con la canción, playhead al centro, **grilla de beats** (más fuerte cada 4) y cue. Rueda del mouse: zoom de 2 a 32 s. Muestra tiempo real: con SYNC las grillas de los dos decks quedan alineadas.
  - **Vista general** de la canción completa debajo, con click/arrastre para saltar y la ventana del zoom marcada.
- El cache de análisis guarda también la tonalidad y se escribe cada 5 s (no se pierde si la PC se apaga de golpe). Los análisis viejos sin tonalidad se recalculan solos.
- Pruebas: tonalidad (C, Am, G, Em; Camelot; transposición; compatibilidad), Key Lock (tono constante a +10 %, key shift +12, latencia compensada < 15 ms, SYNC con un deck en Key Lock, cero allocations, rendimiento: 2 decks con Key Lock ≈ 3,4 % de un núcleo en Release).

### Cambiado
- La zona de waveforms es más alta y el display de cada deck muestra BPM y tonalidad en grande.

## [0.4.1] — Pedidos del usuario: PLAY/PAUSA, símbolos y ayuda

### Agregado
- **Botón PAUSA** separado del **PLAY** en cada deck (PAUSA titila mientras el deck está en pausa).
- **Símbolos** en los botones de transporte además del texto: PLAY ▶, PAUSA ❚❚, CUE |◀, SYNC ↻.
- **Ayuda al pasar el mouse** (tooltips) en todos los controles: transporte, tempo, rango, platos, pads, perillas, faders, crossfader, CUE, PHONES, CUE MIX, SPLIT, vúmetros, LIMIT, display y vista de la canción, y el botón "+". En español e inglés.

## [0.4.0] — Fase 3 (parte 1): BPM, SYNC, tempo, platos y lista de canciones

### Agregado
- **Detección de BPM y grilla** con algoritmo propio (flujo espectral + autocorrelación + ajuste fino plegando todo el tema): precisión de centésimas en house y cumbia de prueba; rango 78–156 BPM (los tempos fuera de rango se pliegan una octava). La fase de la grilla se toma de los graves (bombo/bajo).
- Análisis en segundo plano al cargar un tema y **cache de resultados** (`analysis.json`): un tema ya analizado no se vuelve a analizar (salvo que el archivo cambie).
- **Display del deck**: BPM efectivo (con el tempo aplicado) en grande, tempo en %.
- **Fader de TEMPO** funcional (abajo = más rápido, como en un CDJ), con botón de rango **±8 / ±16 / ±50 %**; doble click vuelve a 0.
- **SYNC**: iguala el tempo con la otra bandeja (eligiendo mitad/doble si corresponde) y **alinea los beats**. Mientras está encendido sigue los cambios de tempo de la otra bandeja y corrige la fase suavemente; al dar PLAY arranca ya en fase. Mover el tempo a mano lo suelta. Desfase medido en 5 minutos: < 5 ms.
- **Platos que giran** a 33⅓ rpm según la posición real (más rápido con más tempo, quietos en pausa); el anillo naranja se enciende mientras reproduce.
- **Lista de canciones** con botón **"+"** en CARPETAS (agregar canciones o una carpeta entera) y arrastrando archivos/carpetas sobre la biblioteca. Columnas #, Título, Artista, BPM, Duración y Formato, ordenables. Se carga arrastrando una fila a un deck, con doble click (al deck libre) o click derecho. Supr quita de la lista. La lista se guarda entre sesiones (`library.json`).
- Archivo → Agregar canciones a la lista.
- Optimización: el fondo del controlador se cachea en una imagen; repintar los platos a 60 fps es barato.
- Pruebas: BPM (house 128 y 124,5; cumbia 95 y 104,3 con humanización; plegado de octava; silencio; cancelación), grilla (< 15 ms), tempo, SYNC (relación mitad/doble, alineación, seguimiento, PLAY en fase, **5 minutos en fase**), cache de análisis.


## [0.3.0] — Fase 2: Mixer completo y salidas

### Agregado
- **GAIN/TRIM** por canal (±12 dB).
- **EQ de 3 bandas tipo isolator** (cruces Linkwitz-Riley de 24 dB/oct en 250 Hz y 3 kHz): plano al centro (±0,05 dB medido), +6 dB al máximo y **kill** (banda eliminada, más de 40 dB) con la perilla toda a la izquierda.
- **FILTER/BASS** combinado: pasa-bajos a la izquierda (hasta 60 Hz), pasa-altos a la derecha (hasta 8 kHz), sin efecto al centro. Filtro de variable de estado que se puede barrer rápido sin clicks.
- **Limitador de seguridad** en el master (anticipación de 2 ms, techo −0,3 dBFS): la salida nunca satura. LED **LIMIT** en la barra superior.
- **Auriculares**: CUE por canal (pre-fader), perilla PHONES, **CUE MIX** (cue ↔ master) y limitador propio. El volumen MASTER no afecta lo que se escucha en auriculares.
- **Ruteo de 4 salidas**: master a 1-2 o 3-4 y auriculares al otro par. Si la placa tiene 2 salidas, el master nunca se pierde.
- **SPLIT**: master mono en el canal izquierdo y auriculares mono en el derecho (botón en el mixer y en Opciones).
- **Crossfader**: curvas suave / corte / scratch e inversión.
- **Vúmetros LED**: por canal (en el mixer) y master estéreo (barra superior) con pico + RMS, retención de pico, LED de clip (click para apagarlo), escala en dBFS y degradé verde → amarillo → naranja → rojo con glow, a 60 fps sincronizados con la pantalla. Balística: ataque instantáneo y caída de 18 dB/s.
- **Opciones → Mixer**: curva e inversión del crossfader, salida del master, SPLIT y aviso si la placa tiene solo 2 salidas. Se guardan entre sesiones.
- Perillas bipolares (EQ, GAIN, FILTER, CUE MIX) con el arco naranja desde el centro.
- Pruebas: EQ (plano, kill, +6 dB), trim, filtro (cortes y barrido estable), limitador (nunca supera el techo; transparente por debajo), ruteo, CUE, CUE MIX, SPLIT, curvas del crossfader, vúmetros y balística, y cero allocations con todo el mixer en movimiento.

### Cambiado
- La salida ya no es idéntica bit a bit a la pista: el EQ isolator cambia la fase (como cualquier mixer) y el limitador agrega 2 ms de latencia. El nivel con todo al centro sigue siendo exactamente el mismo.

## [0.2.0] — Fase 1: Motor de audio mínimo

### Agregado
- **2 decks** con carga de **MP3, WAV, AIFF, FLAC, OGG y M4A/AAC** (M4A mediante Windows Media Foundation).
- **Carga en segundo plano y progresiva**: un hilo por deck; se puede dar PLAY apenas empieza a cargar.
- **Conversión de sample rate de alta calidad** al cargar (r8brain-free-src, MIT), con filtro anti-aliasing y sin retardo.
- **Transporte estilo CDJ**: PLAY/PAUSE y CUE (vuelve al cue, fija el cue en pausa, preescucha mientras se mantiene, PLAY + CUE sigue sonando). PLAY titila en pausa.
- **Sin clicks**: fundidos de 3 ms en play, pausa, saltos, CUE y cargas (segundo cabezal de fundido).
- **Mixer básico**: volumen por canal (curva cuadrática), crossfader de potencia constante y MASTER, todos suavizados (20 ms). Protección de recorte a 0 dBFS hasta el limitador de la Fase 2.
- **Arrastrar y soltar** desde el Explorador sobre cada deck (plato, mitad del controlador o su franja de waveform), con resaltado del deck destino.
- **Archivo → Cargar en Deck 1/2...** con selector de archivos filtrado por formato.
- **Display por deck**: título y artista (desde el nombre "Artista - Título"), tiempo transcurrido y restante, % de carga y **vista general de la pista** con click/arrastre para saltar, marca de cue y parte reproducida atenuada.
- Mensajes de error claros y traducidos al cargar (archivo inexistente, formato no soportado, dañado, demasiado largo, sin memoria).
- Comunicación UI → motor por cola lock-free; las pistas que el audio suelta vuelven por otra cola y se liberan en el hilo de mensajes.
- Pruebas: lógica de CUE, ausencia de clicks medida, mezcla y curvas, decodificación de los 6 formatos, calidad del resampler, carga progresiva y cancelación, **cero allocations en el callback de audio** (verificado reemplazando new/delete) y rendimiento (2 decks ≈ 0,5 % de un núcleo).

### Conocido / pendiente
- Si se cambia el sample rate del dispositivo con pistas cargadas, estas se reproducen con interpolación Hermite hasta volver a cargarlas.
- La biblioteca todavía no recibe archivos (Fase 6).

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
- Empaquetado (`cmake --install --component app`) con las DLL de Visual C++ al lado del ejecutable; los tags `vX.Y.Z` publican un Release con el ZIP descargable.
- Documentación: `docs/ARQUITECTURA.md` y `docs/LICENCIAS.md`.

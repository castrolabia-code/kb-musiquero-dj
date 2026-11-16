# Controladores MIDI

## Conectar
Enchufá el controlador por USB: se detecta solo (en unos 2 segundos) y aparece en el pie
de la ventana y en **Opciones > Controladores MIDI**. No hace falta reiniciar.

## Asignar (MIDI Learn)
1. Click derecho sobre cualquier control de la pantalla (botón, perilla, fader, plato, pad...).
2. Elegí **MIDI Learn: <función>**. El pie muestra "MIDI LEARN: mové un control...".
3. Mové o apretá el control del controlador. Listo: queda asignado y guardado.
   - **Esc** cancela.
   - Si ese control físico ya hacía otra cosa, se reasigna.
4. Para quitar: click derecho > **Quitar asignación MIDI**, o desde Opciones > MIDI.

Los pads tienen su propio menú (click derecho): ahí también está "MIDI Learn".

## Tipos de control
| Función | Control físico | Cómo se interpreta |
|---|---|---|
| Botones (PLAY, CUE, SYNC, pads...) | nota o CC | apretado / suelto (CUE y los pads respetan el "mantener apretado") |
| Faders y perillas | CC (0-127) o pitch bend (14 bits) | valor absoluto |
| Jog, encoders, BROWSE | CC relativo | se detecta solo: centro en 64 (65 = +1, 63 = -1) o complemento a 2 (1 = +1, 127 = -1) |

**Jog**: con VINYL encendido hace scratch (tocá el plato con "JOG TOUCH" asignado); sin VINYL, pitch bend.
128 pasos = una vuelta de disco.

## Luces (LEDs)
Los botones con luz (PLAY, PAUSE, CUE, SYNC, KEY LOCK, SLIP, VINYL, SHIFT, modos, pads, CUE de
auriculares, SPLIT, AUTO MIX, FX ON, LOOP) mandan su estado al controlador por la salida MIDI
con el **mismo nombre**, con el mismo mensaje que el botón (nota o CC, valor 127 / 0).

## Funciones mapeables
- **Deck 1 / Deck 2**: play, pause, playPause, cue, sync, keyLock, keyDown, keyUp, slip, vinyl, shift,
  modeHotCue, modeFx, modeSampler, modeLoop, loopIn, loopOut, loopHalve, loopDouble, pad1-pad4,
  jogTouch, jog, tempo, tempoRange, tempoNudge, load (carga la canción seleccionada en la lista).
- **Mixer**: canal 1/2 volume, gain, high, mid, low, filter, cue; crossfader, master, phones,
  cueMix, mixTime, split, autoMix.
- **FX 1 / FX 2**: on, wet, param, beatsDown, beatsUp.
- **Biblioteca**: browse (moverse por la lista), largeView.

## Archivo de mapeo (JSON)
Se guarda en `%APPDATA%\Castro Labs\KB Musiquero DJ\midi_mapping.json`. Opciones > MIDI permite
**exportarlo** (para compartirlo o hacer copia) e **importarlo**.

```json
{ "format": "kb-musiquero-midi", "version": 1,
  "bindings": [
    { "target": "deck1.play", "device": "DDJ-400", "channel": 1, "type": "note", "number": 11, "mode": "button", "sensitivity": 1.0 },
    { "target": "deck1.jog",  "device": "DDJ-400", "channel": 1, "type": "cc",   "number": 33, "mode": "relative64", "sensitivity": 1.0 },
    { "target": "deck1.tempo","device": "DDJ-400", "channel": 1, "type": "pitchbend", "number": 0, "mode": "absolute", "sensitivity": 1.0 }
  ] }
```
`type`: note / cc / pitchbend · `mode`: button / absolute / relative64 / relative2s ·
`device` vacío = cualquier controlador.

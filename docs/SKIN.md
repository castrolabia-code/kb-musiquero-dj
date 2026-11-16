# Skin — renders del diseño (enfoque híbrido)

KB - MUSIQUERO DJ dibuja todo por código (vectorial, nítido en cualquier resolución).
Para el **acabado final** se pueden agregar **renders en PNG** del chasis, los platos y el
mixer: si existen, el programa los usa en lugar del dibujo por código. Lo dinámico
(perillas, LEDs, botones, vúmetros, waveforms, el vinilo que gira, la carátula) se sigue
dibujando en tiempo real encima.

## Dónde van

Carpeta `skin` en cualquiera de estos lugares (se usa el primero que tenga la imagen):

1. Al lado del ejecutable: `C:\Program Files\KB Musiquero DJ\skin\`
2. En los datos del usuario: `%APPDATA%\Castro Labs\KB Musiquero DJ\skin\`

## Archivos

| Archivo | Qué es | Proporción | Tamaño @1x (1080p) |
|---|---|---|---|
| `chassis.png` | Chasis amarillo de toda la ventana (con sombreado 3D, sin controles) | 16:9 | 1920 × 1080 |
| `platter.png` | Base del plato: aro metálico y sombra. El centro (vinilo) puede quedar negro: el vinilo que gira se dibuja encima | 1:1 | 400 × 400 |
| `mixer.png` | Panel negro del mixer (textura, biseles) | ≈ 1:1,2 | 420 × 500 |

Para pantallas 1440p y 4K, agregar las versiones **@2x** y **@4x** con el mismo nombre:
`chassis@2x.png` (3840 × 2160), `chassis@4x.png` (7680 × 4320), etc. El programa elige
la más chica que no pixele en la pantalla actual.

## Recomendaciones

- PNG de 24 bits (chasis, mixer) o 32 bits con transparencia (plato: lo de afuera del aro transparente).
- Sin texto ni controles en los renders: los textos se traducen (ES/EN) y los controles son interactivos.
- Mantener el layout de la referencia (ver la captura actual de la app): el programa no
  mueve los controles según la imagen.
- Para volver al dibujo por código, borrar o renombrar el archivo.

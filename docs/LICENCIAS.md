# Licencias de dependencias — KB - MUSIQUERO DJ

Registro obligatorio: **toda dependencia se anota acá antes de incorporarse**, con su licencia y
qué implica para un producto comercial cerrado.

Leyenda: ✅ compatible · ⚠️ compatible con condiciones · ⏳ planificada (todavía no incorporada)

## Incorporadas

| Dependencia | Versión | Uso | Licencia | Estado | Obligaciones |
|---|---|---|---|---|---|
| **JUCE** | 8.0.15 | Framework de la app, audio, interfaz | AGPLv3 **o** licencia comercial JUCE | ⚠️ | Para vender sin publicar el código hace falta una **licencia comercial de JUCE** acorde a la facturación (planes Starter, Indie y Pro). **Revisar los términos vigentes en juce.com antes de la primera venta.** Las librerías que JUCE trae dentro (FLAC, Ogg Vorbis: BSD; zlib; etc.) tienen licencias permisivas y se listan en "Créditos" de la app en la Fase 9. |
| **Catch2** | 3.16.1 | Pruebas unitarias | BSL-1.0 | ✅ | Solo se usa en desarrollo; no se distribuye con el producto. |

## Opcionales (desactivadas por defecto)

| Dependencia | Uso | Licencia | Estado | Obligaciones |
|---|---|---|---|---|
| **ASIO SDK** (Steinberg) | Driver ASIO en Windows (`KB_ENABLE_ASIO=ON`) | GPLv3 **o** acuerdo de licencia propietario de Steinberg | ⚠️ | La opción GPLv3 **no sirve** para un producto cerrado. Para incluir ASIO hay que **firmar el acuerdo propietario de Steinberg** (es gratuito, se pide en steinberg.net/developers). El SDK **no se sube al repositorio**. Mientras tanto, WASAPI exclusivo cubre la baja latencia. El nombre/logo "ASIO" tiene reglas de uso de marca de Steinberg. |

## Planificadas

| Dependencia | Fase | Uso | Licencia | Estado | Notas |
|---|---|---|---|---|---|
| Windows Media Foundation | 1 | Decodificar M4A/AAC | Componente del SO | ✅ | No se redistribuye nada. |
| Decodificador MP3 de JUCE | 1 | MP3 | Incluido en JUCE | ✅ | Las patentes de MP3 vencieron en 2017. |
| r8brain-free-src | 1 | Conversión de sample rate de alta calidad | MIT | ✅ ⏳ | Requiere incluir el aviso de copyright. |
| Signalsmith Stretch | 3 | Time-stretch / Key Lock | MIT | ✅ ⏳ | Requiere incluir el aviso de copyright. |
| TagLib | 6 | Tags y carátulas | LGPL-2.1 **o** MPL-1.1 | ✅ ⏳ | Se enlaza como **DLL** (LGPL): el usuario debe poder reemplazarla; incluir texto de licencia y ofrecer el código de TagLib. |
| SQLite | 6 | Base de datos de la biblioteca | Dominio público | ✅ ⏳ | Sin obligaciones. |
| Orbitron / Rajdhani | 7 | Tipografía del skin | SIL OFL 1.1 | ✅ ⏳ | Se pueden embeber; incluir el texto de la OFL; no vender la fuente por separado. **Eurostile NO** (comercial de Linotype, requiere compra). |
| Inno Setup | 9 | Instalador | Licencia de Inno Setup (permite uso comercial) | ✅ ⏳ | Solo herramienta de build. |

## Descartadas

| Dependencia | Motivo |
|---|---|
| Rubber Band | GPL (o licencia comercial paga). Reemplazada por Signalsmith Stretch (MIT). |
| Windows Media Format de JUCE (`JUCE_USE_WINDOWS_MEDIA_FORMAT`) | API vieja; se usa Media Foundation directamente. |

## Recordatorios para el dueño del producto

1. **Licencia comercial de JUCE**: obligatoria antes de vender. Verificar el límite de facturación del plan elegido.
2. **ASIO**: firmar el acuerdo con Steinberg antes de activar `KB_ENABLE_ASIO` en builds que se distribuyan.
3. **Firma de código** (Fase 9): certificado de firma de código para Windows (OV/EV) para evitar avisos de SmartScreen.

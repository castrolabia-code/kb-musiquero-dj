# Licencias de dependencias — KB - MUSIQUERO DJ

Registro obligatorio: **toda dependencia se anota acá antes de incorporarse**, con su licencia y
qué implica para un producto comercial cerrado.

Leyenda: ✅ compatible · ⚠️ compatible con condiciones · ⏳ planificada (todavía no incorporada)

## Incorporadas

| Dependencia | Versión | Uso | Licencia | Estado | Obligaciones |
|---|---|---|---|---|---|
| **JUCE** | 8.0.15 | Framework de la app, audio, interfaz | AGPLv3 **o** licencia comercial JUCE | ⚠️ | Para vender sin publicar el código hace falta una **licencia comercial de JUCE** acorde a la facturación (planes Starter, Indie y Pro). **Revisar los términos vigentes en juce.com antes de la primera venta.** Las librerías que JUCE trae dentro (FLAC, Ogg Vorbis: BSD; zlib; etc.) tienen licencias permisivas y se listan en "Créditos" de la app en la Fase 9. |
| **r8brain-free-src** | 6.5 | Conversión de sample rate de alta calidad al cargar pistas | MIT | ✅ | Incluir el aviso de copyright (© Aleksey Vaneev) en "Créditos" y en la documentación del instalador (Fase 9). Usa la FFT de Ooura, incluida y de licencia permisiva. |
| **Windows Media Foundation** | (sistema operativo) | Decodificar M4A/AAC | Componente de Windows | ✅ | No se redistribuye nada. En Windows "N" (Europa) sin el Media Feature Pack, M4A no carga y la app muestra un error claro. |
| Decodificadores de JUCE (MP3, FLAC, Ogg Vorbis, WAV, AIFF) | (JUCE) | Formatos de audio | Incluidos en JUCE (FLAC/Vorbis: BSD) | ✅ | Las patentes de MP3 vencieron en 2017. |
| Reverb de JUCE (`juce::Reverb`, Freeverb) | (JUCE) | Efecto REVERB | Incluida en JUCE (misma licencia que JUCE) | ✅ | Sin obligaciones extra (cubierta por la licencia de JUCE). |
| **Signalsmith Stretch** | 1.4.0 | Key Lock (cambiar tempo sin cambiar tono) y key shift | MIT | ✅ | Incluir el aviso de copyright (© Geraint Luff / Signalsmith Audio Ltd) en "Créditos" (Fase 9). |
| **Signalsmith Linear** | 0.6.4 | FFT/STFT que usa Signalsmith Stretch | MIT | ✅ | Incluir el aviso de copyright (© Signalsmith Audio) en "Créditos". |
| **Orbitron** (Black, Bold) | 2018 (theleagueof/orbitron) | Logo y títulos | SIL OFL 1.1 | ✅ | Embebida en el ejecutable; texto de la licencia en `assets/fonts/OFL-Orbitron.txt` (se incluye con el instalador). No se vende la fuente por separado. |
| **Rajdhani** (SemiBold, Bold) | Google Fonts | Toda la interfaz | SIL OFL 1.1 | ✅ | Igual que Orbitron: `assets/fonts/OFL-Rajdhani.txt`. **Eurostile NO** (comercial de Linotype). |
| **TagLib** | 2.0.2 | Tags (título, artista, género, BPM, key) y carátulas de MP3, FLAC, OGG, M4A, WAV, AIFF | LGPL-2.1 (o MPL-1.1) | ⚠️ | Se compila como **biblioteca dinámica** (`tag.dll` / `libtag.so`) al lado del ejecutable: el usuario puede reemplazarla (requisito LGPL). Se incluye el texto de la licencia (`licenses/TagLib-LGPL-2.1.txt`) y se debe **ofrecer el código fuente de TagLib** (es el de github.com/taglib/taglib, tag v2.0.2, sin modificaciones): mencionarlo en "Créditos" y en el instalador (Fase 9). |
| **utfcpp** | (submódulo de TagLib) | UTF-8 dentro de TagLib | Boost Software License 1.0 | ✅ | Texto en `licenses/utfcpp-BSL-1.0.txt`. |
| **SQLite** | 3.49.2 | Base de datos de la biblioteca | Dominio público | ✅ | Sin obligaciones. Amalgamación incluida en `third_party/sqlite` (obtenida del paquete npm `better-sqlite3` 11.10.0, que trae la amalgamación oficial con un cambio cosmético: `30.0*86400.0` → `2592000.0`, el mismo valor). sqlite.org no es accesible desde el entorno de desarrollo. |
| **Catch2** | 3.16.1 | Pruebas unitarias | BSL-1.0 | ✅ | Solo se usa en desarrollo; no se distribuye con el producto. |

## Opcionales (desactivadas por defecto)

| Dependencia | Uso | Licencia | Estado | Obligaciones |
|---|---|---|---|---|
| **ASIO SDK** (Steinberg) | Driver ASIO en Windows (`KB_ENABLE_ASIO=ON`) | GPLv3 **o** acuerdo de licencia propietario de Steinberg | ⚠️ | La opción GPLv3 **no sirve** para un producto cerrado. Para incluir ASIO hay que **firmar el acuerdo propietario de Steinberg** (es gratuito, se pide en steinberg.net/developers). El SDK **no se sube al repositorio**. Mientras tanto, WASAPI exclusivo cubre la baja latencia. El nombre/logo "ASIO" tiene reglas de uso de marca de Steinberg. |

## Planificadas

| Dependencia | Fase | Uso | Licencia | Estado | Notas |
|---|---|---|---|---|---|
| Inno Setup | 9 | Instalador | Licencia de Inno Setup (permite uso comercial) | ✅ ⏳ | Solo herramienta de build. |

## Descartadas

| Dependencia | Motivo |
|---|---|
| Rubber Band | GPL (o licencia comercial paga). Reemplazada por Signalsmith Stretch (MIT). |
| Windows Media Format de JUCE (`JUCE_USE_WINDOWS_MEDIA_FORMAT`) | API vieja; se usa Media Foundation directamente. |
| Interpolador `WindowedSincInterpolator` de JUCE (para el cambio de sample rate) | No ajusta su filtro al bajar de frecuencia: con pistas de 96/48 kHz en un dispositivo de 44.1 kHz habría aliasing audible. r8brain sí lo hace (verificado por prueba: residuo < −100 dB). |

## Recordatorios para el dueño del producto

1. **Licencia comercial de JUCE**: obligatoria antes de vender. Verificar el límite de facturación del plan elegido.
2. **ASIO**: firmar el acuerdo con Steinberg antes de activar `KB_ENABLE_ASIO` en builds que se distribuyan.
3. **Firma de código** (Fase 9): certificado de firma de código para Windows (OV/EV) para evitar avisos de SmartScreen.

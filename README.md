# KB - MUSIQUERO DJ · by Castro Labs

Software DJ comercial de 2 decks para Windows 10/11 (x64), preparado para portarse a Android.
JUCE 8 + C++20 + CMake.

> Estado: **Fase 3 (parte 1) — BPM, SYNC, tempo, platos y lista de canciones** (v0.4.0) (ver [CHANGELOG.md](CHANGELOG.md)).
> El documento maestro del producto está en [CLAUDE.md](CLAUDE.md).

## Descargar el programa (sin compilar)

**https://github.com/castrolabia-code/kb-musiquero-dj/releases**

Bajar el `.zip` de la última versión, descomprimirlo y ejecutar `KB Musiquero DJ.exe`.
El ZIP ya trae las DLL de Visual C++, no hace falta instalar nada más.

Para publicar una versión nueva: crear y subir un tag `vX.Y.Z` (GitHub Actions arma el ZIP y el Release).

## Compilar en Windows

Requisitos:
- Visual Studio 2022 (o más nuevo) con el workload **"Desarrollo para el escritorio con C++"**
- CMake 3.25+ (viene con Visual Studio)
- Git

```bash
cmake --preset windows-x64-release
```

```bash
cmake --build --preset windows-x64-release --parallel
```

```bash
ctest --preset windows-x64-release
```

El ejecutable queda en `build/windows-x64-release/src/app/KBMusiqueroDJ_artefacts/Release/`.
Para depurar: configurar con `windows-x64-debug` y abrir la solución de Visual Studio que se genera en `build/windows-x64-debug/`.

La primera configuración descarga JUCE y Catch2 (por eso tarda un par de minutos).

### ASIO (opcional)

Desactivado por defecto. Requiere el ASIO SDK de Steinberg y **firmar su acuerdo de licencia
propietario** para uso comercial (ver [docs/LICENCIAS.md](docs/LICENCIAS.md)):

```bash
cmake --preset windows-x64-release -DKB_ENABLE_ASIO=ON -DKB_ASIO_SDK_DIR=C:/sdk/asiosdk
```

## Compilar en Linux (solo desarrollo y pruebas)

```bash
cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug
```

## Documentación

- [docs/ARQUITECTURA.md](docs/ARQUITECTURA.md): módulos, hilos y reglas de tiempo real
- [docs/LICENCIAS.md](docs/LICENCIAS.md): dependencias y licencias
- [CHANGELOG.md](CHANGELOG.md)

© Castro Labs. Todos los derechos reservados.

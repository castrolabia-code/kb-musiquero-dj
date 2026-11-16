# licensing/ — Fase 9

Activación comercial: clave de producto con validación offline firmada (criptografía asimétrica:
la app solo contiene la clave pública) y validación online preparada.

## Generador de licencias con ventana

`KB Generador de Licencias.exe` (target `KBLicenseGenerator`, fuente `tools/license_generator/Main.cpp`)
hace lo mismo que `kb_license_tool issue` pero con interfaz: ver `tools/license_generator/LEEME.txt`.
Se publica como artefacto de CI separado y NO se incluye en el instalador del cliente.

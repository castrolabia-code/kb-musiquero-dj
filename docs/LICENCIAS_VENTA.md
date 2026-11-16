# Licencias de venta (claves de producto)

El programa **no arranca sin una clave válida** (no hay modo de prueba). Las claves se
firman con RSA 2048 + SHA-256 y se verifican **sin internet** con la clave pública que
viene dentro del programa (`src/licensing/PublicKey.h`).

## La clave privada
- Se generó con `kb_license_tool genkeys` y se entregó **aparte** (no está en el repositorio).
- **Guardala en un lugar seguro** (gestor de contraseñas + copia offline). Quien la tenga puede fabricar licencias.
- Si se pierde o se filtra: generar un par nuevo, reemplazar `PublicKey.h` y publicar una versión
  nueva (las claves emitidas con la anterior dejan de valer en esa versión).

## Emitir una licencia para un cliente
Compilar el proyecto (genera `kb_license_tool`) y ejecutar:

```
kb_license_tool issue private.key "Nombre del cliente" cliente@correo.com
```

Opciones:
- `--expires 2027-12-31` → licencia con vencimiento (por defecto, perpetua).
- `--machine 114789DF849C579A` → atada a una PC (el cliente ve el "ID de esta PC" en la ventana de activación).
- `--edition Pro` → edición (se muestra en "Acerca de").

Para verificar una clave: `kb_license_tool verify public.key <clave>`.

## Validación online (preparada)
`licensing::onlineValidationUrl()` devuelve vacío. Cuando exista un servidor de licencias,
poner ahí su dirección para activar / revocar claves por internet.

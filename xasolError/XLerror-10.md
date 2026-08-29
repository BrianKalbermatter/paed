# XL-10 — Archivos: modo de apertura y organización

## Qué dice

```
error XL-10: modo de apertura invalido en 'ABRIR X/(mae)': los de ABRIR son E, S, ES
error XL-10: 'mae' no se puede ordenar por campos: 'reg' no es un REGISTRO declarado
error XL-10: el campo 'clave' esta dos veces en la clave de 'mae'
error XL-10: a 'mae' le faltan los campos de la clave
error XL-10: 'mae' lleva un solo campo y 'clave, otro' tiene 2
```

## Por qué pasa

Dos familias de problemas con archivos.

**El modo de apertura.** Un archivo se abre diciendo para qué:

| Modo | Para qué |
|---|---|
| `E/` | leer |
| `S/` | escribir |
| `E/S` | las dos, sobre el mismo archivo |

**La cláusula de organización.** Los campos de la clave tienen que existir en
el `REGISTRO` del archivo, y `INDEXADO POR` lleva **uno solo**:

```paed
mae: ARCHIVO DE remedio ORDENADO POR farmacia, medicamento;
arch: ARCHIVO DE reg INDEXADO POR clave;
```

Esta validación existe para atajar el error que si no aparece mucho después,
disfrazado de datos desordenados en la salida.

## Cómo se arregla

```paed
ABRIR E/(mae);
ABRIR S/(nuevo);
ABRIR E/S(arch_mae);
```

Y revisá que cada campo de la cláusula esté declarado en el `REGISTRO` que le
diste al archivo.

## Ver también

- `docs/ARCHIVOS.md` — el juego de archivos completo
- [XL-11](XLerror-11.md) — el `REGISTRO` del archivo

---

Volver al [índice de errores](../errores.md).

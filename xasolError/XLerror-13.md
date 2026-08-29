# XL-13 — Se pasó un límite del lenguaje

## Qué dice

```
error XL-13: demasiadas instrucciones (maximo 256)
error XL-13: demasiadas declaraciones (maximo 64)
error XL-13: demasiados bloques anidados (maximo 32)
error XL-13: demasiadas subacciones (maximo 12)
error XL-13: no entran mas variables ('x')
```

## Por qué pasa

PAED tiene límites fijos y los alcanzaste. No son arbitrarios: el programa
entero vive en memoria estática, así que cada límite es un tamaño reservado de
antemano.

| Qué | Máximo |
|---|---|
| Instrucciones del `PROCESO` | 256 |
| Declaraciones del `AMBIENTE` | 64 |
| Bloques anidados | 32 |
| Subacciones | 12 |
| Campos de un registro | 16 |
| Campos de una clave | 4 |

## Cómo se arregla

Casi siempre no es que el programa sea grande, es que hay algo repetido que
pide ser una subacción. Un ejercicio de cátedra completo — una actualización
secuencial con sus seis casos — entra sin problema en esos límites.

Si de verdad hace falta más, los límites están en `lang/include/paed/parser.h`
como `PAED_MAX_INSTRS`, `PAED_MAX_DECLS` y compañía.

## 32 bloques anidados

Ese no se alcanza escribiendo bien. Si llegaste, casi seguro hay un cierre que
falta y PAED sigue creyendo que todo lo de abajo está adentro — mirá
[XL-04](XLerror-04.md) antes de subir el límite.

---

Volver al [índice de errores](../errores.md).

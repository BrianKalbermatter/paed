# XL-04 — Bloque sin cerrar, o cierre que no corresponde

## Qué dice

```
error XL-04: falta FIN_SI: el SI de la linea 4 quedo abierto
error XL-04: FIN_SI sin un SI abierto
error XL-04: FIN_SI cierra un MIENTRAS abierto en la linea 8
error XL-04: SINO repetido: el SI de la linea 3 ya tiene uno
```

## Por qué pasa

Los bloques de PAED se abren y se cierran, y tienen que quedar **anidados**: el
que se abre último se cierra primero.

| Abre | Cierra |
|---|---|
| `SI ... ENTONCES` | `FIN_SI` |
| `MIENTRAS ... HACER` | `FIN_MIENTRAS` |
| `PARA ... HACER` | `FIN_PARA` |
| `REPETIR` | `HASTA QUE ...` |
| `SEGUN ... HACER` | `FIN_SEGUN` |
| `REGISTRO` | `FIN_REGISTRO` |

Los tres casos:

**Falta el cierre.** El más común. El mensaje te dice **en qué línea se abrió**
el bloque que quedó colgado — eso es lo que hay que mirar, no la línea del
error, que es donde PAED se dio cuenta.

**Cierre de más.** Un `FIN_SI` sin ningún `SI` abierto: o sobra, o el `SI` está
más arriba de lo que creés y ya se cerró.

**Cierre cruzado.** `FIN_SI` cerrando un `MIENTRAS`. Casi siempre es un cierre
que falta más adentro:

```paed
MIENTRAS (i < 10) HACER
    SI (a = 1) ENTONCES
        ESCRIBIR("hola");
FIN_MIENTRAS          ← el SI nunca se cerró
```

## Cómo se arregla

Indentá. En serio: la sangría no cambia nada para PAED, pero un bloque sin
cerrar se ve de una cuando los cierres están alineados con sus aperturas.

## Ver también

- [XL-11](XLerror-11.md) — cuando el que quedó abierto es un `REGISTRO`
- [XL-05](XLerror-05.md) — cuando el bloque abre pero le falta la condición

---

Volver al [índice de errores](../errores.md).

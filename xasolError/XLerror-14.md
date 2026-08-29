# XL-14 — El programa no termina

## Qué dice

```
error XL-14: el programa paso los 2000000 pasos sin terminar (bucle infinito?)
```

## Por qué pasa

PAED cuenta las instrucciones que ejecuta y corta a los dos millones. No es un
límite de tiempo: es un seguro contra el bucle que no avanza.

Un ejercicio de cátedra, aun recorriendo un archivo entero, no llega ni cerca.

## Las tres causas

**La condición nunca se hace falsa.**

```paed
i := 0;
MIENTRAS (i < 10) HACER
    ESCRIBIR(i);        ← i nunca cambia
FIN_MIENTRAS
```

**Falta el avance dentro del ciclo.** Es el clásico de archivos y secuencias: el
`LEER` de abajo del ciclo es el que hace que `FDA` se vuelva verdadero.

```paed
LEER(mae, reg);
MIENTRAS NFDA(mae) HACER
    tratar_registro;
    LEER(mae, reg);     ← sin esto, no termina nunca
FIN_MIENTRAS
```

**El avance está, pero adentro de un `SI`** que no siempre se cumple. El avance
va en el ciclo, no en una rama.

## Adentro de xasol

La consola tiene además su propio corte a los 5 segundos, con `timeout`. Si ves
`-- cortado: el programa paso los 5 segundos --` en vez de este error, es que
se colgó en otro lado — casi siempre un `LEER` esperando datos que no llegan,
que es [XL-15](XLerror-15.md).

---

Volver al [índice de errores](../errores.md).

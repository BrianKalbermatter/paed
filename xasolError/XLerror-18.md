# XL-18 — Se usa una variable antes de darle valor

## Qué dice

```
programa.paed:6: error XL-18: 'b' esta declarado pero todavia no tiene valor: hay que asignarle algo antes de usarlo
programa.paed:9: error XL-18: el campo 'reg.cant' no tiene valor todavia
```

## Por qué pasa

La variable **existe** — está declarada en el `AMBIENTE` — pero nunca le
asignaste nada. Declarar crea la cajita; no la llena.

```paed
AMBIENTE
    a: ENTERO;
    b: ENTERO;
PROCESO
    a := b + 1;      ← b existe, pero está vacía
```

## Cómo se arregla

Dale un valor antes de usarla. De las tres formas que hay:

```paed
b := 5;              ← asignándole
LEER(b);             ← pidiéndola por consola
LEER(arch, reg);     ← trayéndola de un archivo
```

## No confundirlo con XL-03

Son dos errores distintos y por un tiempo PAED los decía igual, lo que hacía
perder mucho tiempo:

| | Qué pasó | Qué buscar |
|---|---|---|
| **XL-18** (este) | está declarada, falta el valor | el `:=` o el `LEER` que falta |
| [XL-03](XLerror-03.md) | **no está declarada** | un nombre mal escrito, o la declaración que falta |

Si el mensaje dice "no está declarado", no busques un `:=` — buscá el nombre.
Casi siempre es una mayúscula distinta, o un **parámetro usado afuera de su
subacción**:

```paed
FUNCION cuadrado(E number: ENTERO): ENTERO
    PROCESO
        cuadrado := number ** 2;     ← acá number existe
FIN_FUNCION

PROCESO
    LEER(num);
    r := cuadrado(number);           ← acá NO: el que tiene el 7 es num
```

Ese es el caso que más engaña: acabás de escribir `LEER(num)` y el error habla
de `number`, así que parece que el valor se perdió. No se perdió — son dos
nombres distintos, y `number` sólo vive adentro de la función.

## Ver también

- [XL-03](XLerror-03.md) — el nombre no existe
- [XL-16](XLerror-16.md) — el nombre existe y el dato no le sirve

---

Volver al [índice de errores](../errores.md).

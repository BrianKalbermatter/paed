# XL-09 — Cantidad de argumentos incorrecta

## Qué dice

```
error XL-09: 'sumar' lleva 2 argumento(s) y se le pasaron 3
error XL-09: AVZ sobre la secuencia 'sec' lleva exactamente 2 argumentos: AVZ(sec, dato)
error XL-09: LEER necesita al menos un destino: LEER(x)
error XL-09: demasiados argumentos (maximo 16)
```

## Por qué pasa

Llamaste bien pero con la cantidad equivocada. La firma manda: si declaraste
dos parámetros, la llamada lleva dos argumentos.

## Cómo se arregla

Comparalos lado a lado:

```paed
FUNCION sumar(a: ENTERO; b: ENTERO): ENTERO    ← dos parámetros
...
r := sumar(3, 5);                                   ← dos argumentos
```

## Parámetros y argumentos no son lo mismo

Los **parámetros** son los nombres de la declaración — cajitas vacías. Los
**argumentos** son los valores reales de la llamada, los que van en esas
cajitas. Confundirlos es lo que lleva a usar el nombre del parámetro afuera de
la subacción, que es [XL-03](XLerror-03.md).

## Ver también

- [XL-08](XLerror-08.md) — cuando lo que llamás directamente no existe

---

Volver al [índice de errores](../errores.md).

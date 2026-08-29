# XL-15 — Se acabaron los datos de entrada

## Qué dice

```
error XL-15: la entrada se termino antes de darle un valor a 'num'
error XL-15: LEER no tiene de donde sacar los datos
```

## Por qué pasa

El programa llegó a un `LEER` y no había nada del otro lado.

Es **el error más confuso de la consola de xasol**, porque el programa parece
correcto. Y lo está: el problema es que ahí nadie tipea.

## Cómo se arregla

Ponele los datos adentro del `.paed`, en un bloque de comentarios al final:

```paed
ACCION paed ES
    AMBIENTE
        num: ENTERO;
    PROCESO
        ESCRIBIR("Introduzca un numero: ");
        LEER(num);
        ESCRIBIR("leí ", num);
FIN_ACCION

// ── ENTRADA ──
// 7
```

Una línea por cada `LEER`, en orden. `F10` lee ese bloque y se lo pasa al
programa; sin él, la entrada llega vacía y sale este error.

Es la misma idea que el bloque `SALIDA ESPERADA` de los tests y que los datos
de una `SECUENCIA`: un programa es **un** archivo, y los datos del enunciado
son parte de él.

## Por qué la consola no te deja tipear directamente

Porque la consola espera a que el proceso termine. Un `LEER` esperando a que
escribas colgaría PseudoGames entero — la consola esperando al programa y el
programa esperándote a vos. Con los datos adentro del archivo, `F10` sigue
siendo un botón que siempre vuelve.

**Para correrlo a mano**, desde la terminal, con los datos por tubería:

```bash
echo 7 | paed Hola.paed
```

## Ver también

- [XL-14](XLerror-14.md) — el otro motivo por el que un programa no termina

---

Volver al [índice de errores](../errores.md).

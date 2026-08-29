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

## Por qué la consola no te deja tipear

`F10` corre el programa con la entrada **cerrada** (`< /dev/null`). Si no, un
`LEER` se quedaría esperando para siempre y colgaría PseudoGames entero: la
consola espera a que el proceso termine, y el proceso espera a que alguien
escriba.

## Cómo se arregla

**Para probar desde la consola**, ponele los datos adentro del `.paed`, en un
bloque al final:

```paed
// ── ENTRADA ──
// 7
```

Es la misma idea que el bloque `SALIDA ESPERADA` de los tests: un programa es
**un** archivo, y los datos del enunciado son parte del programa.

**Para correrlo a mano**, desde la terminal, con los datos por tubería:

```bash
echo 7 | paed Hola.paed
```

## Ver también

- [XL-14](XLerror-14.md) — el otro motivo por el que un programa no termina

---

Volver al [índice de errores](../errores.md).

# XL-03 — Nombre que no está en el AMBIENTE

## Qué dice

```
error XL-03: 'cuadrado' no esta declarado en el AMBIENTE
error XL-03: 'Sec' no esta declarado, pero si 'sec' (linea 10): los identificadores distinguen mayusculas
error XL-03: ARR trabaja sobre una secuencia y 'fantasma' no esta declarado en el AMBIENTE
```

## Por qué pasa

Usaste un nombre que nunca declaraste. En PAED **todo se declara antes de
usarse**, en el `AMBIENTE`.

Hay tres variantes, y la segunda es la que más tiempo hace perder:

**1. Te olvidaste de declararla.**

```paed
PROCESO
    cuadrado := 4;      ← cuadrado no existe
```

**2. Está declarada pero con otras mayúsculas.** PAED distingue: `sec` y `Sec`
son dos nombres distintos. Cuando pasa esto el mensaje te dice cuál encontró y
en qué línea, justamente porque el error es difícil de ver leyendo.

**3. Es un parámetro y lo estás usando afuera.** Un parámetro vive **solo
adentro** de su subacción:

```paed
FUNCION devolverCuadro(E number: ENTERO): ENTERO
    PROCESO
        devolverCuadro := number ** 2;    ← acá number existe
FIN_FUNCION

PROCESO
    cuadrado := devolverCuadro(number);   ← acá NO
```

Afuera, lo que tenés es la variable que vas a pasarle.

## Cómo se arregla

Declarala en el `AMBIENTE` con su tipo:

```paed
AMBIENTE
    cuadrado: ENTERO;
```

## Ver también

- [XL-07](XLerror-07.md) — cuando la declaración existe pero está mal escrita

---

Volver al [índice de errores](../errores.md).

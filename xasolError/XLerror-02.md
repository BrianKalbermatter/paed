# XL-02 — Declaración de subacción mal formada

## Qué dice

```
error XL-02: 'calcular' es una FUNCION y no dice que tipo devuelve
error XL-02: parametro sin tipo en 'sumar': se escribe 'E nombre: TIPO'
error XL-02: 'X' no es un modo de parametro: los modos son E, S, ES y VAR
error XL-02: parametro sin nombre en 'sumar'
```

## Por qué pasa

La firma de una subacción tiene partes obligatorias, y falta alguna.

Una `FUNCION` **siempre** dice de qué tipo es lo que devuelve, porque su
llamada va adentro de una expresión y el lenguaje tiene que saber con qué está
operando:

```paed
FUNCION sumar(a: ENTERO; b: ENTERO): ENTERO
                                       ─┬────
                                        └── esto no es opcional
```

Un `PROCEDIMIENTO` es al revés: **no** lleva tipo, porque no devuelve nada.
Ponérselo también es este error.

## Cómo se arregla

Cada parámetro se escribe `nombre: TIPO`, y van separados por `;`:

```paed
FUNCION sumar(a: ENTERO; b: ENTERO): ENTERO
PROCEDIMIENTO cargar(sec: SECUENCIA DE CARACTER)
```

## `E` y `S` NO van en los parámetros

Es el error de notación más fácil de cometer, porque esas letras existen en
PAED — pero **son el modo de apertura de un ARCHIVO**:

```paed
ABRIR E/(mae);        ← acá sí: el archivo se abre para leer
ABRIR E/S(arch);      ← para leer y escribir

FUNCION sumar(a: ENTERO; b: ENTERO): ENTERO    ← acá no va ningún modo
```

Medido sobre la teoría de la cátedra: **cero** apariciones de `E` como modo de
parámetro, y varias de `E/` en `ABRIR`. La forma con modo aparece en la wiki,
que arrastra errores conocidos.

PAED acepta las dos, así que un programa con `E` en un parámetro corre igual.
En un parcial escribilo sin modo.

## Ver también

- [XL-01](XLerror-01.md) — cuando la firma está bien pero falta el `PROCESO`
- [XL-09](XLerror-09.md) — cuando la firma está bien y la llamada no

---

Volver al [índice de errores](../errores.md).

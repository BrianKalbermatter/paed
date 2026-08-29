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
FUNCION sumar(E a: ENTERO; E b: ENTERO): ENTERO
                                       ─┬────
                                        └── esto no es opcional
```

Un `PROCEDIMIENTO` es al revés: **no** lleva tipo, porque no devuelve nada.
Ponérselo también es este error.

## Cómo se arregla

Cada parámetro se escribe `MODO nombre: TIPO`, y van separados por `;`:

```paed
FUNCION sumar(E a: ENTERO; E b: ENTERO): ENTERO
PROCEDIMIENTO cargar(VAR sec: SECUENCIA DE CARACTER)
```

Los cuatro modos:

| Modo | Significa |
|---|---|
| `E` | Entrada — el dato entra y no se modifica afuera |
| `S` | Salida — la subacción devuelve un dato por ahí |
| `ES` | Entrada/Salida — entra con valor y puede salir cambiado |
| `VAR` | Por referencia. Es la que más aparece en los templates de cátedra |

## Ver también

- [XL-01](XLerror-01.md) — cuando la firma está bien pero falta el `PROCESO`
- [XL-09](XLerror-09.md) — cuando la firma está bien y la llamada no

---

Volver al [índice de errores](../errores.md).

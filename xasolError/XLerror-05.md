# XL-05 — Falta la condición de un bloque de control

## Qué dice

```
error XL-05: el SI no tiene condicion
error XL-05: se esperaba: SI <condicion> ENTONCES
error XL-05: el MIENTRAS no tiene condicion
error XL-05: al HASTA le falta la condicion: HASTA QUE <condicion>
```

## Por qué pasa

Escribiste la palabra que abre el bloque pero no lo que decide.

```paed
SI ENTONCES              ← ¿si qué?
MIENTRAS HACER           ← ¿mientras qué?
```

También pasa cuando la condición está pero le falta la palabra del final
(`ENTONCES`, `HACER`), porque sin ella PAED no sabe dónde termina lo que tiene
que evaluar.

## Cómo se arregla

Cada uno tiene su forma completa:

```paed
SI (nota >= 6) ENTONCES
    ...
FIN_SI

MIENTRAS (i < 10) HACER
    ...
FIN_MIENTRAS

REPETIR
    ...
HASTA QUE (respuesta = 'N')
```

## Los paréntesis

No son obligatorios, pero ayudan cuando la condición tiene varias partes:

```paed
SI (edad >= 18) Y (tiene_dni = V) ENTONCES
```

## Ver también

- [XL-06](XLerror-06.md) — el `PARA`, que tiene su propia forma
- [XL-04](XLerror-04.md) — cuando el bloque abre bien y no cierra

---

Volver al [índice de errores](../errores.md).

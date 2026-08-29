# XL-06 — PARA mal escrito

## Qué dice

```
error XL-06: al PARA le falta ':=' con el valor inicial
error XL-06: al PARA le falta HASTA con el valor final
error XL-06: al PARA le falta el valor inicial
error XL-06: el PARA tiene ',' pero no dice el incremento
```

## Por qué pasa

El `PARA` es el bloque con más partes de PAED, y todas son obligatorias:

```paed
PARA i := 1 HASTA 10 HACER
    ─┬─ ─┬── ────┬──── ──┬──
     │   │       │       └── abre el bloque
     │   │       └── hasta dónde
     │   └── desde dónde
     └── la variable que cuenta
```

## Cómo se arregla

```paed
PARA i := 1 HASTA 10 HACER
    ESCRIBIR(i);
FIN_PARA
```

Con incremento distinto de 1:

```paed
PARA i := 10 HASTA 1, -1 HACER
```

Si escribís la coma tenés que poner el incremento — una coma suelta es el
cuarto mensaje de arriba.

## Ver también

- [XL-05](XLerror-05.md) — los otros bloques de control
- [XL-03](XLerror-03.md) — la variable del `PARA` también se declara en el `AMBIENTE`

---

Volver al [índice de errores](../errores.md).

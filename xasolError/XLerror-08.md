# XL-08 — Procedimiento o función que no existe

## Qué dice

```
error XL-08: procedimiento desconocido 'inexistente' (no esta en sintaxis.json ni es una subaccion de este programa)
error XL-08: subaccion desconocida
error XL-08: nombre de procedimiento invalido: '3x'
```

## Por qué pasa

Llamaste a algo que PAED no conoce. Busca en dos lados, y el mensaje te dice en
cuáles miró:

1. Los procedimientos del lenguaje, definidos en `data/sintaxis.json`
   (`ESCRIBIR`, `LEER`, `ABRIR`, `CERRAR`, `ARR`, `AVZ`, …).
2. Las subacciones **de este programa**.

Si pediste una librería con `USAR`, también la nombra.

## Cómo se arregla

Las tres causas, en orden de frecuencia:

**Está mal escrito.** `MOVERR` por `MOVER`, `ESCRIBER` por `ESCRIBIR`.

**Es una subacción tuya y no está declarada**, o está declarada **debajo** de
donde la llamás.

**Te falta el `USAR`.** Un procedimiento de librería necesita que la pidas:

```paed
USAR escena;
```

## Ver también

- [XL-02](XLerror-02.md) — cuando la subacción existe pero su firma está mal
- [XL-09](XLerror-09.md) — cuando existe y le pasaste mal los argumentos

---

Volver al [índice de errores](../errores.md).

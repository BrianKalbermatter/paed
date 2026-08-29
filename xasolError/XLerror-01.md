# XL-01 — Falta un bloque de la subacción

## Qué dice

```
Hola.paed:6: error XL-01: la subaccion 'devolverCuadro' no tiene bloque PROCESO
Hola.paed:5: error XL-01: instruccion antes del PROCESO de la subaccion 'devolverCuadro'
```

## Por qué pasa

Una `FUNCION` o un `PROCEDIMIENTO` **no son una declaración más**. Son una
subacción, y una subacción tiene la misma estructura que el programa: su propio
`AMBIENTE` (opcional) y su propio `PROCESO`.

Escribir el cuerpo pegado a la firma, sin el `PROCESO` en el medio, es el error
más común:

```paed
FUNCION devolverCuadro(number: ENTERO): ENTERO
    devolverCuadro := number ** 2;      ← ¿en qué bloque va esto?
FIN_FUNCION
```

Ahí la instrucción está en tierra de nadie: la firma ya terminó y el `PROCESO`
no empezó. De ahí los dos mensajes — uno por la instrucción huérfana y otro
porque el bloque nunca apareció.

## Cómo se arregla

```paed
FUNCION devolverCuadro(number: ENTERO): ENTERO
    PROCESO
        devolverCuadro := number ** 2;
FIN_FUNCION
```

El `AMBIENTE` de la subacción es opcional; el `PROCESO` no.

## Dónde va la subacción

**Dentro del `AMBIENTE`**, como todo lo demás **[cát]**:

```paed
ACCION paed ES
    AMBIENTE
        num: ENTERO;
        cuadrado: ENTERO;

        FUNCION devolverCuadro(number: ENTERO): ENTERO
            PROCESO
                devolverCuadro := number ** 2;
        FIN_FUNCION

    PROCESO
        LEER(num);
        cuadrado := devolverCuadro(num);
FIN_ACCION
```

## Lo que se lleva puesto

Una `FUNCION` retorna **asignándole a su propio nombre**: `devolverCuadro := ...`.
No hay `RETORNAR` en el camino normal.

Y los parámetros van **sin modo**: `(number: ENTERO)`. El `E`/`S`/`E/S` que
existe en PAED es el modo de apertura de un ARCHIVO — `ABRIR E/(mae)` — no de
un parámetro. Ver [XL-02](XLerror-02.md).

## Ver también

- `docs/PAED.md` — subacciones
- [XL-02](XLerror-02.md) — cuando el problema está en la firma, no en el bloque

---

Volver al [índice de errores](../errores.md).

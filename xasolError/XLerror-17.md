# XL-17 — La estructura del programa está fuera de orden

## Qué dice

```
programa.paed:1: error XL-17: AMBIENTE va justo despues de ACCION ... ES y antes de PROCESO
programa.paed:1: error XL-17: falta ACCION <nombre> ES
programa.paed:8: error XL-17: instruccion despues de FIN_ACCION
programa.paed:3: error XL-17: USAR va ANTES de la ACCION: una libreria se pide al principio
```

## Por qué pasa

Un programa PAED tiene **un orden fijo**, y no es decorativo: cada bloque
significa algo distinto y el intérprete necesita saber en cuál está parado.

```paed
USAR escena;              ← si hay librerías, van primero de todo

ACCION nombre ES          ← abre el programa
AMBIENTE                  ← qué existe: variables, registros, subacciones
PROCESO                   ← qué hace
FIN_ACCION                ← cierra, y después no va nada
```

Los cuatro casos:

**Falta el `ACCION ... ES`.** Empezaste por el `AMBIENTE`. Sin la cabecera el
programa no tiene ni nombre ni principio.

**Falta el `PROCESO`.** Declaraste todo y no hay nada que ejecutar.

**Hay algo después del `FIN_ACCION`.** Ahí el programa ya terminó. Si es un
comentario está bien; si es código, no se va a ejecutar nunca.

**El `USAR` está adentro.** Una librería se pide antes de abrir la `ACCION`,
porque tiene que estar disponible desde la primera línea.

## Cómo se arregla

El esqueleto mínimo que corre:

```paed
ACCION ejemplo ES
AMBIENTE
    x: ENTERO;
PROCESO
    x := 1;
    ESCRIBIR("x vale ", x);
FIN_ACCION
```

## El AMBIENTE es opcional, el PROCESO no

Un programa que sólo escribe algo no necesita declarar nada, pero igual lleva
la cabecera y el cierre:

```paed
ACCION hola ES
AMBIENTE
PROCESO
    ESCRIBIR("hola");
FIN_ACCION
```

## Ver también

- [XL-01](XLerror-01.md) — el mismo problema pero adentro de una subacción
- [XL-04](XLerror-04.md) — bloques de control sin cerrar

---

Volver al [índice de errores](../errores.md).

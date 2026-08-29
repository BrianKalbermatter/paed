# XL-12 — Asignación mal formada

## Qué dice

```
error XL-12: falta la expresion a la derecha de ':=' en 'x'
error XL-12: destino de asignacion invalido: '3'
error XL-12: falta ']' en el destino 'notas[2'
error XL-12: falta el indice en el destino 'notas[]'
```

## Por qué pasa

Una asignación es `destino := expresión;` y falta o sobra algo de un lado.

El **destino** tiene que ser algo donde se pueda guardar: una variable, una
posición de arreglo, un campo de registro, o el nombre de la función cuando
estás retornando. Un número no es un destino.

## Cómo se arregla

```paed
x := 3;
notas[2] := 9;
reg.nombre := "Ana";
sumar := a + b;        ← el retorno de una FUNCION
```

## `:=` y `=` no son lo mismo

En PAED se separan y es una de las cosas que más se mezclan al venir de otros
lenguajes:

| | Qué hace |
|---|---|
| `:=` | **asigna** — guarda un valor |
| `=` | **compara**, y también declara una constante o un registro |

```paed
SI (a = 1) ENTONCES     ← compara
    a := 2;             ← asigna
FIN_SI
```

## Ver también

- [XL-03](XLerror-03.md) — cuando el destino no está declarado

---

Volver al [índice de errores](../errores.md).

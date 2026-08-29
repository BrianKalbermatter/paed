# XL-07 — Declaración de variable mal formada

## Qué dice

```
error XL-07: declaracion invalida: se esperaba nombre: TIPO;
error XL-07: nombre de variable invalido: '2num'
error XL-07: falta el tipo de 'edad'
error XL-07: al arreglo 'notas' le falta 'DE <tipo>'
error XL-07: a la secuencia 'sec' le falta el tipo despues de 'DE'
```

## Por qué pasa

Una declaración se escribe `nombre: TIPO;` y falta alguna de las tres partes, o
el nombre no es válido.

Un nombre **no puede empezar con un número** ni llevar espacios ni símbolos.
Letras, dígitos y guión bajo, empezando por letra.

## Cómo se arregla

Los tipos simples:

```paed
AMBIENTE
    edad: ENTERO;
    promedio: REAL;
    inicial: CARACTER;
    aprobo: LOGICO;
    nombre: AN(20);
```

Los que son envoltorios de otro tipo llevan `DE`:

```paed
    notas: ARREGLO[1..10] DE ENTERO;
    sec: SECUENCIA DE CARACTER;
    mae: ARCHIVO DE remedio;
```

Ahí es donde aparecen los mensajes de "le falta `DE <tipo>`": un arreglo, una
secuencia y un archivo **guardan algo**, y hay que decir qué.

## Ver también

- [XL-11](XLerror-11.md) — declarar un `REGISTRO`, que tiene su propia forma
- [XL-03](XLerror-03.md) — cuando la declaración falta del todo

---

Volver al [índice de errores](../errores.md).

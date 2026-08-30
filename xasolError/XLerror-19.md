# XL-19 — CONJUNTO mal declarado, o usado sin declarar

## Qué dice

```
c.paed:3: error XL-19: falta '}' para cerrar el conjunto: se escribe nombre = {a, b, c};
c.paed:3: error XL-19: conjunto sin elementos: 'vocales' no tiene ninguno
c.paed:3: error XL-19: nombre de conjunto invalido: '2mal'
c.paed:3: error XL-19: el conjunto 'vocales' tiene un elemento vacio
c.paed:3: error XL-19: el conjunto 'v' ya estaba declarado en la linea 3
mal.paed:6: error XL-19: 'vocales' no es un conjunto declarado: se declara en el AMBIENTE con vocales = {a, b, c};
```

## Por qué pasa

Un **conjunto** es una lista fija de valores que se declara en el AMBIENTE con
`=` y llaves, y contra la que después se pregunta con `EN`:

```paed
AMBIENTE
    vocales = {"A", "E", "I", "O", "U"};
```

Se declara con `=` y no con `:` por lo mismo que un `REGISTRO`: no es una
variable de un tipo, es algo que vos **definís**.

Las causas, en orden de cuánto tiempo hacen perder:

**1. Se usó un conjunto que no está en el AMBIENTE.** Es la más común, y casi
siempre es el nombre mal escrito o el conjunto declarado en otro programa.

```paed
ACCION t ES
AMBIENTE
    v: CARACTER;
PROCESO
    v := "B";
    SI (v EN vocales) ENTONCES        // 'vocales' no existe
        ESCRIBIR(1);
    FIN_SI
FIN_ACCION
```

**2. Falta la llave que cierra.** El `;` termina la declaración, pero el `}`
termina el conjunto, y son dos cosas distintas.

**3. El conjunto está vacío**, o tiene una coma de más que deja un hueco:
`{1, , 2}`. Un conjunto sin elementos no puede contener nada, así que toda
pregunta con `EN` daría falso siempre — eso no es un conjunto, es un error de
tipeo.

**4. El nombre no es un identificador válido**, o ya se había declarado antes.

## Cómo se arregla

```paed
ACCION t ES
AMBIENTE
    vocales = {"A", "E", "I", "O", "U"};
    v: CARACTER;
PROCESO
    v := "B";
    SI (v NO EN vocales) ENTONCES
        ESCRIBIR(v, " es consonante");
    FIN_SI
FIN_ACCION
```

```
B es consonante
```

## Lo que se lleva puesto: `EN` es una comparación

`EN` está en el mismo nivel de prioridad que `=` y `<>`. No es una instrucción
del `SI`: es un operador, y por eso anda igual en un `SI`, en un `MIENTRAS` y en
el `HASTA` de un `REPETIR` — los tres evalúan su condición con el mismo
evaluador.

Las comillas de los elementos son del literal, no del dato: `{"A"}`, `{'A'}` y
`{A}` son el mismo conjunto. Y el conjunto no declara de qué tipo es: cada
elemento se compara con las reglas del `=` del lenguaje, así que `{1, 2, 3}`
compara como números y `{"A", "E"}` como texto.

## Ver también

- [XL-11](XLerror-11.md) — el `REGISTRO`, que se declara con la misma forma `nombre = ...`
- [XL-13](XLerror-13.md) — cuando son demasiados conjuntos para el lenguaje
- `docs/PAED.md` — la sección de conjuntos

---

Volver al [índice de errores](../errores.md).

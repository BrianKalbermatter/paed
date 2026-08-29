# XL-16 — El dato no corresponde al tipo declarado

## Qué dice

```
error XL-16: 'cant' es ENTERO: 'abc' no es un numero
error XL-16: 'cant' es ENTERO: no lleva decimales
error XL-16: '1' no es un valor VEC3 valido para posicion de MOVER
```

## Por qué pasa

El tipo lo decide la **declaración**, no el dato. Un campo declarado `ENTERO`
con `abc` adentro es un error de lectura, y no un cero silencioso.

Eso importa sobre todo al leer archivos: si un `ENTERO` mal cargado se
convirtiera en cero sin avisar, el programa correría entero y daría un
resultado equivocado. El error acá es mucho más barato que el resultado
equivocado allá.

## Cómo se arregla

Revisá que el dato coincida con lo declarado:

| Tipo | Acepta |
|---|---|
| `ENTERO` | `12`, `-3` — sin punto |
| `REAL` | `12`, `3.5`, `-0.2` |
| `CARACTER` | un solo carácter |
| `LOGICO` | `V` o `F` |
| `AN(n)` | texto de hasta `n` |

**Un `ENTERO` no acepta `3.5`.** No es un descuido del lenguaje: son tipos
distintos, no dos formas del mismo. Si el dato lleva decimales, el campo se
declara `REAL`.

## En el CSV

El campo **vacío** sí vale en una columna numérica, y no por descuido: en un
archivo de movimientos, el campo vacío es lo que significa "esta no se
modifica".

## Ver también

- `docs/ARCHIVOS.md` — cómo se cargan los datos con `paed datos`

---

Volver al [índice de errores](../errores.md).

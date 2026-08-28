# Estructura del repositorio

Qué hay en cada carpeta, y por qué está donde está. Este documento describe el
**repositorio**; la definición del lenguaje es [`PAED.md`](PAED.md).

## Hoy

```
paed/
├── lang/              EL INTÉRPRETE — es todo el producto
│   ├── src/           parser.c, expr.c, interpreter.c, secuencia.c,
│   │                  plataforma.c  ->  se empaquetan en libpaed.a
│   ├── include/paed/  los .h, la API pública para quien use la librería
│   ├── cli/main.c     el comando `paed`, un envoltorio de la librería
│   └── vendor/cjson/  código de terceros, no se toca
│
├── data/
│   ├── sintaxis.json  LA DEFINICIÓN DEL LENGUAJE (ver abajo)
│   └── escena.json    librería de escena 3D de VimMon — NO es PAED
│
├── docs/
│   ├── PAED.md        la spec única del lenguaje
│   ├── ESTRUCTURA.md  este archivo
│   ├── ESCENA.md      la librería de VimMon
│   └── wiki_paed.txt  documento histórico: las preguntas a la cátedra
│
├── tests/             27 programas .paed + correr.sh
├── aprender/          el tutorial: ejercicios ROTOS a propósito + solucion/
├── ejercicios/        parciales y simulacros — material de estudio, NO tests
├── Makefile
└── VERSION
```

### `data/sintaxis.json` no es un archivo de configuración

Es **el lenguaje**. El parser lo lee en runtime y el Makefile lo embebe en el
binario, así que `paed` funciona como archivo suelto. Que `ABRIR` acepte modo de
apertura y `LEER` no sale de ahí, no de una lista en C.

Esto no es una preferencia estética. Hubo un momento en el proyecto con **tres
listas de keywords desincronizadas** — `sintaxis.json`, `paed.tmLanguage.json` y
`editorText.c` — y a ninguna le entraban `FIN_REGISTRO`, `SUBACCION`, `FDA`,
`NFDA`, `Verdadero`, `Falso` ni `nil`. Ese es el bug que mató a la versión
anterior del lenguaje. Una sola fuente leída en runtime es la respuesta a ese
bug.

### Por qué `tests/` está en la raíz

Un test es **un archivo**: el programa `.paed` trae al final, en comentarios, su
bloque `ENTRADA` y su bloque `SALIDA ESPERADA`. No hay lista que mantener —
agregar un test es dejar el archivo ahí.

No existe modo de regrabar la salida automáticamente, a propósito. Si un test
falla, el runner muestra el diff y el bloque se corrige a mano leyéndolo.
Regrabar sin leer es exactamente cómo un test deja de proteger: "arregla" el
test en vez del bug.

### Por qué `aprender/` no está en `tests/` ni en `ejercicios/`

Son tres cosas distintas y conviene no confundirlas:

| Carpeta | Qué es | Estado esperado |
|---|---|---|
| `tests/` | la batería que protege al lenguaje | **pasan todos** |
| `aprender/` | el tutorial de `paed aprender` | **fallan todos** (vienen rotos) |
| `ejercicios/` | parciales y simulacros, material de estudio | no se corren |

`aprender/` usa **el mismo formato que `tests/`** — el `.paed` trae al final su
bloque `ENTRADA` y su `SALIDA ESPERADA` — porque un ejercicio *es* un test, con
la única diferencia de que viene roto a propósito. Un segundo formato para lo
mismo envejecería sin que nadie se entere.

Lo que sí tiene de más es `// ── PISTA ──`, y `aprender/solucion/` con la
versión resuelta de cada uno. Las soluciones **no viajan en el binario**: son
para que `make test-aprender` pueda demostrar dos cosas que importan — que
ningún ejercicio pasa sin tocarlo, y que todos tienen solución que pasa.

Los ejercicios sí viajan en el binario, embebidos por `aprender/generar.sh`
igual que `sintaxis.json`. Por eso quien baja `paed` suelto de un release tiene
el tutorial completo sin clonar nada, y por eso `paed aprender reset` puede
existir: el original está adentro del ejecutable.

### Por qué `ejercicios/` no está en `tests/`

Son parciales y simulacros escritos como estudiante, contra el intérprete viejo.
Tienen sintaxis que la cátedra no avala (ver `PAED.md §13.2`). Mezclarlos con la
batería haría que el runner los ejecutara y fallara — o peor, que alguien
"arreglara" el parser para aceptarlos.

## Qué cambió el 2026-08-14

### Antes

```
paed/
├── Frankly/                   <- el intérprete en BASH, ya reemplazado
│   ├── paed                     715 líneas de bash
│   ├── core/*.sh                flags, validacion, analisisSemantico, palabras
│   ├── stdlib/matematica.sh     TRUNC, ABSO, REDOND en bash
│   ├── tools/                   generar.sh + gen_sintaxis.c
│   ├── generated/               paed_keywords.h
│   ├── syntaxes/ syntax/ ftdetect/   tres resaltadores
│   ├── DOC.txt
│   ├── data/sintaxis.json     <- VIVO, enterrado acá adentro
│   ├── docs/PAED.md           <- VIVO, enterrado acá adentro
│   ├── tests/                 <- VIVO, enterrado acá adentro
│   └── AprendiendoPseudo/, ejercicio*.paed
└── lang/
```

### El problema

`Frankly` era el intérprete en bash. Ya estaba muerto: el Makefile no lo miraba,
`lang/` no lo incluía, y ningún `.c` tocaba un solo `.sh` de `core/` o
`stdlib/`. Código reemplazado, no código a portar.

Pero adentro de esa carpeta habían quedado enterradas **las dos cosas más vivas
del repositorio**: `sintaxis.json`, que se embebe en el binario y se instala en
el sistema, y `PAED.md`, que es la spec única.

Nunca fueron de Frankly. Son de PAED, y estaban guardadas en la carpeta del
intérprete retirado.

### El movimiento

| Antes | Ahora | Motivo |
|---|---|---|
| `Frankly/data/` | `data/` | Es el lenguaje, no la implementación vieja |
| `Frankly/docs/` | `docs/` | Es la spec, no notas de Frankly |
| `Frankly/tests/` | `tests/` | Prueban el intérprete en C, no el de bash |
| `Frankly/AprendiendoPseudo/`, `Frankly/ejercicio*.paed`, `Frankly/paed.paed` | `ejercicios/` | Material de estudio, no batería |

La carpeta `Frankly/` ya no existe.

### Rutas que hubo que corregir

| Archivo | Qué apuntaba mal |
|---|---|
| `Makefile` | la fuente de `sintaxis.json`, el `install`, el target `test` |
| `tests/correr.sh` | la raíz del repo (`../..` -> `..`) y `tests_dir` |
| `lang/src/parser.c` | los candidatos de búsqueda del directorio de datos |
| `lang/include/paed/parser.h` | la documentación de esos candidatos |
| `.github/workflows/release.yml` | el `cp` de `sintaxis.json` al paquete |
| `docs/PAED.md` | §0.1 y el ejemplo de `paedrun` |
| **15 archivos de `tests/`** | ver abajo |

Los 15 tests no fallaron por casualidad: su bloque `SALIDA ESPERADA` contiene la
ruta del propio archivo, porque los errores del parser la imprimen
(`tests/arreglo_limites.paed:25: error: ...`). Mover la carpeta cambió esa ruta.
Se corrigió la ruta y **nada más**.

> Esto deja algo a la vista: los tests dependen de su propia ubicación en el
> disco. Hoy no molesta, pero es la razón por la que mover una carpeta rompió 15
> pruebas que no tenían nada que ver con lo que se movió.

### Lo retirado — borrado el 2026-08-27

Estaba en `_void/`: el intérprete original en bash, sus `core/` y `stdlib/`, las
herramientas que generaban keywords en C, y tres resaltadores incompletos. Nada
de eso entraba en el build ni valía como fuente de sintaxis.

Se borró porque ya no hacía falta ni para consultarlo. **El historial de git lo
tiene igual.** Las dos lecciones que dejó — por las que el lenguaje está armado
como está — se rescataron en [`LECCIONES.md`](LECCIONES.md).

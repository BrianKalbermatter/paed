# \_void — lo que ya no se usa

Nada de acá entra en el build. El Makefile no lo mira, `lang/` no lo incluye y
`make install` no lo copia. Está guardado y no borrado por una sola razón: fue
la primera implementación de PAED y todavía se puede consultar. El día que no
haga falta ni para eso, se borra — el historial de git lo tiene igual.

**Nada de acá es autoridad de sintaxis.** La definición del lenguaje es
`data/sintaxis.json` y `docs/PAED.md`, y punto.

## Qué hay

| Qué | Qué era | Por qué está muerto |
|---|---|---|
| `paed-interprete-bash` | El intérprete original, 715 líneas de bash | Lo reemplazó `lang/*.c`. Además era permisivo de más: aceptaba `PARA ... a ...` y comillas dobles, que no son de cátedra |
| `core/` | `flags.sh`, `validacion.sh`, `analisisSemantico.sh`, `palabras.sh` | Solo los usaba el intérprete bash, vía `source` |
| `stdlib/` | `matematica.sh` — TRUNC, ABSO, REDOND en bash | Ídem. Esas funciones tienen 0 apariciones en el material de cátedra |
| `tools/` | `generar.sh` + `gen_sintaxis.c` | Generaba las tres cosas de abajo, y las tres están muertas |
| `generated/paed_keywords.h` | Keywords en C, generadas | **Nadie en `lang/` lo incluye.** El parser lee `sintaxis.json` en runtime |
| `syntaxes/paed.tmLanguage.json` | Resaltador TextMate (VS Code, Sublime) | Incompleto y con comillas dobles |
| `syntax/paed.lua`, `ftdetect/paed.vim` | Resaltador de Neovim | Incompleto, y otra lista de keywords más para desincronizar |
| `helix/` | Resaltador tree-sitter: `grammar.js` + `queries/*.scm` | **Era un SEGUNDO parser de PAED.** El de verdad es `lang/src/parser.c`, y adivinaba por forma lo que aquel sabe con certeza — ver abajo |
| `DOC.txt` | Notas del intérprete bash | Él mismo dice que `docs/PAED.md` le gana |

## La lección que dejó

Hubo un momento con **tres listas de keywords desincronizadas**:
`sintaxis.json`, `paed.tmLanguage.json` y `editorText.c`. A ninguna le entraban
`FIN_REGISTRO`, `SUBACCION`, `FDA`, `NFDA`, `Verdadero`, `Falso` ni `nil`.

Ese es el bug que mató a la versión anterior del lenguaje, y es exactamente por
eso que hoy `sintaxis.json` es una sola fuente que se lee en runtime en vez de
copiarse a mano a cuatro lados.

## Por qué también cayó `helix/` (tree-sitter)

`helix/` no repetía las keywords — esas las leía de `sintaxis.json`, bien. Repetía
la **gramática**: era un segundo parser de PAED al lado de `lang/src/parser.c`.

Y un parser de resaltado, que mira una ventana chica de texto, no puede saber lo
que sabe el parser de verdad, que leyó el AMBIENTE entero:

| En `grammar.js` había que | El parser en C lo sabe porque |
|---|---|
| adivinar que `V` es booleano y no una variable llamada `v`, por la posición | `V` no está en `decls[]`, y si estuviera es una variable |
| sacar `N` y `AN` de los tipos, porque `N` también es nombre de variable | `N: ENTERO` está declarado; no hay nada que adivinar |
| no poder distinguir `fecha` (un REGISTRO) de cualquier otro nombre | `registros[]` tiene los REGISTRO declarados |
| no poder distinguir un ARCHIVO de una variable común | `PAEDDecl.es_archivo` |

El resaltado de hoy sale de `lang/src/colores.c`, que **usa el parser de verdad**.
Una sola gramática, una sola fuente.

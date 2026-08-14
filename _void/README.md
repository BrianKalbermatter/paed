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
| `syntax/paed.lua`, `ftdetect/paed.vim` | Resaltador de Neovim | Hoy el resaltado es tree-sitter, en `helix/` |
| `DOC.txt` | Notas del intérprete bash | Él mismo dice que `docs/PAED.md` le gana |

## La lección que dejó

Hubo un momento con **tres listas de keywords desincronizadas**:
`sintaxis.json`, `paed.tmLanguage.json` y `editorText.c`. A ninguna le entraban
`FIN_REGISTRO`, `SUBACCION`, `FDA`, `NFDA`, `Verdadero`, `Falso` ni `nil`.

Ese es el bug que mató a la versión anterior del lenguaje, y es exactamente por
eso que hoy `sintaxis.json` es una sola fuente que se lee en runtime en vez de
copiarse a mano a cuatro lados.

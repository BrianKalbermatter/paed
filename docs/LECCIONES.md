# Lecciones — por qué PAED está armado así

Rescatado de `_void/README.md` antes de borrarlo (2026-08-27). No es historia
por nostalgia: son las dos razones por las que hoy el lenguaje tiene la forma
que tiene, y las dos se pagaron con una versión muerta.

## 1. Tres listas de keywords desincronizadas

Hubo un momento con la misma lista de palabras clave copiada en **tres** lados:
`sintaxis.json`, `paed.tmLanguage.json` y `editorText.c`. A ninguna le entraban
`FIN_REGISTRO`, `SUBACCION`, `FDA`, `NFDA`, `Verdadero`, `Falso` ni `nil`.

Ese es el bug que mató a la versión anterior del lenguaje.

**Por eso hoy `data/sintaxis.json` es una sola fuente que se lee en runtime**, en
vez de copiarse a mano a cuatro lados. La regla que sale de acá:

> Ninguna palabra clave se escribe a mano en ningún parser ni en ningún
> resaltador. Va a `sintaxis.json` y todos la toman de ahí.

## 2. Un resaltador no puede saber lo que sabe el parser

`_void/helix/` (tree-sitter) no repetía las keywords — esas las leía de
`sintaxis.json`, bien. Repetía la **gramática**: era un segundo parser de PAED
al lado de `lang/src/parser.c`.

Y un parser de resaltado, que mira una ventana chica de texto, no puede saber lo
que sabe el parser de verdad, que leyó el `AMBIENTE` entero:

| En `grammar.js` había que | El parser en C lo sabe porque |
|---|---|
| adivinar que `V` es booleano y no una variable llamada `v`, por la posición | `V` no está en `decls[]`, y si estuviera es una variable |
| sacar `N` y `AN` de los tipos, porque `N` también es nombre de variable | `N: ENTERO` está declarado; no hay nada que adivinar |
| no poder distinguir `fecha` (un `REGISTRO`) de cualquier otro nombre | `registros[]` tiene los `REGISTRO` declarados |
| no poder distinguir un `ARCHIVO` de una variable común | `PAEDDecl.es_archivo` |

**El resaltado de la terminal sale de `lang/src/colores.c`, que usa el parser de
verdad.** Una sola gramática, una sola fuente.

### La parte que quedó sin resolver

Lo de arriba vale para la terminal. **Helix es otra cosa**: no puede llamar a
`colores.c`, porque un editor externo necesita tree-sitter o un language server.
Por eso la gramática siguió instalada (`~/.config/helix/runtime/grammars/paed.so`)
aunque el README la diera por muerta.

O sea que hay dos caminos honestos y hay que elegir uno:

1. **Tree-sitter para Helix**, aceptando que es léxico y adivina — con la
   gramática deliberadamente tonta, para que el único solapamiento con
   `parser.c` sea la lista de tokens que ya tiene dueño.
2. **Un language server** que use `parser.c` de verdad. Una sola gramática, y
   Helix pinta con lo que el parser sabe con certeza.

El 1 ya está hecho y anda. El 2 es la solución de los lenguajes grandes (Go,
Rust, C#) y es bastante más trabajo.

## Qué había en `_void/`

El intérprete original en bash (715 líneas), sus `core/` y `stdlib/`, las
herramientas que generaban keywords en C, y tres resaltadores incompletos
(TextMate, Neovim, tree-sitter). Todo eso lo reemplazó `lang/*.c`.

Está en el historial de git si alguna vez hace falta mirarlo.

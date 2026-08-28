#ifndef VIMMON_COLORES_H
#define VIMMON_COLORES_H

#include <paed/parser.h>

// Resaltado de PAED, hecho con EL parser de PAED.
//
// Hubo una version anterior con tree-sitter (ver docs/LECCIONES.md). Era una
// segunda gramatica del lenguaje al lado de parser.c, y como toda segunda
// fuente de verdad, mentia: tenia que ADIVINAR por la forma del texto cosas que
// el parser sabe con certeza porque leyo el AMBIENTE entero.
//
//     fechas: fecha;          ¿'fecha' es un tipo o una variable?
//     archivoFacturas: ...    ¿esto es un archivo o un entero mas?
//     N := N + 1;             ¿'N' es el tipo N(4) o una variable llamada N?
//
// Un resaltador que mira una ventana de texto no puede contestar ninguna. El
// parser contesta las tres mirando PAEDProgram: registros[], decls[].es_archivo
// y decls[].name. Por eso el resaltado se hace DESPUES de parsear, y no en vez
// de parsear.
//
// Que color le toca a cada rol NO se decide aca: sale de data/sintaxis.json,
// que ya trae 'color' y 'scope' en cada categoria.

// Un pedazo de texto del archivo, ya clasificado.
//
// `linea` y `col` arrancan en 1 (como los cuenta un editor y como los reporta
// el parser en sus errores), y se miden en BYTES, no en caracteres: es lo que
// necesita quien tiene que pintar el buffer.
typedef struct {
    int         linea;
    int         col;
    int         largo;
    // El nombre de la categoria de sintaxis.json: "bucles", "tipos",
    // "tipos_usuario", "archivos", "variables"... Es un puntero a memoria de
    // sintaxis.json y vive mientras no se llame paed_syntax_free().
    const char *rol;
    // El texto del token, terminado en '\0'. Solo vale durante la llamada:
    // apunta al buffer de la linea, que se reusa en la siguiente.
    const char *texto;
} PAEDToken;

// Se llama una vez por token, en el orden en que aparecen en el archivo.
typedef void (*PAEDTokenFn)(const PAEDToken *tok, void *ud);

// Recorre el archivo entero y llama a `fn` por cada token.
//
// `prog` es lo que devolvio paed_parse_file. Puede venir de un parseo que FALLO:
// el parser junta errores y sigue leyendo, asi que el AMBIENTE ya esta cargado y
// los colores salen igual. Un editor tiene que colorear el archivo roto —
// justamente ahi es cuando mas se lo necesita.
//
// Si `prog` es NULL colorea igual, pero solo con lo que dice sintaxis.json: sin
// tipos de usuario y sin archivos.
//
// Devuelve 0, o -1 si no pudo abrir el archivo.
int paed_colorear_archivo(const char *path, const PAEDProgram *prog,
                          PAEDTokenFn fn, void *ud);

// La secuencia ANSI para un rol, lista para escribir en la terminal
// (ej: "\033[38;5;208m"). Devuelve "" si ese rol no lleva color.
//
// Traduce el NOMBRE de color de sintaxis.json ('naranja') a lo que entiende una
// terminal. editorBim no usa esta: usa el nombre, y lo dibuja como quiera.
const char *paed_rol_ansi(const char *rol);

// Apaga el color. Es "\033[0m".
const char *paed_ansi_reset(void);

#endif // VIMMON_COLORES_H

#ifndef PAED_SENTENCIAS_INTERNO_H
#define PAED_SENTENCIAS_INTERNO_H

// Parsear las sentencias de una linea. Vive en src/: no es API de PAED.
//
// Esto y bloques.c son MUTUAMENTE RECURSIVOS, y es correcto que lo sean: un
// bloque contiene sentencias, y una sentencia puede abrir un bloque. La
// recursion es del lenguaje, no un accidente del codigo — por eso se declara
// en un header en vez de forzar a los dos a vivir en el mismo archivo.

#include "paed/parser.h"

// Que hacer con cada pedazo. `ctx` es lo que necesite el que llama y le vuelve
// tal cual: el AMBIENTE le pasa el REGISTRO que esta abierto, el PROCESO nada.
typedef void (*ParteFn)(PAEDProgram *p, char *texto, int lineno, void *ctx);

// Parte la linea por ';' y llama a `fn` con cada pedazo. El corte solo vale
// FUERA de comillas y de parentesis: en ESCRIBIR('hola; chau') ese ';' es parte
// del texto.
void por_cada_sentencia(PAEDProgram *p, char *linea, int lineno,
                        ParteFn fn, void *ctx);

// Parsea todas las sentencias de una linea. La catedra permite varias
// separadas por ';' en el mismo renglon: `a := 1; b := 2;`.
void parse_sentencias(PAEDProgram *p, char *linea, int lineno);

#endif // PAED_SENTENCIAS_INTERNO_H

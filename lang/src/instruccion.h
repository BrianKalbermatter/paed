#ifndef PAED_INSTRUCCION_INTERNO_H
#define PAED_INSTRUCCION_INTERNO_H

// Parsear UNA instruccion del PROCESO: `ESCRIBIR('hola')`, `a := 1`,
// `ABRIR E/(arch)`. Vive en src/: no es API de PAED.

#include "paed/parser.h"

// Parsea una instruccion ya suelta —sin ';' y sin comentario— y la agrega al
// programa. La linea llega despues de que sentencias.c la partiera y de que
// bloques.c descartara las cabeceras de bloque.
void parse_instruction(PAEDProgram *p, char *linea, int lineno);

#endif // PAED_INSTRUCCION_INTERNO_H

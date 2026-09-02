#ifndef PAED_SENTENCIAS_INTERNO_H
#define PAED_SENTENCIAS_INTERNO_H

// Parsear las sentencias de una linea. Vive en src/: no es API de PAED.
//
// Esto y bloques.c son MUTUAMENTE RECURSIVOS, y es correcto que lo sean: un
// bloque contiene sentencias, y una sentencia puede abrir un bloque. La
// recursion es del lenguaje, no un accidente del codigo — por eso se declara
// en un header en vez de forzar a los dos a vivir en el mismo archivo.

#include "paed/parser.h"

// Parsea todas las sentencias de una linea. La catedra permite varias
// separadas por ';' en el mismo renglon: `a := 1; b := 2;`.
void parse_sentencias(PAEDProgram *p, char *linea, int lineno);

#endif // PAED_SENTENCIAS_INTERNO_H

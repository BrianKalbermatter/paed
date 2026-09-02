#ifndef PAED_AMBIENTE_INTERNO_H
#define PAED_AMBIENTE_INTERNO_H

// El bloque AMBIENTE: que existe en un programa y de que tipo. Vive en src/:
// no es API de PAED.
//
// El AMBIENTE decide QUE existe; el PROCESO decide que PASA. Son dos temas y se
// parsean en dos lados.

#include "paed/parser.h"

// Parsea una linea del AMBIENTE. `reg` lleva el REGISTRO que se este armando,
// o NULL si la linea no esta adentro de uno: un REGISTRO abre un sub-bloque y
// sus campos se declaran adentro.
void parse_ambiente(PAEDProgram *p, char *linea, int lineno, PAEDRegistro **reg);

#endif // PAED_AMBIENTE_INTERNO_H

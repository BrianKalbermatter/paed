#ifndef PAED_SUBACCIONES_INTERNO_H
#define PAED_SUBACCIONES_INTERNO_H

// FUNCION y PROCEDIMIENTO. Vive en src/: paed_subaccion si es publica y esta
// declarada en paed/parser.h, el resto es nuestro.

#include "paed/parser.h"

// Si la linea abre una subaccion, devuelve la palabra clave que la abre
// (FUNCION o PROCEDIMIENTO). NULL si no la abre.
const char *abre_subaccion(const char *linea);

// Parsea un parametro de la cabecera y lo agrega a la subaccion.
void parse_param(PAEDProgram *p, PAEDSubaccion *sub, char *texto, int lineno);

// Parsea la cabecera entera —nombre, parametros y tipo de retorno— y anota la
// subaccion en el programa. Devuelve la que anoto, o NULL si no pudo.
PAEDSubaccion *parse_subaccion_cabecera(PAEDProgram *p, const char *kw,
                                        char *linea, int lineno);

#endif // PAED_SUBACCIONES_INTERNO_H

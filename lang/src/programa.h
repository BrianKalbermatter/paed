#ifndef PAED_PROGRAMA_INTERNO_H
#define PAED_PROGRAMA_INTERNO_H

// El PAEDProgram que se esta construyendo. Vive en src/: no es API de PAED,
// es lo que los modulos del parser se prestan entre ellos.

#include "paed/parser.h"

// Reserva la proxima instruccion ya inicializada. Devuelve NULL —y anota el
// error— si el programa llego a PAED_MAX_INSTRS.
//
// salto = -1 significa "esta no salta a ningun lado"; las de bloque lo
// completan cuando se cierra el bloque.
PAEDInstr *nueva_instr(PAEDProgram *p, PAEDKind kind, int lineno);

// Si la linea cierra la ACCION. Se aceptan FIN_ACCION y FINACCION.
int es_fin_accion(const char *linea);

#endif // PAED_PROGRAMA_INTERNO_H

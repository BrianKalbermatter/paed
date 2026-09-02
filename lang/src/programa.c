// El programa que se esta construyendo: reservar instrucciones.
//
// Arranca con una sola funcion a proposito. nueva_instr es el asignador del
// arreglo de instrucciones, y la necesitan tanto el parseo de instrucciones
// como la pila de bloques: mientras viviera adentro de parser.c, el modulo de
// bloques no podia salir sin arrastrarla.
//
// Aca van a mudarse despues el analisis del archivo completo y las
// subacciones, que son las otras partes que trabajan sobre el PAEDProgram
// entero y no sobre una linea suelta.

#include "programa.h"
#include "reporte.h"

#include <stddef.h>
#include <string.h>


// Reserva la proxima instruccion ya inicializada. salto = -1 significa "esta no
// salta a ningun lado"; las de bloque lo completan cuando se cierra el bloque.
PAEDInstr *nueva_instr(PAEDProgram *p, PAEDKind kind, int lineno) {
    if (p->instr_count >= PAED_MAX_INSTRS) {
        add_error(p, lineno, "demasiadas instrucciones (maximo %d)", PAED_MAX_INSTRS);
        return NULL;
    }
    PAEDInstr *in = &p->instrs[p->instr_count++];
    memset(in, 0, sizeof(*in));
    in->kind  = kind;
    in->line  = lineno;
    in->salto = -1;
    in->siguiente = -1;
    return in;
}

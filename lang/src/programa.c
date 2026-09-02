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
#include "texto.h"

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

// Cierre de la ACCION. Se aceptan DOS formas, decididas el 2026-08-10:
//
//   FIN_ACCION   la de la wiki, y la que usan todos los .paed del repo
//   FINACCION    la misma sin el guion bajo
//
// Las dos son una sola palabra, asi que cuestan un solo strcmp y ningun
// lookahead. La forma de la catedra (`FIN ACCION`, con ESPACIO, en
// AED_2021_UnI.pdf pagina 10) queda AFUERA por decision del autor: partida en
// dos palabras obligaria a mirar la siguiente antes de decidir.
int es_fin_accion(const char *linea) {
    return kw_es(linea, "FIN_ACCION") ||
           kw_es(linea, "FINACCION");
}

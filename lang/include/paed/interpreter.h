#ifndef VIMMON_INTERPRETER_H
#define VIMMON_INTERPRETER_H

#include <stddef.h>   // size_t

#include "parser.h"

// ── Procedimientos que pone el HOST ──────────────────────────────────────────
//
// El interprete trae solo lo que define el pseudocodigo AED: LEER, ESCRIBIR y
// el control de flujo. Todo lo demas lo agrega quien lo hospeda — VimMon
// registra su escena 3D (CUBO, MOVER, GIRAR...), y otro programa registraria
// lo suyo. Es la misma idea del bus de plugins: el nucleo no conoce a sus
// extensiones, las extensiones se anotan.
//
// Devuelve 0 si el procedimiento salio bien, -1 si fallo (y en ese caso avisa
// el motivo con paed_runtime_error).
typedef int (*PaedProc)(const PAEDProgram *prog, const PAEDInstr *in, void *ud);

// Anota un procedimiento con su nombre. `ud` es el estado del host (por
// ejemplo la escena) y le vuelve tal cual en cada llamada, asi el interprete no
// necesita saber que es. Registrar dos veces el mismo nombre REEMPLAZA: sirve
// para reapuntar a otro estado sin acumular entradas viejas.
//
// Devuelve 0 si quedo anotado, -1 si no hay lugar.
int  paed_register_proc(const char *nombre, PaedProc fn, void *ud);

// Borra todos los procedimientos del host. El lenguaje queda pelado.
void paed_clear_procs(void);

// Reporta un error de ejecucion con archivo:linea, igual que los del propio
// interprete. Es publica para que los procedimientos del host no inventen su
// propio formato de mensaje.
void paed_runtime_error(const PAEDProgram *prog, const PAEDInstr *in, const char *msg);

// ── De donde salen los datos de LEER ─────────────────────────────────────────
//
// El interprete NO abre stdin por su cuenta, y no es un capricho: corre DENTRO
// del game loop del renderer, asi que un fgets bloqueante congelaria la ventana
// entera esperando que alguien tipee en una terminal que quiza ni esta a la
// vista. El que hospeda al interprete es el que sabe de donde vienen los datos:
// paedrun engancha stdin, la ventana SDL todavia no engancha nada.
//
// Deja UNA linea en `buf`, sin el '\n'. Devuelve 0 si trajo un dato, -1 si la
// entrada se termino.
typedef int (*PaedEntrada)(char *buf, size_t n, void *ud);

// Engancha la fuente de datos. Sin fuente, LEER de consola falla con un mensaje
// claro en vez de colgarse esperando algo que nunca va a llegar.
void interp_set_entrada(PaedEntrada fn, void *ud);

// Ejecuta el programa. Devuelve 0 si se ejecuto entero.
// Los errores en runtime se reportan con archivo:linea, igual que el parser.
int  interp_exec (const PAEDProgram *prog);

#endif // VIMMON_INTERPRETER_H

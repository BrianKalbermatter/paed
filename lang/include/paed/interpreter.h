#ifndef VIMMON_INTERPRETER_H
#define VIMMON_INTERPRETER_H

#include <stddef.h>   // size_t

#include "parser.h"
#include "expr.h"   // Valor: lo que devuelve una funcion del host

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

// ── Funciones del host ───────────────────────────────────────────────────────
//
// Un PROCEDIMIENTO hace algo; una FUNCION devuelve un valor y por eso se puede
// usar DENTRO de una expresion: `SI TECLA('W') ENTONCES`, `t := TICKS();`.
// Esa diferencia es de la catedra, no un invento de PAED (01-nucleo.md §7).
//
// Un juego necesita las dos: dibujar es un procedimiento, pero PREGUNTAR si una
// tecla esta apretada es una funcion. Sin esto el game loop no se puede escribir
// en pseudocodigo, porque no habria forma de consultarle nada al mundo de afuera.
//
// Recibe el TEXTO de cada argumento, no su valor, por la misma razon que las
// funciones del propio programa (ver PaedFnLlamar en expr.h). El que quiera el
// valor lo evalua con expr_eval.
//
// Devuelve 0 si salio bien, -1 si no, y en ese caso deja el motivo en `error`.
typedef int (*PaedFunc)(const char *const *args, int n, Valor *out,
                        char *error, size_t error_n, void *ud);

// Anota una funcion con su nombre. Mismas reglas que paed_register_proc:
// registrar dos veces REEMPLAZA. Devuelve 0 si quedo anotada, -1 si no hay lugar.
//
// A diferencia de los procedimientos, una funcion NO necesita declararse en un
// .json: el parser no la ve. Las expresiones las resuelve expr.c en tiempo de
// ejecucion, cuando pregunta si el nombre existe.
int  paed_register_func(const char *nombre, PaedFunc fn, void *ud);

// Evalua el TEXTO de un argumento con las variables del programa en curso.
//
// Los procedimientos y funciones del host reciben sus argumentos SIN evaluar
// (ver PaedFnLlamar en expr.h), asi que `CUBO(x = px + 1)` le llega la cadena
// "px + 1", no un numero. Para resolverla hace falta el Entorno, que es privado
// del interprete y tiene que seguir siendolo: si el host pudiera tocarlo,
// cualquier extension podria pisar las variables del programador.
//
// Devuelve 0 si evaluo, -1 si no, y en ese caso deja el motivo en `error`.
int  paed_eval(const char *texto, Valor *out, char *error, size_t error_n);

// Borra todos los procedimientos Y funciones del host. El lenguaje queda pelado.
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

// ── De donde salen los datos de una SECUENCIA ────────────────────────────────
//
// Misma decision que con LEER, y por la misma razon: el interprete no sabe ni
// tiene por que saber donde viven los datos. Lo que cambia es la forma — una
// secuencia no se pide de a un dato por vez, se pide ENTERA y una sola vez, al
// arrancar el programa. Es lo que corresponde: la secuencia es un dato fijo del
// enunciado, no algo que alguien tipea mientras el programa corre.
//
// Deja el contenido de la secuencia `nombre` en `buf`. Devuelve 0 si la
// encontro, -1 si no hay datos para esa secuencia.
typedef int (*PaedSecuenciaDatos)(const char *nombre, char *buf, size_t n, void *ud);

// Engancha la fuente. Sin fuente, una secuencia de entrada queda vacia: el
// primer AVZ la da por terminada y el programa avisa, en vez de inventar datos.
void interp_set_secuencia(PaedSecuenciaDatos fn, void *ud);

// Ejecuta el programa. Devuelve 0 si se ejecuto entero.
// Los errores en runtime se reportan con archivo:linea, igual que el parser.
int  interp_exec (const PAEDProgram *prog);

#endif // VIMMON_INTERPRETER_H

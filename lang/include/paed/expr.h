#ifndef VIMMON_EXPR_H
#define VIMMON_EXPR_H

#include <stddef.h>   // size_t
#include "parser.h"

// Evaluador de expresiones de PAED.
//
// El parser guarda las condiciones y las asignaciones CRUDAS, como texto. Este
// modulo es el que las convierte en un valor. Sin el, el parser sabe que
// `MIENTRAS (cont < 4) HACER` es un bucle y a donde saltar, pero no sabe si
// hay que entrar.
//
// Es un descenso recursivo: una funcion por nivel de prioridad, y cada una
// llama a la de mayor prioridad que ella. La tabla sale de
// TEORIA_COMPLETA.txt:361-371 (de MENOR a mayor prioridad aca abajo):
//
//     O                 <- la funcion mas externa
//     Y
//     =  <>
//     <  <=  >  >=
//     +  -               (suma y resta)
//     *  /  DIV  MOD
//     **                 (potencia, asociativa a derecha)
//     + - NO             (unarios)
//     literales, variables, funciones, ( )
//
// Que la prioridad sea el ORDEN DE LAS LLAMADAS y no una tabla de numeros es
// lo que hace que "2 + 3 * 4" de 14 y no 20: cuando suma() pide sus operandos,
// producto() ya se comio el "3 * 4".

#define PAED_MAX_VARS 64

// Cuantos elementos de arreglo entran EN TOTAL, sumando todos los arreglos del
// programa. Es un pool compartido y no un array por variable: reservar el
// maximo en cada una de las 64 variables gastaria memoria en los 63 escalares.
#define PAED_MAX_ELEMS 512

typedef enum {
    VAL_NUM,      // entero o real: en PAED no se distinguen al evaluar
    VAL_TEXTO,
    VAL_LOGICO,
} TipoValor;

typedef struct {
    TipoValor tipo;
    double    num;                 // VAL_NUM
    int       logico;              // VAL_LOGICO: 0 o 1
    char      texto[PAED_VAL_MAX]; // VAL_TEXTO
} Valor;

typedef struct {
    char  nombre[PAED_NAME_MAX];
    Valor valor;        // el valor, cuando es un escalar

    // Arreglo declarado con ARREGLO[desde..hasta]. En AED los limites los pone
    // el programador y NO arrancan en 0: A[1..10] va del 1 al 10, no del 0 al 9.
    int   es_arreglo;
    int   desde, hasta; // ambos inclusive
    int   off;          // donde empiezan sus elementos dentro de Entorno.pool
} Variable;

// Tabla de variables. Sin malloc, como todo el resto del proyecto.
typedef struct {
    Variable items[PAED_MAX_VARS];
    int      count;
    Valor    pool[PAED_MAX_ELEMS]; // elementos de todos los arreglos, seguidos
    int      pool_usado;
    char     error[PAED_MSG_MAX];  // vacio si la ultima operacion salio bien
} Entorno;

void   env_init  (Entorno *e);
Valor *env_buscar(Entorno *e, const char *nombre);   // NULL si no existe
int    env_set   (Entorno *e, const char *nombre, Valor v);  // 0 OK, -1 lleno

// Reserva los elementos de un arreglo y los deja en 0.
// Devuelve 0 si salio bien, -1 si no: el motivo queda en e->error.
int env_declarar_arreglo(Entorno *e, const char *nombre, int desde, int hasta);

// ¿Existe ya una variable con ese nombre? No toca e->error ni la crea.
// Sirve para distinguir "campo valido de un registro" de "campo inventado":
// los campos se crean todos al arrancar, asi que uno que no esta, no existe.
int env_existe(Entorno *e, const char *nombre);

// Devuelve el elemento `indice` de un arreglo, listo para leer o escribir.
// Devuelve NULL si la variable no existe, no es un arreglo, o el indice se fue
// de los limites declarados: el motivo queda en e->error.
Valor *env_elem(Entorno *e, const char *nombre, int indice);

// Evalua `texto` y deja el resultado en `out`.
// Devuelve 0 si salio bien, -1 si no: el motivo queda en env->error.
int expr_eval(const char *texto, Entorno *env, Valor *out);

// Lee un valor como condicion. Un NUM cuenta como falso solo si es 0, y un
// texto vacio cuenta como falso: asi una condicion nunca queda "indefinida".
int valor_verdadero(const Valor *v);

// Escribe el valor en `out` con formato legible (para ESCRIBIR).
void valor_a_texto(const Valor *v, char *out, size_t out_size);

#endif // VIMMON_EXPR_H

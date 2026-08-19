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

    // HV — alto valor. Es MAYOR QUE CUALQUIER COSA, y por eso es un tipo y no
    // un numero grande.
    //
    // Un 999999999 no sirve: las claves de los parciales son TEXTO
    // ("F1-Ibuprofeno"), y comparar() pasa los dos lados a texto cuando uno lo
    // es. Ahi strcmp("999999999", "F1-Ibuprofeno") da que HV es MENOR, porque
    // '9' viene antes que 'F' en ASCII — justo al reves de lo que HV significa.
    //
    // Como tipo propio la comparacion es una regla y no una casualidad del
    // ASCII: gana siempre, contra numeros y contra textos.
    VAL_ALTO,

    // DECLARADA PERO SIN VALOR TODAVIA.
    //
    // Va al final del enum a proposito: asi un Valor puesto en cero sigue
    // siendo VAL_NUM, que es lo que supone todo el codigo que hace `Valor v =
    // {0}`. Para que una variable nazca vacia hay que pedirlo explicitamente.
    //
    // Existe por las SUBACCIONES. Antes una variable declarada simplemente no
    // estaba en la tabla, y "no esta" significaba "no tiene valor". Con
    // subacciones eso se rompe: si una subaccion le asigna a un global que
    // todavia no tenia valor, env_set no lo encuentra, lo crea ARRIBA — o sea
    // adentro del marco — y al volver el marco se lo lleva puesto. El
    // 'Inicializar' del template de corte de control hace exactamente eso.
    //
    // Con este tipo las declaradas SI estan en la tabla desde el arranque:
    // env_set las encuentra y les escribe en su lugar. Y de paso una local
    // tapa a una global desde que se declara, no recien cuando se le asigna.
    VAL_VACIO,
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

// ── Marcos de variables locales ─────────────────────────────────────────────
//
// Una subaccion necesita que sus parametros y sus locales TAPEN a las globales
// que se llamen igual, y que desaparezcan al volver. La tabla de variables es
// una pila, asi que un marco es simplemente un par de marcas:
//
//     int base = e->count, pool = e->pool_usado;   // abrir el marco
//     env_push(e, "a", valor);                     // parametros y locales
//     ... correr la subaccion ...
//     env_truncar(e, base, pool);                  // cerrar el marco
//
// Para que tapar funcione, env_buscar recorre la tabla DESDE EL FINAL: la
// ultima entrada con ese nombre es la mas interna.

// Crea SIEMPRE una entrada nueva, aunque ya exista una con ese nombre. Es lo
// que distingue declarar de asignar: env_set le escribe a la variable que ya
// esta, env_push declara una nueva que la tapa.
// Devuelve 0, o -1 si la tabla esta llena.
int env_push(Entorno *e, const char *nombre, Valor v);

// Tira todo lo que se declaro despues de esas marcas.
void env_truncar(Entorno *e, int count, int pool_usado);

// Igual que env_buscar pero mirando SOLO de `desde` en adelante, o sea solo
// adentro del marco. Sirve para preguntar "¿esta subaccion le asigno algo a su
// propio nombre?" sin que conteste que si una variable global que se llama
// igual. Devuelve NULL si no hay ninguna en ese tramo.
Valor *env_buscar_marco(Entorno *e, const char *nombre, int desde);

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

// ── Funciones del propio programa dentro de una expresion ───────────────────
//
// `x := sumar(3, 5)` obliga al evaluador de expresiones a ejecutar codigo, y
// ejecutar codigo es trabajo del interprete. En vez de que expr.c aprenda a
// correr instrucciones — que lo volveria un segundo interprete — el interprete
// le deja estos dos ganchos.
//
// Son dos y no uno porque hay que decidir COMO leer los argumentos antes de
// tenerlos: las funciones del lenguaje (TRUNC, ABSO) llevan uno solo, y las del
// programador pueden llevar varios separados por coma. Primero se pregunta si
// el nombre es una funcion del programa, y recien ahi se leen sus argumentos.

// ¿`nombre` es una FUNCION declarada en este programa?
typedef int (*PaedFnExiste)(const char *nombre, void *ud);

// Corre la funcion con los argumentos ya evaluados. Devuelve 0 si salio bien,
// -1 si no, y en ese caso deja el motivo en `error`.
typedef int (*PaedFnLlamar)(const char *nombre, const Valor *args, int n,
                            Valor *out, char *error, size_t error_n, void *ud);

void expr_set_funcion(PaedFnExiste existe, PaedFnLlamar llamar, void *ud);

// Compara dos valores con las MISMAS reglas que el operador '=' del lenguaje:
// numeros por valor, y si alguno de los dos es texto, los dos por ASCII.
// Devuelve <0, 0 o >0 igual que strcmp.
//
// Se exporta para que el SEGUN elija su rama con la misma semantica que un
// SI ... = ... . Con una comparacion propia, 'SEGUN x HACER 1:' y
// 'SI x = 1 ENTONCES' podrian no coincidir, que es exactamente el tipo de
// diferencia que nadie encuentra hasta que ya rompio algo.
int expr_comparar(const Valor *a, const Valor *b);

// Lee un valor como condicion. Un NUM cuenta como falso solo si es 0, y un
// texto vacio cuenta como falso: asi una condicion nunca queda "indefinida".
int valor_verdadero(const Valor *v);

// Escribe el valor en `out` con formato legible (para ESCRIBIR).
void valor_a_texto(const Valor *v, char *out, size_t out_size);

#endif // VIMMON_EXPR_H

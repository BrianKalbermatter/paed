#ifndef VIMMON_DATOS_H
#define VIMMON_DATOS_H

#include <stddef.h>

#include "parser.h"

// El cargador de datos — `paed datos <archivo.paed>`.
//
// Por que existe: en el parcial la catedra te DA los archivos, ya cargados y ya
// ordenados. Aca no te los da nadie. Y armarlos a mano en una planilla es donde
// aparecen los dos errores que despues se pagan corriendo el programa: una
// columna que no coincide con el REGISTRO, y filas que no estan en el orden que
// la declaracion promete.
//
// Este modulo resuelve las dos, y las resuelve LEYENDO EL AMBIENTE. No tiene
// una definicion propia de que columnas lleva cada archivo ni de cual es su
// clave: eso ya lo dijiste al escribir el REGISTRO y el ORDENADO POR. El dato
// bueno es el .paed; esto solo lo obedece.
//
//     mae: ARCHIVO DE remedio ORDENADO POR codigo;
//              |                            |
//              |                            +-- por aca se ordenan las filas
//              +-- de aca salen las columnas y el tipo de cada una
//
// ── El orden de trabajo que habilita ─────────────────────────────────────────
//
//   1. Escribis el AMBIENTE: los REGISTRO y los ARCHIVO con su clausula.
//   2. Corres `paed datos` y cargas las filas.
//   3. Recien ahi escribis el PROCESO, con los datos ya fijos en disco.
//
// Ese orden importa porque un algoritmo de mezcla o de actualizacion secuencial
// SUPONE que la entrada viene ordenada. Depurar el algoritmo contra datos
// desordenados es perseguir un bug que no esta en el codigo.
//
// ── Lo que NO hace ───────────────────────────────────────────────────────────
//
// No inventa filas y no toca el .paed. Los datos los escribis vos: son los del
// enunciado, y una fila generada al azar hace que el programa corra sin
// demostrar nada.

// Cuantas filas entran en la carga. Un ejercicio de la catedra tiene decenas de
// registros, asi que 256 sobra. El buffer es estatico y no de la pila: 256
// filas por 16 campos por 128 bytes es medio mega, y eso no entra en un frame.
#define PAED_MAX_FILAS 256

// ── Como termino la carga ────────────────────────────────────────────────────
typedef enum {
    PAED_DATOS_OK = 0,          // se escribio el .csv
    PAED_DATOS_SIN_ARCHIVO,     // ese nombre no esta declarado como ARCHIVO
    PAED_DATOS_SIN_REGISTRO,    // su tipo no es un REGISTRO: no se sabe que columnas lleva
    PAED_DATOS_ERROR,           // no se pudo; el motivo va en msg
} PAEDDatosResultado;

// ── Una tabla en memoria, antes de tocar el disco ────────────────────────────
//
// Se carga entera, se ordena, y recien despues se escribe. No se escribe fila
// por fila a medida que se tipea porque ordenar necesita verlas todas: hasta
// que no esta la ultima, no se sabe donde va la primera.
typedef struct {
    // Las columnas, en el orden del REGISTRO. Es tambien el orden del CSV.
    char campos[PAED_MAX_CAMPOS][PAED_NAME_MAX];
    // El tipo declarado de cada columna, para saber si su valor es numero.
    char tipos [PAED_MAX_CAMPOS][PAED_NAME_MAX];
    int  campo_count;

    // Que columnas forman la clave, por INDICE dentro de campos[]. Sale de la
    // clausula del archivo, ya resuelta contra el REGISTRO por el parser.
    int  clave[PAED_MAX_CLAVE];
    int  clave_count;

    int  filas;
} PAEDTabla;

// El buffer de valores va aparte del struct a proposito: PAEDTabla se puede
// pasar por la pila, y medio mega de filas no.
extern char paed_datos_celdas[PAED_MAX_FILAS][PAED_MAX_CAMPOS][PAED_VAL_MAX];

// ── La logica ────────────────────────────────────────────────────────────────
//
// Nada de esta mitad imprime ni lee del teclado. Es la misma division que en
// asistente.h, y por el mismo motivo: que el menu de consola y cualquier otro
// front-end den exactamente el mismo resultado.

// Arma la tabla vacia de un archivo declarado: sus columnas, sus tipos y su
// clave. Es lo que hay que saber ANTES de pedir el primer valor.
PAEDDatosResultado paed_datos_tabla(const PAEDProgram *prog, const char *nombre,
                                    PAEDTabla *out, char *msg, size_t msg_n);

// Carga en la tabla las filas que YA tiene el .csv en disco, si existe.
// Devuelve cuantas trajo, o -1 si no pudo leerlo.
//
// Es lo que hace que cargar datos sea acumulativo: un archivo que ya viene
// armado no se vuelve a tipear, se le agregan las filas que falten y se
// reordena todo junto.
int paed_datos_cargar_csv(const PAEDProgram *prog, const char *nombre, PAEDTabla *t);

// ¿El valor sirve para esa columna? Devuelve 1 si si. Si no, deja en `msg` el
// motivo en castellano.
//
// La regla la pone el TIPO declarado, no el dato: en una columna ENTERO, 'abc'
// es un error acá y no un cero silencioso veinte lineas mas abajo.
int paed_datos_valor_valido(const PAEDTabla *t, int campo, const char *valor,
                            char *msg, size_t msg_n);

// Ordena las filas por la clave de la tabla. Es ESTABLE: dos filas con la misma
// clave conservan el orden en que se cargaron.
//
// Eso no es un detalle. En un archivo de movimientos por lotes la misma clave
// trae varios movimientos, y tienen que aplicarse en el orden en que estan: un
// orden inestable cambiaria el resultado del programa sin tocar el programa.
//
// Una tabla sin clave (archivo secuencial no ordenado) se deja como esta.
void paed_datos_ordenar(PAEDTabla *t);

// Cuantas filas repiten la clave de la anterior. Cero en un maestro, y en un
// archivo de movimientos por lotes es justamente lo que se espera.
//
// No es un error: es un dato para mostrar, porque el mismo numero significa
// cosas opuestas segun que archivo sea.
int paed_datos_claves_repetidas(const PAEDTabla *t);

// Escribe el .csv: encabezado con los campos del REGISTRO y una fila por cada
// una de la tabla, en el orden en que quedaron.
//
// PISA el archivo que haya. Es seguro porque el flujo carga primero lo que
// estaba (paed_datos_cargar_csv) y despues escribe todo junto: lo que se pisa
// es lo mismo que se acaba de leer, mas lo nuevo.
PAEDDatosResultado paed_datos_escribir(const PAEDProgram *prog, const char *nombre,
                                       const PAEDTabla *t, char *msg, size_t msg_n);

// ── El front-end de consola ──────────────────────────────────────────────────
//
// `paed datos <archivo.paed>`. argv viene desde 'datos' en adelante: argv[0] es
// "datos" y argv[1] el archivo. Devuelve 0 si salio bien.
int paed_datos(int argc, char **argv);

#endif // VIMMON_DATOS_H

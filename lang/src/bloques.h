#ifndef PAED_BLOQUES_INTERNO_H
#define PAED_BLOQUES_INTERNO_H

// La pila de bloques. Vive en src/: no es API de PAED.
//
// SI/MIENTRAS/PARA/SEGUN abren un bloque y su FIN_ lo cierra. Los bloques
// abiertos se apilan mientras se lee el archivo: el ultimo que se abrio es el
// primero que se cierra. Cuando uno cierra, recien ahi se sabe a donde salta la
// instruccion que lo abrio, y se completa hacia atras.

#include "paed/parser.h"

// Un bloque que se abrio y todavia no cerro.
typedef struct {
    PAEDKind kind;   // PAED_SI, PAED_MIENTRAS, PAED_PARA, PAED_REPETIR o PAED_SEGUN
    int      line;   // donde se abrio, para poder decirlo en el error
    int      instr;  // que instruccion lo abrio, para parchearle el salto
    int      sino;   // indice del SINO si ya aparecio; -1 si no
    int      ultimo_caso; // SEGUN: indice del ultimo CASO, para encadenar las ramas
} Abierto;

typedef struct {
    Abierto items[PAED_MAX_BLOQUES];
    int     tope;
} Pila;

// El nombre de un tipo de bloque, para nombrarlo en un error.
const char *nombre_kind(PAEDKind k);

// Trata la linea si es de bloque. Devuelve 1 si lo era —y ya la proceso—, 0 si
// no lo era y le toca a otro.
int parse_bloque(PAEDProgram *p, char *linea, int lineno, Pila *pila);

#endif // PAED_BLOQUES_INTERNO_H

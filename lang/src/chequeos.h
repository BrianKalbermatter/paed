#ifndef PAED_CHEQUEOS_INTERNO_H
#define PAED_CHEQUEOS_INTERNO_H

// Los chequeos que necesitan el programa ENTERO leido. Vive en src/: no es API
// de PAED.
//
// No se pueden hacer mientras se lee linea por linea: para saber si un LEER es
// valido hay que saber que dejo abierto el ABRIR de mas arriba, y para saber si
// una subaccion existe hay que haber leido todo, porque una puede llamar a otra
// declarada mas abajo.

#include "paed/parser.h"

// Que el modo con que se abrio cada archivo permita lo que se le hace despues:
// no se lee de uno abierto en S/ ni se graba en uno abierto en E/.
void chequear_modos(PAEDProgram *p);

// Que las claves de los argumentos existan en la definicion del procedimiento.
void chequear_claves(PAEDProgram *p);

// Que toda llamada a una subaccion apunte a una que exista, y que una FUNCION
// no se llame como si fuera un procedimiento.
void chequear_subacciones(PAEDProgram *p);

#endif // PAED_CHEQUEOS_INTERNO_H

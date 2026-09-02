#ifndef PAED_GRAFIAS_INTERNO_H
#define PAED_GRAFIAS_INTERNO_H

// Traducir las grafias de la catedra a la forma canonica del parser.
//
// La catedra escribe lo mismo de muchas formas: un mismo cierre aparece como
// FIN_SI, FinSi;, Fsi; y FIN SI;. En vez de que cada punto del parser conozca
// todas las variantes, se traducen una sola vez y el resto ve una sola forma.

#include <stddef.h>

// Normaliza la linea EN EL LUGAR y devuelve el arranque. `espacio` es cuanto
// queda en el buffer desde `linea`, porque la traduccion puede alargarla.
//
// Solo toca lineas que son UNA palabra clave sola: una linea con codigo no se
// modifica nunca, asi que ningun ';' de una instruccion corre peligro.
char *normalizar_catedra(char *linea, size_t espacio);

#endif // PAED_GRAFIAS_INTERNO_H

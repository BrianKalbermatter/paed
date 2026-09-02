#ifndef PAED_REPORTE_INTERNO_H
#define PAED_REPORTE_INTERNO_H

// El reporte de errores del parseo, compartido por todos los modulos del
// parser. Vive en src/ y no en include/paed/: paed_print_errors si es publica
// y esta declarada en paed/parser.h, pero add_error es nuestra.

#include "paed/parser.h"

// Anota un error en el programa. No imprime: los errores se acumulan y salen
// todos juntos al final. Si ya hay PAED_MAX_ERRORS, se descarta en silencio.
//
// El formato es el de printf. La linea es la del archivo .paed, no la del C.
void add_error(PAEDProgram *p, int line, const char *fmt, ...);

// Ordena los errores por linea, de forma ESTABLE. Se llama una vez al final:
// algunos errores no se encuentran leyendo —las claves, los modos, las
// llamadas a subacciones— y se detectan con el archivo entero leido, asi que
// sin esto caen todos juntos despues de los demas.
void ordenar_errores(PAEDProgram *p);

#endif // PAED_REPORTE_INTERNO_H

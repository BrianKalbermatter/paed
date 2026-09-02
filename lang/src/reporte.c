// Juntar los errores del parseo y despues imprimirlos.
//
// Se separo porque add_error es LA dependencia transversal del parser: 154
// llamadas repartidas por todo el archivo. Cada modulo que sale de parser.c
// necesita poder reportar, asi que mientras esto viviera adentro del parser
// cada corte arrastraba el reporte con el.
//
// Los errores NO se imprimen cuando pasan: se acumulan en el programa y salen
// todos juntos al final. Es a proposito y esta explicado en parser.h — los
// errores de parseo son independientes entre si, y mostrar solo el primero
// obliga a compilar cinco veces para enterarse de cinco cosas.
//
// Distinto de errores.c, que es el CATALOGO de codigos XL-NN: aquel clasifica
// un mensaje ya escrito, este los junta y los saca por pantalla.

#include "paed/parser.h"
#include "paed/errores.h"
#include "reporte.h"

#include <stdarg.h>
#include <stdio.h>

// ── Errores ───────────────────────────────────────────────────────────────────

void add_error(PAEDProgram *p, int line, const char *fmt, ...) {
    if (p->error_count >= PAED_MAX_ERRORS) return;
    PAEDError *e = &p->errors[p->error_count++];
    e->line = line;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->msg, sizeof(e->msg), fmt, ap);
    va_end(ap);
}

void paed_print_errors(const PAEDProgram *prog) {
    for (int i = 0; i < prog->error_count; i++) {
        // El codigo de la familia a la que pertenece el mensaje, para poder
        // buscarlo en xasolError/. Vacio si todavia no esta catalogado, y ahi
        // el error sale como salia antes. Ver lang/src/errores.c.
        const char *cod = paed_codigo_error(prog->errors[i].msg);
        fprintf(stderr, "%s:%d: error%s%s: %s\n",
                prog->path, prog->errors[i].line,
                cod[0] ? " " : "", cod, prog->errors[i].msg);
    }
    if (prog->error_count >= PAED_MAX_ERRORS)
        fprintf(stderr, "%s: error: demasiados errores, se corto el reporte\n", prog->path);
}

// Los errores se reportan en el orden en que se ENCONTRARON, y algunos no se
// encuentran leyendo: las claves, los modos y las llamadas a subacciones se
// verifican al final, con el programa entero en la mano. Sin ordenar, esos
// errores caen todos juntos despues de los demas, y quien lee el reporte
// arregla la linea 30, vuelve a compilar, y recien ahi se entera de que la 9
// tambien estaba mal.
//
// Es una insercion y no un qsort porque tiene que ser ESTABLE: dos errores de
// la misma linea son independientes, y el orden entre ellos es el orden en que
// se detectaron, que es el que mejor se lee.
void ordenar_errores(PAEDProgram *p) {
    for (int i = 1; i < p->error_count; i++) {
        PAEDError e = p->errors[i];
        int j = i - 1;
        while (j >= 0 && p->errors[j].line > e.line) {
            p->errors[j + 1] = p->errors[j];
            j--;
        }
        p->errors[j + 1] = e;
    }
}

// Varias sentencias en una sola linea: `a := 1; b := 2;`
//
// La catedra las permite, asi que el parser tiene que partirlas antes de mirar
// cada una. El corte solo vale FUERA de comillas y de parentesis.
//
// Lo usan los DOS bloques del programa, y por eso vive aparte: el AMBIENTE
// parte declaraciones y el PROCESO parte instrucciones. Lo unico que cambia es
// que hacer con cada pedazo, y eso entra por parametro.

#include "sentencias.h"
#include "reporte.h"
#include "texto.h"
#include "instruccion.h"

#include <stdio.h>
#include <string.h>

// ── Varias sentencias en una sola linea ───────────────────────────────────────
//
//     arr(secAlu); avz(secAlu, v);      en el PROCESO
//     a: ENTERO; b: ENTERO;             en el AMBIENTE
//
// Las dos cosas son la misma regla. El `;` es TERMINADOR (§11.1, resuelto el
// 2026-08-12 con `wiki.txt`, donde toda sentencia lo lleva, incluida la ultima
// del bloque), y lo que habilita es no tener que gastar un renglon por cada
// una: si son un solo gesto — arrancar una secuencia y traer su primer
// elemento — separarlas en dos lineas esconde que son una sola idea.
//
// Sin esto, el AMBIENTE fallaba EN SILENCIO, que es peor que fallar: en
// `s: SECUENCIA DE ENTERO; n: ENTERO;` el tipo de `s` quedaba siendo el texto
// "ENTERO; n: ENTERO" y `n` no se declaraba nunca. El programa arrancaba igual
// — un escalar nace en su primera asignacion — y recien reventaba mucho
// despues, en el primer ARR, culpando a otra cosa.
//
// El corte solo vale FUERA de comillas y de parentesis: en
// ESCRIBIR('hola; chau') ese ';' es parte del texto. Y las cabeceras de bloque
// nunca llegan aca — parse_bloque ya se las quedo — asi que el ';' que separa
// el paso del PARA queda intacto.

// Que hacer con cada pedazo. `ctx` es lo que necesite el que llama y le vuelve
// tal cual: el AMBIENTE le pasa el REGISTRO que esta abierto, el PROCESO nada.
typedef void (*ParteFn)(PAEDProgram *p, char *texto, int lineno, void *ctx);

void por_cada_sentencia(PAEDProgram *p, char *linea, int lineno,
                               ParteFn fn, void *ctx) {
    char   sent[PAED_LINEA_MAX + 2];   // +2: el ';' que se le devuelve, y el '\0'
    size_t n       = 0;
    char   comilla = 0;
    int    nivel   = 0;
    int    primero = 1;

    for (char *c = linea; ; c++) {
        int corta = (*c == '\0') || (*c == ';' && !comilla && nivel == 0);

        if (!corta) {
            if      (comilla)                 { if (*c == comilla) comilla = 0; }
            else if (*c == '\'' || *c == '"')   comilla = *c;
            else if (*c == '(')                 nivel++;
            else if (*c == ')' && nivel > 0)    nivel--;

            if (n < sizeof(sent) - 2) sent[n++] = *c;
            continue;
        }

        sent[n] = '\0';
        char *s = trim(sent);

        if (*c == ';') {
            if (!*s) {
                add_error(p, lineno, "hay un ';' sin nada adelante");
            } else {
                size_t len = strlen(s);
                s[len]     = ';';       // el que el parseo de abajo exige
                s[len + 1] = '\0';
                fn(p, s, lineno, ctx);
            }
            n = 0;
            primero = 0;
            continue;
        }

        // Fin de la linea. Lo que quedo colgando sin ';' se manda igual: el
        // error que corresponde es "falta ';'", y lo da el que parsea. Una
        // linea sin ningun ';' cae aca entera y en la primera vuelta, que es
        // como llegaba antes de que esto existiera.
        if (*s || primero) fn(p, s, lineno, ctx);
        return;
    }
}

static void una_instruccion(PAEDProgram *p, char *texto, int lineno, void *ctx) {
    (void)ctx;
    parse_instruction(p, texto, lineno);
}

void parse_sentencias(PAEDProgram *p, char *linea, int lineno) {
    por_cada_sentencia(p, linea, lineno, una_instruccion, NULL);
}

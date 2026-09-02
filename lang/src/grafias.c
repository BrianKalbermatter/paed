// Las grafias de la catedra, traducidas a una forma canonica.
//
// Es una capa de TRADUCCION, no de parseo: corre antes que todo lo demas y
// deja la linea en la forma que el resto del parser espera. Por eso vive en su
// propio archivo y no adentro del parser — nadie mas necesita saber que
// existe.
//
// El detalle de por que una capa y no un 'if' en cada lugar esta en el
// comentario de abajo, que se movio entero con el codigo.

#include "grafias.h"
#include "texto.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// ── Las grafias de la catedra ────────────────────────────────────────────────
//
// La catedra escribe lo mismo de muchas formas: entre las diapositivas, los 27
// templates oficiales de UTN-FRRe/isi-aed/Pseudocodigo y los parciales
// resueltos, un mismo cierre aparece como FIN_SI, FinSi;, Fsi; y FIN SI;.
//
// Decision del 2026-08-17: LA CATEDRA TIENE LA RAZON. Antes PAED aceptaba una
// sola forma de cada cosa y rechazaba las demas; ahora las reconoce a todas y
// las traduce a UNA forma canonica antes de que el resto del parser las mire.
//
// Por que una capa de traduccion y no un 'if' mas en cada lugar: los cierres se
// analizan en cuatro puntos distintos (parse_bloque, parse_ambiente, el cierre
// de ACCION y la pila). Con un 'if' por variante en cada punto, agregar una
// grafia obliga a tocar cuatro lugares y el dia que se olvida uno, la misma
// palabra anda en un contexto y no en el otro. Aca la lista esta escrita una
// sola vez y se lee de un vistazo.
//
// Solo se traducen lineas que son UNA palabra clave sola. Una linea con codigo
// no se toca nunca, asi que ningun ';' de una instruccion corre peligro.

typedef struct { const char *catedra; const char *paed; } Grafia;

static const Grafia GRAFIAS[] = {
    // El bloque de codigo: 'Algoritmo' sale de Sino.txt, Mientras.txt,
    // Para.txt, REGISTRO.txt y de casi todos los parciales.
    { "ALGORITMO",      "PROCESO"      },

    // El SINO: 'Contrario' esta en Sino.txt y en Segun.txt.
    { "CONTRARIO",      "SINO"         },

    // Cierres de SI
    { "FINSI",          "FIN_SI"       },
    { "FIN SI",         "FIN_SI"       },
    { "FSI",            "FIN_SI"       },

    // Cierres de MIENTRAS
    { "FINMIENTRAS",    "FIN_MIENTRAS" },
    { "FIN MIENTRAS",   "FIN_MIENTRAS" },
    { "FMIENTRAS",      "FIN_MIENTRAS" },

    // Cierres de PARA
    { "FINPARA",        "FIN_PARA"     },
    { "FIN PARA",       "FIN_PARA"     },
    { "FPARA",          "FIN_PARA"     },

    // Cierres de SEGUN: 'FinSegun;' es la forma de Segun.txt.
    { "FINSEGUN",       "FIN_SEGUN"    },
    { "FIN SEGUN",      "FIN_SEGUN"    },

    // Cierres de subaccion. 'Fin_Proc;' y 'FinSubaccion.' salen de los
    // templates oficiales; ver 05-notacion. 'Fin;' a secas NO entra aca: cierra
    // tanto una subaccion como la ACCION, y traducirlo a ciegas romperia el
    // cierre del programa. Se resuelve por contexto mas abajo.
    { "FINFUNCION",       "FIN_FUNCION"       },
    { "FIN FUNCION",      "FIN_FUNCION"       },
    { "FFUNCION",         "FIN_FUNCION"       },
    { "FINPROCEDIMIENTO", "FIN_PROCEDIMIENTO" },
    { "FIN PROCEDIMIENTO","FIN_PROCEDIMIENTO" },
    { "FIN_PROC",         "FIN_PROCEDIMIENTO" },
    { "FINPROC",          "FIN_PROCEDIMIENTO" },
    { "FPROC",            "FIN_PROCEDIMIENTO" },
    { "FINSUBACCION",     "FIN_SUBACCION"     },
    { "FIN SUBACCION",    "FIN_SUBACCION"     },

    // Cierres de REGISTRO: 'freg;' y 'fin_reg;' salen del Tema 12 y del
    // template de corte de control.
    { "FINREGISTRO",    "FIN_REGISTRO" },
    { "FIN REGISTRO",   "FIN_REGISTRO" },
    { "FINREG",         "FIN_REGISTRO" },
    { "FIN_REG",        "FIN_REGISTRO" },
    { "FREG",           "FIN_REGISTRO" },
};

// Saca el terminador de una palabra clave sola: el ';' de 'FinSi;' y el '.' de
// 'FinAccion.'. Los dos son de catedra y ninguno aporta informacion.
//
// Se aplica SOLO si lo que queda es una palabra (letras, digitos, '_' y a lo
// sumo un espacio interno, para 'FIN SI'). Una instruccion como
// 'ESCRIBIR(total);' tiene parentesis, asi que nunca entra aca y su ';' queda
// intacto — que es exactamente lo que el resto del parser necesita.
static void sacar_terminador(char *linea) {
    size_t n = strlen(linea);
    while (n > 0 && (linea[n - 1] == ';' || linea[n - 1] == '.' ||
                     isspace((unsigned char)linea[n - 1])))
        n--;
    if (n == 0 || n == strlen(linea)) { linea[n] = '\0'; return; }

    for (size_t i = 0; i < n; i++)
        if (!isalnum((unsigned char)linea[i]) && linea[i] != '_' && linea[i] != ' ')
            return;   // tiene algo que no es palabra: no se toca
    linea[n] = '\0';
}

// Traduce la linea a la forma canonica de PAED. Devuelve el mismo puntero.
//
// `espacio` es lo que queda del buffer desde `linea`: la forma canonica puede
// ser MAS LARGA que la que escribio el usuario ('FSI' -> 'FIN_SI'), asi que el
// tamaño se pide en vez de suponerlo.
char *normalizar_catedra(char *linea, size_t espacio) {
    sacar_terminador(linea);

    for (size_t i = 0; i < sizeof(GRAFIAS) / sizeof(GRAFIAS[0]); i++)
        if (kw_es(linea, GRAFIAS[i].catedra)) {
            if (strlen(GRAFIAS[i].paed) + 1 > espacio) return linea;   // no entra: se deja como vino
            memcpy(linea, GRAFIAS[i].paed, strlen(GRAFIAS[i].paed) + 1);
            return linea;
        }

    return linea;
}

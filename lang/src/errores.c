#include "paed/errores.h"

#include <string.h>

// ── El catalogo ──────────────────────────────────────────────────────────────
//
// Cada fila es una FAMILIA de errores: su codigo, un titulo corto, y los
// pedazos de texto que reconocen a un mensaje como parte de ella.
//
// Los `pedazos` son la parte FIJA del mensaje — la que no cambia de un
// programa a otro. En:
//
//     la subaccion 'devolverCuadro' no tiene bloque PROCESO
//
// lo que identifica al error es "no tiene bloque PROCESO"; el nombre de la
// subaccion es del programa, no del error.
//
// Gana el pedazo MAS LARGO que matchea, no el primero que aparece. Asi el
// orden de la tabla no decide nada y las familias pueden quedar ordenadas por
// numero, que es como se leen.
//
// El motivo se ve con un caso: "falta FIN_REGISTRO: el registro 'r' quedo
// abierto" matchea el "quedo abierto" de XL-04 y el "falta FIN_REGISTRO" de
// XL-11. Los dos son ciertos, pero el segundo dice mas — y es mas largo. La
// regla del pedazo mas largo elige el mas especifico sola.
typedef struct {
    const char *codigo;
    const char *titulo;
    const char *pedazos[8];   // terminada en NULL
} Familia;

static const Familia CATALOGO[] = {

    { "XL-01", "Falta un bloque de la subaccion",
      { "no tiene bloque PROCESO",
        "antes del PROCESO de la subaccion",
        NULL } },

    { "XL-02", "Declaracion de subaccion mal formada",
      { "no dice que tipo devuelve",
        "no dice de que tipo",
        "no es un modo de parametro",
        "parametro sin tipo",
        "parametro sin nombre",
        NULL } },

    { "XL-03", "Nombre que no esta en el AMBIENTE",
      { "no esta declarado",
        "no esta declarada",
        NULL } },

    { "XL-04", "Bloque sin cerrar, o cierre que no corresponde",
      { "quedo abierto",
        "falta FIN_",
        "falta HASTA:",
        "cierra un",
        "sin un",
        "repetido",
        NULL } },

    { "XL-05", "Falta la condicion de un bloque de control",
      { "no tiene condicion",
        "le falta la condicion",
        "se esperaba: SI",
        "se esperaba: MIENTRAS",
        "se esperaba: PARA",
        NULL } },

    { "XL-06", "PARA mal escrito",
      { "le falta ':=' con el valor inicial",
        "le falta HASTA",
        "le falta el valor inicial",
        "le falta el valor final",
        "pero no dice el incremento",
        NULL } },

    { "XL-07", "Declaracion de variable mal formada",
      { "declaracion invalida",
        "nombre de variable invalido",
        "falta el tipo de",
        "le falta 'DE <tipo>'",
        "le falta el tipo despues de 'DE'",
        NULL } },

    { "XL-08", "Procedimiento o funcion que no existe",
      { "procedimiento desconocido",
        "subaccion desconocida",
        "nombre de procedimiento invalido",
        NULL } },

    { "XL-09", "Cantidad de argumentos incorrecta",
      { "argumento(s) y se le pasaron",
        "lleva exactamente",
        "demasiados argumentos",
        "necesita al menos un destino",
        "lleva dos argumentos",
        NULL } },

    { "XL-10", "Archivos: modo de apertura y organizacion",
      { "modo de apertura",
        "no se puede ordenar por campos",
        "esta dos veces en la clave",
        "le faltan los campos",
        "lleva un solo campo",
        NULL } },

    { "XL-11", "REGISTRO mal declarado",
      { "no tiene ningun campo",
        "falta FIN_REGISTRO",
        "ya se declaro en la linea",
        "no tiene un campo",
        "campo de registro no puede ser un archivo",
        "nombre de registro invalido",
        NULL } },

    { "XL-12", "Asignacion mal formada",
      { "falta la expresion a la derecha",
        "destino de asignacion invalido",
        "falta ']' en el destino",
        "falta el indice en el destino",
        NULL } },

    { "XL-13", "Se paso un limite del lenguaje",
      { "demasiadas instrucciones",
        "demasiadas declaraciones",
        "demasiados registros",
        "demasiados conjuntos",
        "demasiadas subacciones",
        "demasiados bloques anidados",
        "no entran mas variables",
        NULL } },

    { "XL-14", "El programa no termina",
      { "sin terminar",
        "bucle infinito",
        NULL } },

    { "XL-15", "Se acabaron los datos de entrada",
      { "la entrada se termino",
        "no tiene de donde sacar los datos",
        NULL } },

    { "XL-18", "Se usa una variable antes de darle valor",
      { "todavia no tiene valor",
        "no tiene valor todavia",
        NULL } },

    { "XL-17", "La estructura del programa esta fuera de orden",
      { "falta ACCION",
        "falta PROCESO",
        "se esperaba: ACCION",
        "va justo despues de ACCION",
        "va ANTES de la ACCION",
        "instruccion antes de ACCION",
        "instruccion despues de FIN_ACCION",
        NULL } },

    { "XL-19", "CONJUNTO mal declarado, o usado sin declarar",
      { "para cerrar el conjunto",
        "nombre de conjunto invalido",
        "conjunto sin elementos",
        "tiene un elemento vacio",
        "ya estaba declarado en la linea",
        "no es un conjunto declarado",
        "va el nombre de un conjunto",
        NULL } },

    { "XL-16", "El dato no corresponde al tipo declarado",
      { "no es un numero",
        "no lleva decimales",
        "no es un valor",
        "no es parte de un numero",
        "espera V o F",
        "espera UN caracter",
        "trajo una linea vacia",
        NULL } },
};

static const int N = (int)(sizeof(CATALOGO) / sizeof(CATALOGO[0]));

const char *
paed_codigo_error(const char *msg)
{
    if (!msg || !msg[0]) return "";

    const char *mejor = "";
    size_t largo_mejor = 0;

    for (int i = 0; i < N; i++) {
        for (int j = 0; CATALOGO[i].pedazos[j]; j++) {
            const char *pedazo = CATALOGO[i].pedazos[j];
            size_t largo = strlen(pedazo);
            if (largo > largo_mejor && strstr(msg, pedazo)) {
                mejor       = CATALOGO[i].codigo;
                largo_mejor = largo;
            }
        }
    }
    if (mejor[0]) return mejor;

    // Sin catalogar. No es un problema: el mensaje sale igual, solo que sin
    // codigo. Ver el comentario de errores.h.
    return "";
}

int         paed_codigos_count(void)      { return N; }
const char *paed_codigo_nombre(int i)     { return (i >= 0 && i < N) ? CATALOGO[i].codigo : ""; }
const char *paed_codigo_titulo(int i)     { return (i >= 0 && i < N) ? CATALOGO[i].titulo : ""; }

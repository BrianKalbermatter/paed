#ifndef VIMMON_ASISTENTE_H
#define VIMMON_ASISTENTE_H

#include <stddef.h>

#include "parser.h"

// El asistente de archivos — `paed asistente <archivo.paed>`.
//
// Por que existe: en AED la estructura de los ejercicios de archivo NO cambia.
// Actualizacion, mezcla y corte de control siempre traen el mismo juego —
// maestro, movimientos, maestro nuevo, bajas, errores — y las mismas tres
// organizaciones. Como la forma es fija, se puede mirar un .paed a medio
// escribir y decir que archivos declaro, de que tipo es cada uno y cual falta.
// Esa es toda la ventaja, y es la razon de este archivo.
//
// Lo que CONTESTA, sin ejecutar el programa:
//
//     - que archivos hay declarados en el AMBIENTE, y en que linea
//     - de que tipo es cada uno: secuencial, ordenado o indexado
//     - si su tipo es un REGISTRO de verdad (si no, ABRIR va a fallar)
//     - si su .csv ya existe en disco
//
// El companero de este modulo es datos.h, que carga las FILAS de ese .csv
// ordenadas por la clave declarada. Este dice que archivos hay; aquel los llena.
//
// ── La division de este archivo ──────────────────────────────────────────────
//
// La primera mitad no imprime ni lee del teclado: devuelve datos. La segunda es
// un menu de consola encima. Que sean dos mitades y no un solo bloque es lo que
// permite que otro front-end — el editor, un test — pregunte lo mismo y obtenga
// exactamente la misma respuesta.
//
// Ninguna de estas funciones guarda estado entre llamadas. El asistente no
// tiene configuracion propia a proposito: lo unico que define el programa es el
// texto del .paed. Un asistente con memoria propia es un segundo lugar donde se
// define el programa, y ya sabemos como termina eso.

// ── Un tipo de archivo que se puede ofrecer ───────────────────────────────────────────────
//
// Salen de data/sintaxis.json ("archivos" -> "organizaciones"), filtradas por
// "en_asistente". No hay ninguna lista de tipos de archivo en el C: encender
// uno es una linea de JSON.
typedef struct {
    char etiqueta[64];        // lo que se lee en el menu: "Archivo Secuencial"
    char descripcion[160];    // el renglon de abajo, para el que no sabe cual elegir
    // El nombre interno ("secuencial", "ordenado", "indexado"). Es lo que se le
    // pasa despues a paed_asistente_crear: la etiqueta es para el humano, esto
    // es para el codigo.
    char organizacion[PAED_NAME_MAX];
    // La clausula que le corresponde, o vacio si no lleva ninguna. Secuencial
    // no lleva: se declara `arch: ARCHIVO DE reg;` y listo.
    char clausula[PAED_NAME_MAX];
    int  implementado;        // 0 = el interprete todavia no la sabe ejecutar
} PAEDOpcion;

// ── Un archivo declarado en el AMBIENTE ──────────────────────────────────────
typedef struct {
    char nombre[PAED_NAME_MAX];   // 'arch', la variable
    char tipo  [PAED_NAME_MAX];   // lo que va despues de DE
    char csv   [PAED_PATH_MAX];   // donde iria el archivo: 'arch.csv', al lado del .paed
    char organizacion[PAED_NAME_MAX];  // la que ya tiene declarada hoy
    int  linea;

    // 0 cuando lo que va despues de DE no es un REGISTRO declarado. Es el error
    // que despues revienta en ABRIR, y aca se sabe sin ejecutar nada. Con esto
    // en 0 no se puede crear el .csv: no se sabe que columnas lleva.
    int  tipo_es_registro;

    int  csv_existe;              // el .csv ya esta en disco
} PAEDArchivoInfo;

// ── Que paso al crear ────────────────────────────────────────────────────────
typedef enum {
    PAED_ASIST_CREADO = 0,      // se creo el .csv con su encabezado
    PAED_ASIST_YA_EXISTIA,      // habia uno y NO se toco (puede tener datos)
    PAED_ASIST_SIN_REGISTRO,    // el tipo no es un REGISTRO: no hay columnas
    PAED_ASIST_ERROR,           // no se pudo; el motivo va en msg
} PAEDAsistResultado;

// Llena `out` con los archivos declarados en el AMBIENTE y devuelve cuantos.
int paed_asistente_archivos(const PAEDProgram *prog, PAEDArchivoInfo *out, int max);

// ¿Hay un ARCHIVO declarado en esa linea? Es la consulta por POSICION: quien
// sabe donde esta el cursor pregunta por esa linea y se entera de si ahi hay
// una declaracion de archivo, sin recorrer la lista entera.
//
// Devuelve 1 y llena `out` si lo hay, 0 si no. `out` puede ser NULL si solo se
// quiere la respuesta.
int paed_asistente_en_linea(const PAEDProgram *prog, int linea, PAEDArchivoInfo *out);

// Los tipos de archivo que se pueden ofrecer. Devuelve cuantos puso, o -1 si no
// pudo leer la definicion del lenguaje.
int paed_asistente_opciones(PAEDOpcion *out, int max);

// Crea el .csv del archivo `nombre`, con el encabezado sacado de los campos de
// su REGISTRO. El nombre del .csv NO se elige: sale de la variable —
// `arch: ARCHIVO DE registro;` da `arch.csv` — y se arma con la misma funcion
// que usa el interprete para buscarlo despues.
//
// NUNCA pisa un .csv que ya exista: ese archivo puede tener los datos que
// cargaste a mano, y perderlos por elegir una opcion de un menu seria
// imperdonable. Si ya esta, devuelve PAED_ASIST_YA_EXISTIA y no lo toca.
//
// `organizacion` es el campo del mismo nombre de la PAEDOpcion elegida. Hoy no
// cambia el .csv (las tres organizaciones guardan las mismas columnas): sirve
// para avisar cuando el interprete todavia no la ejecuta.
//
// `msg` recibe una frase lista para mostrarle al usuario. Puede ser NULL.
PAEDAsistResultado paed_asistente_crear(const PAEDProgram *prog,
                                        const char *nombre,
                                        const char *organizacion,
                                        char *msg, size_t msg_n);

// ── El front-end de consola ──────────────────────────────────────────────────
//
// `paed asistente <archivo.paed>`. Es la MISMA logica de arriba con un menu de
// texto encima. Todo lo que decide esta arriba; aca solo se dibuja.
//
// argv viene desde 'asistente' en adelante: argv[0] es "asistente" y argv[1] el
// archivo. Devuelve 0 si salio bien.
int paed_asistente(int argc, char **argv);

#endif // VIMMON_ASISTENTE_H

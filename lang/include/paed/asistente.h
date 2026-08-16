#ifndef VIMMON_ASISTENTE_H
#define VIMMON_ASISTENTE_H

#include <stddef.h>

#include "parser.h"

// El asistente de archivos — la ventanita de la fase 4.
//
// Por que existe: en AED la estructura de los ejercicios de archivo NO cambia.
// Actualizacion, mezcla y corte de control siempre traen el mismo juego —
// maestro, movimientos, maestro nuevo, bajas, errores — y las mismas tres
// organizaciones. Cuando la forma es fija, el editor no tiene que adivinar: ya
// sabe que preguntar. Esa es toda la ventaja, y es la razon de este archivo.
//
// ── COMO SE USA DESDE editorBim ──────────────────────────────────────────────
//
// Nada de aca imprime, ni lee del teclado, ni abre ninguna ventana. Devuelve
// datos y nada mas: la ventanita la dibuja editorBim, que es el unico que sabe
// donde esta el cursor y como se pinta un recuadro.
//
// El ciclo es este:
//
//   1. El usuario escribe una linea. editorBim parsea el buffer y pregunta:
//
//          PAEDArchivoInfo info;
//          if (paed_asistente_en_linea(&prog, linea_del_cursor, &info)) {
//              // hay un ARCHIVO declarado en esta linea -> abrir la ventanita
//          }
//
//   2. Pide las opciones y las dibuja:
//
//          PAEDOpcion ops[PAED_MAX_ORGANIZACIONES];
//          int n = paed_asistente_opciones(ops, PAED_MAX_ORGANIZACIONES);
//
//   3. El usuario elige una, la ventanita se cierra, y se aplica:
//
//          char msg[192];
//          paed_asistente_crear(&prog, info.nombre, ops[i].organizacion,
//                               msg, sizeof(msg));
//          // msg trae que paso, listo para mostrar en la barra de estado
//
// Ninguna de estas funciones guarda estado entre llamadas. El asistente no
// tiene configuracion propia a proposito (KANBAN #f4-asistente): lo unico que
// define el programa es el texto del .paed. Un asistente con memoria propia es
// un segundo lugar donde se define el programa, y ya sabemos como termina eso.

// ── Una opcion de la ventanita ───────────────────────────────────────────────
//
// Salen de data/sintaxis.json ("archivos" -> "organizaciones"), filtradas por
// "en_asistente". No hay ninguna lista de tipos de archivo en el C: encender
// uno es una linea de JSON.
typedef struct {
    char etiqueta[64];        // lo que se lee en la ventanita: "Archivo Secuencial"
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

// ¿Hay un ARCHIVO declarado en esa linea? Es el DISPARADOR de la ventanita:
// editorBim sabe en que linea esta el cursor y pregunta por ella.
//
// Devuelve 1 y llena `out` si lo hay, 0 si no. `out` puede ser NULL si solo se
// quiere la respuesta.
int paed_asistente_en_linea(const PAEDProgram *prog, int linea, PAEDArchivoInfo *out);

// Las opciones que van en la ventanita. Devuelve cuantas puso, o -1 si no pudo
// leer la definicion del lenguaje.
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
// `paed asistente <archivo.paed>`. No es la ventanita: es la MISMA logica de
// arriba con un menu de texto encima, para poder probarla sin editorBim. Que la
// ventanita y esto den lo mismo es lo que hace que probar aca valga.
//
// argv viene desde 'asistente' en adelante: argv[0] es "asistente" y argv[1] el
// archivo. Devuelve 0 si salio bien.
int paed_asistente(int argc, char **argv);

#endif // VIMMON_ASISTENTE_H

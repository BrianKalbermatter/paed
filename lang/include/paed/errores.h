#ifndef VIMMON_PAED_ERRORES_H
#define VIMMON_PAED_ERRORES_H

// Codigos de error de PAED.
//
// Un error del parser o del interprete sale asi:
//
//     Hola.paed:6: error XL-01: la subaccion 'devolverCuadro' no tiene bloque PROCESO
//                        ^^^^^
//
// Ese XL-01 es lo que se busca en la documentacion: cada codigo tiene su
// archivo en `xasolError/XLerror-01.md`, con el error explicado, por que pasa
// y como se arregla. El indice de todos esta en `errores.md`.
//
// ── Por que un codigo y no solo el mensaje ───────────────────────────────────
//
// El mensaje explica QUE paso en ESTE programa; el codigo dice QUE CLASE de
// error es. Sirven para cosas distintas: el mensaje se lee en el momento, el
// codigo se busca cuando el mensaje no alcanzo. Y buscar por mensaje no anda,
// porque los mensajes llevan el nombre de tu variable adentro.
//
// ── Por que una tabla y no un codigo en cada sitio ───────────────────────────
//
// Hay unos 196 lugares que reportan errores. Ponerle un numero a cada uno
// seria numerar 196 veces, y peor: dos sitios que dicen lo mismo tendrian
// numeros distintos segun quien los escribio.
//
// Aca los codigos son FAMILIAS, y a que familia pertenece un mensaje se
// resuelve por su texto fijo — la parte que no cambia de programa a programa.
// Catalogar un error nuevo es agregar una fila y escribir su .md; no se toca
// el parser.
//
// No vive en data/sintaxis.json, a diferencia de casi todo lo demas, porque
// no es una regla del lenguaje: es un contrato entre los mensajes y la
// documentacion. Cambiarlo no cambia que programas son validos.

#include <stddef.h>

#define PAED_CODIGO_MAX 8

// El codigo que le corresponde a un mensaje, o cadena vacia si todavia no
// esta catalogado. Nunca devuelve NULL.
//
// Que un error no tenga codigo no es un problema: sale como salia antes, y el
// dia que valga la pena explicarlo se le agrega la fila.
const char *paed_codigo_error(const char *msg);

// Cuantos codigos hay en el catalogo, y el i-esimo con su descripcion corta.
// Sirve para `paed errores`, que los lista sin abrir la documentacion.
int         paed_codigos_count(void);
const char *paed_codigo_nombre(int i);       // "XL-01"
const char *paed_codigo_titulo(int i);       // "Falta un bloque de la subaccion"

#endif // VIMMON_PAED_ERRORES_H

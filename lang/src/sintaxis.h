#ifndef PAED_SINTAXIS_INTERNO_H
#define PAED_SINTAXIS_INTERNO_H

// Lo que el parser necesita del modulo de sintaxis y no puede ir en el header
// publico.
//
// Vive en src/ y no en include/paed/ a proposito: paed/parser.h NO expone
// cJSON, para que quien use PAED como libreria no herede esa dependencia, y
// todo lo de aca devuelve o recibe cJSON*. Es la frontera entre "la API de
// PAED" y "lo que dos archivos nuestros se cuentan entre ellos".
//
// Lo publico de verdad —paed_syntax_load, paed_categoria_de_palabra y
// compania— sigue declarado en paed/parser.h.

#include "cJSON.h"
#include <stddef.h>

// ── El JSON crudo ────────────────────────────────────────────────────────────

// Una seccion de primer nivel de sintaxis.json, o NULL si no esta.
cJSON *syn_seccion(const char *nombre);

// Los nombres de las librerias cargadas, separados por coma, para poder
// nombrarlas en un mensaje de error. Devuelve cuantas hay.
int syn_libs_nombres(char *out, size_t n);

// ── Los procedimientos que define el lenguaje ────────────────────────────────

// La definicion de un procedimiento por su nombre, o NULL si no existe.
cJSON *proc_def(const char *nombre);

// El nombre CANONICO de un procedimiento: el que se escribe en los mensajes
// cuando el usuario uso una grafia alternativa.
const char *proc_canonico(cJSON *def);

// La definicion de un parametro dentro de un procedimiento.
cJSON *param_def(cJSON *proc, const char *clave);

// Si acepta una cantidad libre de argumentos.
int proc_es_variadico(cJSON *proc);

// ── Los modos de apertura: ABRIR E/(arch) ────────────────────────────────────

// Los modos que admite el procedimiento, o NULL si no admite ninguno.
cJSON *modos_def(cJSON *proc);

// La lista de modos validos, ya escrita, para poder nombrarlos en un error.
void modos_listados(cJSON *modos, char *out, size_t n);

#endif // PAED_SINTAXIS_INTERNO_H

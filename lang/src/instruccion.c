// Parsear UNA instruccion del PROCESO.
//
//     ESCRIBIR('hola')        una llamada con argumentos
//     a := 1                  una asignacion
//     ABRIR E/(arch)          una llamada con modo
//
// La linea llega limpia: sentencias.c ya la partio por ';', grafias.c ya la
// normalizo, y bloques.c ya se quedo con las cabeceras de bloque. Aca solo
// queda una instruccion suelta.
//
// Es el modulo mas grande de los que salieron de parser.c y el ultimo en
// salir, a proposito: todo lo que necesitaba —el texto, el reporte, el
// programa, sintaxis.json— tenia que estar afuera primero.
//
// Ninguna de sus funciones auxiliares se usa desde otro lado: la unica puerta
// es parse_instruction. Por eso el header tiene una sola linea.

#include "instruccion.h"
#include "sintaxis.h"
#include "programa.h"
#include "reporte.h"
#include "texto.h"

#include "cJSON.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── Validacion de valores segun el tipo declarado en sintaxis.json ────────────

static int valida_valor(const char *tipo, const char *val) {
    if (strcmp(tipo, "VEC3") == 0) {
        float x, y, z;
        return val[0] == '(' && sscanf(val, "(%f,%f,%f)", &x, &y, &z) == 3;
    }
    if (strcmp(tipo, "HEX") == 0) {
        if (val[0] != '#') return 0;
        size_t n = strlen(val + 1);
        if (n != 3 && n != 6 && n != 8) return 0;
        for (const char *c = val + 1; *c; c++)
            if (!isxdigit((unsigned char)*c)) return 0;
        return 1;
    }
    if (strcmp(tipo, "NUM") == 0) {
        char *fin = NULL;
        strtod(val, &fin);
        return fin && *fin == '\0' && fin != val;
    }
    if (strcmp(tipo, "ID") == 0) return es_identificador(val);

    // EXPR: cualquier cosa que se evalue en tiempo de ejecucion.
    //
    // Existe porque NUM valida con strtod, o sea que solo acepta un numero
    // ESCRITO: 'x = 3' pasa, 'x = px' o 'x = px + 1' no. Para una escena
    // estatica alcanza, pero un juego calcula TODO — la camara sale de una
    // variable que cambia en cada cuadro. Un parametro EXPR se acepta tal cual
    // y lo resuelve el interprete cuando corre, con las variables ya cargadas.
    if (strcmp(tipo, "EXPR") == 0) return val[0] != '\0';

    return 1;  // tipo desconocido: no bloquea
}

// ── Parseo de una instruccion: PROC(clave = valor, ...); ──────────────────────

// Parte el interior de los parentesis en argumentos, cortando SOLO en las comas
// de nivel 0. Asi (0,2,5) sigue siendo un unico valor y no tres argumentos.
static int split_args(char *inner, char *out[], int max, PAEDProgram *p, int line) {
    int  n = 0, depth = 0;
    char *inicio = inner;

    // Se recuerda CUAL comilla abrio el texto, no solo que hay uno abierto: asi
    // 'no dijo "hola"' no se cierra en la comilla doble de adentro.
    //
    // BUG arreglado el 2026-08-17: antes solo la comilla DOBLE abria texto, asi
    // que una coma adentro de un texto con comilla SIMPLE cortaba el argumento
    // al medio. Y la comilla simple es justo la forma de la catedra
    // (AED_2021_UnI.pdf:10), asi que 'Ingrese un valor entero, vamos a...' —
    // una linea del template Si.txt — no parseaba.
    char comilla = 0;

    for (char *c = inner; ; c++) {
        if (!comilla && (*c == '"' || *c == '\'')) comilla = *c;
        else if (comilla && *c == comilla)          comilla = 0;
        int en_texto = comilla != 0;

        if (!en_texto && (*c == '(' || *c == '[')) depth++;
        if (!en_texto && (*c == ')' || *c == ']')) depth--;

        if ((*c == '\0') || (*c == ',' && depth == 0 && !en_texto)) {
            char fin = *c;
            *c = '\0';
            char *arg = trim(inicio);
            if (*arg) {
                if (n >= max) {
                    add_error(p, line, "demasiados argumentos (maximo %d)", max);
                    return n;
                }
                out[n++] = arg;
            }
            if (fin == '\0') break;
            inicio = c + 1;
        }
    }
    return n;
}

// destino := expresion;   (el ';' ya se saco)
// La expresion se guarda CRUDA: todavia no hay evaluador. Igual se valida que
// el destino sea un identificador y que haya algo del lado derecho, porque
// aceptar basura ahora es esconder el error para mas adelante.
// Copia un valor de argumento avisando si no entra.
//
// strncpy trunca en silencio, y el pedazo que queda casi siempre es texto sin
// la comilla de cierre: el error que sale despues es "falta la comilla de
// cierre", que manda a buscar un problema que no existe. Un texto de mas de
// PAED_VAL_MAX bytes tiene que decir eso mismo.
//
// El limite es en BYTES, no en caracteres: una linea de guiones Unicode gasta
// 3 bytes por guion y llega al tope con un tercio de los simbolos.
static int copiar_valor(PAEDProgram *p, int lineno, char *destino,
                        const char *valor, const char *donde) {
    size_t n = strlen(valor);
    if (n >= PAED_VAL_MAX) {
        add_error(p, lineno,
                  "el valor de %s ocupa %zu bytes y el maximo es %d "
                  "(ojo: cada simbolo Unicode gasta hasta 4 bytes)",
                  donde, n, PAED_VAL_MAX - 1);
        return -1;
    }
    memcpy(destino, valor, n + 1);
    return 0;
}

static void parse_asignacion(PAEDProgram *p, char *linea, int lineno, char *op) {
    *op = '\0';
    char *destino = trim(linea);
    char *expr    = trim(op + 2);

    if (!*expr) {
        add_error(p, lineno, "falta la expresion a la derecha de ':=' en '%s'", destino);
        return;
    }

    // El destino puede ser A[i]: se parte en nombre e indice, y el indice queda
    // CRUDO igual que el resto de las expresiones, para que lo evalue expr.c en
    // tiempo de ejecucion (i cambia en cada vuelta del bucle).
    char *corchete = strchr(destino, '[');
    char  indice[PAED_VAL_MAX] = {0};

    if (corchete) {
        size_t n = strlen(destino);
        if (destino[n - 1] != ']') {
            add_error(p, lineno, "falta ']' en el destino '%s'", destino);
            return;
        }
        destino[n - 1] = '\0';     // saca el ']'
        *corchete      = '\0';     // corta el nombre antes del '['
        snprintf(indice, sizeof(indice), "%s", trim(corchete + 1));
        destino = trim(destino);

        if (!*indice) {
            add_error(p, lineno, "falta el indice en el destino '%s[]'", destino);
            return;
        }
    }

    // es_campo y no es_identificador: el destino puede ser 'pori.vx'.
    if (!es_campo(destino)) {
        add_error(p, lineno, "destino de asignacion invalido: '%s'", destino);
        return;
    }

    PAEDInstr *in = nueva_instr(p, PAED_ASIGNA, lineno);
    if (!in) return;
    strncpy(in->proc, destino, PAED_NAME_MAX - 1);
    strncpy(in->cond, expr,    PAED_COND_MAX - 1);

    // Se guarda como argumento con nombre en vez de un campo nuevo: el
    // interprete distingue "escalar" de "elemento" por si existe 'indice'.
    if (corchete) {
        in->arg_count = 1;
        snprintf(in->args[0].key, PAED_KEY_MAX, "indice");
        snprintf(in->args[0].val, PAED_VAL_MAX, "%s", indice);
    }
}

// Busca el '=' que separa clave de valor, SALTEANDO lo que este entre
// comillas. Con strchr pelado, ESCRIBIR("a = b") se partia por el '=' de
// adentro del texto y el literal quedaba destrozado.
//
// Tampoco cuentan los '=' que son parte de OTRO operador. Sin esto,
// ESCRIBIR("x ", a <= b) partia por el '=' de '<=' y el argumento quedaba
// hecho pedazos. Los tres casos son '<=', '>=' y '=='; el '<>' no lleva '='.
static char *igual_separador(char *s) {
    int en_texto = 0;
    char comilla = 0;
    for (char *c = s; *c; c++) {
        if (!en_texto && (*c == '"' || *c == '\'')) { en_texto = 1; comilla = *c; continue; }
        if (en_texto) { if (*c == comilla) en_texto = 0; continue; }
        if (*c == '=') {
            if (c > s && (c[-1] == '<' || c[-1] == '>' || c[-1] == '=')) continue;
            if (c[1] == '=') { c++; continue; }
            return c;
        }
    }
    return NULL;
}

// ¿Este procedimiento admite argumentos con nombre (`clave = valor`)?
//
// Solo si DECLARA parametros. Las dos familias no se tocan: los procedimientos
// del LENGUAJE (ESCRIBIR, LEER, ARR, AVZ...) son variadicos y declaran
// "params": [] — reciben valores. Los de una LIBRERIA (escena.json: CUBO,
// MOVER...) declaran sus parametros con nombre y no son variadicos — ahi un
// '=' separa clave de valor.
//
// Se mira lo que el procedimiento DECLARA y no una lista de nombres en C, por
// el mismo motivo de siempre: la definicion del lenguaje vive en el JSON.
static int proc_admite_clave_valor(cJSON *def) {
    cJSON *params = cJSON_GetObjectItem(def, "params");
    return cJSON_IsArray(params) && cJSON_GetArraySize(params) > 0;
}

// ── Una comparacion no va como argumento ────────────────────────────────────
//
// Busca un operador de comparacion suelto en un argumento, fuera de comillas.
// Devuelve donde empieza y deja su largo en `largo`; NULL si no hay.
//
// DE DONDE SALE, con honestidad sobre que dice y que NO dice la catedra.
//
// Lo que la teoria SI fija (TEORIA_COMPLETA.txt:319-320, "OPERADORES
// RELACIONALES / Sirven para comparaciones. Devuelven resultado logico: V o
// F"): los seis comparadores son  =  <>  <  <=  >  >=  y una comparacion es
// una expresion con valor. `==` NO EXISTE en AED — la igualdad es '=' sola
// (TEORIA_COMPLETA.txt:324, y wiki_paed.txt:225 lo anota como error de
// escritura arrastrado en los .paed del corpus).
//
// Lo que la teoria NO dice: nada sobre si una comparacion puede ir como
// argumento. Sus dos unicos ejemplos de ESCRIBIR (TEORIA_COMPLETA.txt:442 y
// 445) pasan un texto y una variable, y el corpus de la catedra nunca pasa una
// comparacion — pero que algo no aparezca no es una regla que lo prohiba.
//
// Asi que esto es una DECISION DE PAED, no una cita: se rechaza. El motivo es
// el de siempre en este parser — antes el '=' caia en el troceado
// `clave = valor`, se comia el lado izquierdo y devolvia el derecho sin avisar
// (`ESCRIBIR("x ", 3 = 3)` daba `3`). Entre aceptar una forma que la catedra
// nunca escribe y rechazarla nombrandola, se rechaza: un resultado equivocado
// en silencio es lo contrario de lo que este parser promete (ver
// tests/errores.paed: "el parser NUNCA ignora en silencio").
//
// El mensaje no dice que el procedimiento "muestra" algo, porque esto corre
// sobre TODOS los variadicos y ARR o CERRAR no muestran nada.
//
// Ojo con el orden: los de dos caracteres van ANTES que los de uno, porque si
// no '<=' se detecta como '<' y el mensaje nombra el operador equivocado.
static const char *comparador_suelto(const char *s, int *largo) {
    static const char *OPS2[] = { "<>", "<=", ">=" };
    int en_texto = 0;
    char comilla = 0;

    for (const char *c = s; *c; c++) {
        if (!en_texto && (*c == '"' || *c == '\'')) { en_texto = 1; comilla = *c; continue; }
        if (en_texto) { if (*c == comilla) en_texto = 0; continue; }

        for (size_t i = 0; i < sizeof(OPS2) / sizeof(*OPS2); i++)
            if (strncmp(c, OPS2[i], 2) == 0) { *largo = 2; return c; }

        if (*c == '=' || *c == '<' || *c == '>') { *largo = 1; return c; }
    }
    return NULL;
}

// ── Consola o archivo: cual de las dos formas es ──────────────────────────────

// Busca una declaracion por nombre EXACTO. Los identificadores de PAED
// distinguen mayusculas ('total' y 'Total' son dos variables), asi que va
// strcmp y no strcasecmp.
static const PAEDDecl *decl_por_nombre(const PAEDProgram *p, const char *nombre) {
    for (int i = 0; i < p->decl_count; i++)
        if (strcmp(p->decls[i].name, nombre) == 0) return &p->decls[i];
    return NULL;
}

// Igual pero ignorando mayusculas. Solo se usa para EXPLICAR un error: si
// alguien declaro 'arch' y escribio 'Arch', sin esto la instruccion degradaria
// a forma consola en silencio, que es justo lo que el parser no hace.
static const PAEDDecl *decl_parecida(const PAEDProgram *p, const char *nombre) {
    for (int i = 0; i < p->decl_count; i++)
        if (strcasecmp(p->decls[i].name, nombre) == 0) return &p->decls[i];
    return NULL;
}

// Decide si esta instruccion es la forma de consola o la de archivo, y la
// marca. Devuelve 0 si esta bien, -1 si hubo error.
//
// La regla es una sola: manda el PRIMER ARGUMENTO. Si es una variable
// declarada como archivo, es operacion de archivo; si no, es consola. Se
// resuelve por instruccion, asi que un programa puede tener tres archivos y
// diez LEER de consola sin que se pisen: cada linea se mira sola.
//
// Que procedimientos tienen dos formas NO se decide aca: sale de
// sintaxis.json, del campo "forma_archivo". Hardcodear la lista en C seria
// tener la definicion del lenguaje en dos lados.
static int resolver_forma(PAEDProgram *p, PAEDInstr *instr, cJSON *def,
                          int lineno, const char *nombre) {
    cJSON *fa    = cJSON_GetObjectItem(def, "forma_archivo");
    cJSON *fs    = cJSON_GetObjectItem(def, "forma_secuencia");
    cJSON *pa    = cJSON_GetObjectItem(def, "primer_arg");

    // Que espera el procedimiento en su primer argumento. Lo dice
    // sintaxis.json y no una lista en C, por la misma razon de siempre: la
    // definicion del lenguaje vive en un solo lado.
    //
    //   "archivo"     ABRIR — sin un archivo declarado es error
    //   "secuencia"   ARR, AVZ — idem con secuencias
    //   "declarado"   CREAR, CERRAR — la declaracion DECIDE la forma, pero no
    //                 se exige ninguna: las dos valen, y CERRAR(arch) y
    //                 CERRAR(sec) son la misma palabra sobre cosas distintas
    const char *quiere    = cJSON_IsString(pa) ? pa->valuestring : "";
    int         exige_arc = strcmp(quiere, "archivo")   == 0;
    int         exige_sec = strcmp(quiere, "secuencia") == 0;
    int         exige     = exige_arc || exige_sec;
    int         mira      = exige || strcmp(quiere, "declarado") == 0;

    if (!cJSON_IsObject(fa) && !cJSON_IsObject(fs) && !mira) {
        instr->forma = PAED_FORMA_UNICA;
        return 0;
    }

    // Como se nombra en los mensajes lo que este procedimiento espera.
    const char *cosa = exige_sec ? "una secuencia" : "un archivo";

    instr->forma = PAED_FORMA_CONSOLA;
    if (instr->arg_count == 0) {
        // ABRIR() y ARR() sin nada son error; ESCRIBIR() vacio no.
        if (exige) {
            add_error(p, lineno, "%s necesita %s como primer argumento", nombre, cosa);
            return -1;
        }
        return 0;
    }

    const char *primero = instr->args[0].val;

    // Lo que no es un nombre suelto no puede ser un archivo ni una secuencia.
    // Descarta gratis ESCRIBIR("hola"), LEER(A[i]) y ESCRIBIR(3*x). Se usa
    // es_identificador y no es_campo porque 'p.campo' tampoco es ninguna de
    // las dos cosas.
    if (!es_identificador(primero)) {
        if (exige) {
            add_error(p, lineno, "%s trabaja sobre %s y '%s' no es el nombre de una",
                      nombre, cosa, primero);
            return -1;
        }
        return 0;
    }

    const PAEDDecl *d = decl_por_nombre(p, primero);

    // Secuencia. Va ANTES de la rama de archivo porque las dos entran por el
    // mismo camino y una secuencia nunca es un archivo: distinguirlas aca es
    // lo que permite que ESCRIBIR(secSal, v) grabe en la secuencia mientras
    // ESCRIBIR(arch, reg) sigue siendo la forma de archivo.
    if (d && d->es_secuencia) {
        instr->forma = PAED_FORMA_SECUENCIA;
        snprintf(instr->args[0].key, PAED_KEY_MAX, "secuencia");

        if (cJSON_IsObject(fs)) {
            cJSON *n = cJSON_GetObjectItem(fs, "args");
            if (cJSON_IsNumber(n) && instr->arg_count != n->valueint) {
                add_error(p, lineno,
                          "%s sobre la secuencia '%s' lleva exactamente %d argumentos: "
                          "%s(%s, dato)", nombre, primero, n->valueint, nombre, primero);
                return -1;
            }
        }
        return 0;
    }

    if (d && exige_sec) {
        add_error(p, lineno,
                  "%s trabaja sobre una secuencia, pero '%s' se declaro en la linea %d como %s",
                  nombre, primero, d->line, d->type[0] ? d->type : "otra cosa");
        return -1;
    }

    if (d && d->es_archivo) {
        instr->forma = PAED_FORMA_ARCHIVO;
        // La clave nombra el dato; el enum de arriba decide el camino. Asi el
        // interprete lo lee con paed_get_arg(in, "archivo") como todo lo demas.
        snprintf(instr->args[0].key, PAED_KEY_MAX, "archivo");

        if (cJSON_IsObject(fa)) {
            cJSON *n = cJSON_GetObjectItem(fa, "args");
            if (cJSON_IsNumber(n) && instr->arg_count != n->valueint) {
                add_error(p, lineno,
                          "%s sobre el archivo '%s' lleva exactamente %d argumentos: "
                          "%s(%s, registro)",
                          nombre, primero, n->valueint, nombre, primero);
                return -1;
            }
        }
        // El destino no puede ser otro archivo: se lee DESDE un archivo HACIA
        // un registro en memoria.
        if (instr->arg_count >= 2 && es_identificador(instr->args[1].val)) {
            const PAEDDecl *dest = decl_por_nombre(p, instr->args[1].val);
            if (dest && dest->es_archivo) {
                add_error(p, lineno,
                          "el destino de %s(%s, ...) no puede ser otro archivo: '%s' (linea %d)",
                          nombre, primero, dest->name, dest->line);
                return -1;
            }
        }
        return 0;
    }

    // Declarado, pero NO es un archivo.
    if (d && exige) {
        add_error(p, lineno,
                  "%s trabaja sobre un archivo, pero '%s' se declaro en la linea %d como %s",
                  nombre, primero, d->line, d->type[0] ? d->type : "otra cosa");
        return -1;
    }

    // No esta declarado. Si el nombre existe con otras mayusculas, es casi
    // seguro un error de tipeo y hay que decirlo: si no, la instruccion se
    // trataria como consola sin avisar.
    if (!d) {
        const PAEDDecl *parecida = decl_parecida(p, primero);
        if (parecida && (parecida->es_archivo || parecida->es_secuencia)) {
            add_error(p, lineno,
                      "'%s' no esta declarado, pero si '%s' (linea %d): "
                      "los identificadores distinguen mayusculas",
                      primero, parecida->name, parecida->line);
            return -1;
        }
        if (exige) {
            add_error(p, lineno,
                      "%s trabaja sobre %s y '%s' no esta declarado en el AMBIENTE "
                      "(falta '%s: %s;')", nombre, cosa, primero, primero,
                      exige_sec ? "SECUENCIA DE <tipo>" : "ARCHIVO DE <tipo>");
            return -1;
        }
    }

    // Queda como consola. Es lo correcto: un escalar NO necesita declararse
    // (nace en su primera asignacion), asi que LEER(salario) sin declarar es
    // legitimo. Al archivo y a la secuencia sin declarar los cazan ABRIR y
    // ARR, que si los exigen.
    return 0;
}

// ── El modo de apertura: ABRIR E/(arch) ───────────────────────────────────────
//
// La catedra escribe el modo AFUERA de los parentesis, entre el nombre del
// procedimiento y el '('. Es la unica parte del lenguaje que no es ni un
// argumento ni una palabra clave, asi que se separa antes de todo lo demas.
//
// Todas estas son la misma instruccion:
//
//     ABRIR E/(arch)      ABRIR e/ (arch)      ABRIRe/s(arch)
//     ABRIR /S(arch)      ABRIR E/S (arch)
//
// El espacio no cuenta y la mayuscula tampoco: son formas que aparecen en el
// material tal cual (197 'E/', 67 'S/', 14 'E/S', 7 con la barra al reves), y
// obligar a una sola seria inventar una regla que las fuentes no tienen.
//
// Recorta `nombre` dejando solo el procedimiento, y deja el modo normalizado en
// `modo`: sin barra y en mayuscula. Devuelve 0 si pudo, -1 si ya reporto error.
static int separar_modo(PAEDProgram *p, int lineno, char *nombre,
                        char *modo, size_t nmodo) {
    // Para los mensajes: abajo se recorta `nombre` en el lugar.
    char crudo[PAED_NAME_MAX];
    snprintf(crudo, sizeof(crudo), "%s", nombre);

    // El nombre del procedimiento es la primera corrida de caracteres de
    // identificador. Lo que sigue tiene que ser el modo y nada mas.
    size_t corte = 0;
    while (nombre[corte] && (isalnum((unsigned char)nombre[corte]) || nombre[corte] == '_'))
        corte++;

    char   letras[PAED_MODO_MAX * 2] = {0};   // 'E', 'S' — sin la barra
    size_t nl     = 0;
    int    barras = 0;

    for (const char *c = nombre + corte; *c; c++) {
        if (isspace((unsigned char)*c)) continue;
        if (*c == '/') { barras++; continue; }
        if (!isalpha((unsigned char)*c) || nl + 1 >= sizeof(letras)) {
            add_error(p, lineno, "nombre de procedimiento invalido: '%s'", crudo);
            return -1;
        }
        letras[nl++] = (char)toupper((unsigned char)*c);
    }
    letras[nl] = '\0';

    // La barra es OPCIONAL desde el 2026-08-17. Los templates oficiales de la
    // catedra escriben 'ABRIRe(arch)' y 'ABRIRs(arch)' con el modo pegado y sin
    // barra ninguna (ARCHIVO_LEER.txt, ARCHIVO_CREAR.txt, CORTE DE CONTROL,
    // MEZCLA, ACTUALIZACION). Dos barras siguen siendo error: ahi no hay una
    // forma de la catedra que interpretar, hay un modo escrito mal.
    if (barras > 1) {
        add_error(p, lineno,
                  "'%s' tiene %d barras: el modo de apertura lleva UNA sola, "
                  "como en ABRIR E/(arch)", crudo, barras);
        return -1;
    }

    nombre[corte] = '\0';

    // 'ABRIRe/s' viene sin espacio, asi que la 'e' quedo pegada al nombre. Se
    // le devuelven las letras al modo de a una, por la izquierda — que es de
    // donde salieron — hasta que lo que queda sea un procedimiento que admita
    // modo. Esto es lo que hace que el espacio de verdad no cuente.
    size_t largo = strlen(nombre);
    while (!proc_def(nombre) && largo > 1 && isalpha((unsigned char)nombre[largo - 1])) {
        if (nl + 1 >= sizeof(letras)) break;
        memmove(letras + 1, letras, nl + 1);
        letras[0] = (char)toupper((unsigned char)nombre[largo - 1]);
        nl++;
        nombre[--largo] = '\0';
    }

    cJSON *def = proc_def(nombre);
    if (!def) {
        add_error(p, lineno, "nombre de procedimiento invalido: '%s'", crudo);
        return -1;
    }

    cJSON *modos = modos_def(def);
    if (!modos) {
        // El modo no es decoracion: dice si el archivo se puede leer o grabar.
        // Ponerlo en LEER no significa nada, y aceptarlo callado seria hacerle
        // creer al que lo escribio que ahi tambien decide algo.
        add_error(p, lineno,
                  "%s no lleva modo de apertura: el modo se escribe una sola vez, "
                  "en el ABRIR", nombre);
        return -1;
    }

    if (!cJSON_GetObjectItem(modos, letras)) {
        char validos[PAED_MSG_MAX / 2];
        modos_listados(modos, validos, sizeof(validos));
        add_error(p, lineno, "modo de apertura invalido en '%s': los de %s son %s",
                  crudo, nombre, validos);
        return -1;
    }

    snprintf(modo, nmodo, "%s", letras);
    return 0;
}

void parse_instruction(PAEDProgram *p, char *linea, int lineno) {
    // 1. El ';' final es OPCIONAL desde el 2026-08-17.
    //
    // Antes era obligatorio. El material de catedra es inconsistente al
    // respecto — la wiki lo pone en todas, AED_2021_UnI.pdf no lo pone en la
    // ultima, y los templates oficiales lo saltean seguido (Si.txt escribe
    // `Escribir('Ingrese un valor entero...')` sin ';' y la linea de abajo con
    // ';') — y la decision del 2026-08-17 es que manda la catedra.
    //
    // Que sea opcional NO afloja nada: el ';' sigue siendo lo que SEPARA varias
    // sentencias en un mismo renglon, que es el trabajo por el que estaba. El
    // caso que protegia — `s: SECUENCIA DE ENTERO; n: ENTERO;` quedando con `s`
    // de tipo basura — lo cubre el corte por ';', no el ';' del final.
    size_t len = strlen(linea);
    if (len > 0 && linea[len - 1] == ';') linea[len - 1] = '\0';
    linea = trim(linea);

    if (!*linea) return;   // un ';' suelto no es una instruccion, pero tampoco un error

    // 2. ¿Es una asignacion? Lo es si hay ':=' y aparece ANTES del primer '(',
    //    para que 'x := f(y);' cuente como asignacion y 'AVZ(a, b);' no.
    char *abre     = strchr(linea, '(');
    char *asignaop = strstr(linea, ":=");
    if (asignaop && (!abre || asignaop < abre)) {
        parse_asignacion(p, linea, lineno, asignaop);
        return;
    }

    // 3. Nombre del procedimiento, hasta el '('
    if (!abre) {
        // Una palabra sola es una llamada a subaccion SIN parentesis, que es
        // como las llama el corpus: el template de corte de control escribe
        // 'Inicializar', 'tratar_corte;' y 'corte_3;' sin ninguno.
        //
        // No se pregunta ACA si esa subaccion existe, por lo mismo que en la
        // llamada con parentesis: puede estar declarada mas abajo. Se anota y
        // chequear_subacciones() lo verifica al final. Si el nombre no es de
        // nadie, el error sale ahi y dice exactamente eso.
        if (es_identificador(linea)) {
            if (p->instr_count >= PAED_MAX_INSTRS) {
                add_error(p, lineno, "demasiadas instrucciones (maximo %d)", PAED_MAX_INSTRS);
                return;
            }
            PAEDInstr *llam = &p->instrs[p->instr_count++];
            memset(llam, 0, sizeof(*llam));
            llam->kind         = PAED_LLAMADA;
            llam->es_subaccion = 1;
            llam->salto        = -1;
            llam->line         = lineno;
            strncpy(llam->proc, linea, PAED_NAME_MAX - 1);
            return;
        }

        add_error(p, lineno, "instruccion sin parentesis: se esperaba PROCEDIMIENTO(...)");
        return;
    }

    *abre = '\0';
    char *nombre = trim(linea);
    char *inner  = abre + 1;

    if (!*nombre) {
        add_error(p, lineno, "falta el nombre del procedimiento antes de '('");
        return;
    }

    // El modo de apertura va PEGADO al nombre, antes del parentesis. Se separa
    // aca, antes de validar el nombre: con el modo puesto, 'ABRIR E/' no es un
    // identificador, y el error hablaria de un nombre invalido cuando el nombre
    // esta perfecto.
    //
    // Sin barra pero con un espacio adentro tambien se manda ('ABRIR E(arch)'):
    // ahi el que falta es el modo A MEDIAS, y separar_modo es el unico que puede
    // decir que lo que falta es la barra. Se pide que la primera palabra sea un
    // procedimiento CON modos para no robarle el mensaje a los nombres que estan
    // mal por otro motivo.
    char modo[PAED_MODO_MAX] = {0};
    int  lleva_barra = strchr(nombre, '/') != NULL;
    int  modo_a_medias = 0;

    // Sin barra NI espacio: 'ABRIRs(arch)', la forma de los templates. Se
    // reconoce sacando letras por la derecha hasta dar con un procedimiento que
    // admita modo. Se exige que el nombre entero NO sea ya un procedimiento
    // valido, asi 'CERRAR(' nunca se lee como 'CERRA' + modo 'R'.
    int modo_pegado = 0;

    if (!lleva_barra) {
        char *esp = nombre;
        while (*esp && !isspace((unsigned char)*esp)) esp++;
        if (*esp) {
            char primera[PAED_NAME_MAX];
            snprintf(primera, sizeof(primera), "%.*s", (int)(esp - nombre), nombre);
            modo_a_medias = modos_def(proc_def(primera)) != NULL;
        } else if (!proc_def(nombre)) {
            char corto[PAED_NAME_MAX];
            snprintf(corto, sizeof(corto), "%s", nombre);
            for (size_t n = strlen(corto); n > 1; n--) {
                if (!isalpha((unsigned char)corto[n - 1])) break;
                corto[n - 1] = '\0';
                if (modos_def(proc_def(corto))) { modo_pegado = 1; break; }
            }
        }
    }

    if ((lleva_barra || modo_a_medias || modo_pegado) &&
        separar_modo(p, lineno, nombre, modo, sizeof(modo)) != 0)
        return;

    if (!es_identificador(nombre)) {
        add_error(p, lineno, "nombre de procedimiento invalido: '%s'", nombre);
        return;
    }

    // 3. Cierre de parentesis
    size_t ilen = strlen(inner);
    if (ilen == 0 || inner[ilen - 1] != ')') {
        add_error(p, lineno, "falta ')' en la llamada a %s", nombre);
        return;
    }
    inner[ilen - 1] = '\0';

    // 4. El procedimiento tiene que existir: o en sintaxis.json, o como
    //    subaccion declarada en este mismo programa.
    cJSON *def = proc_def(nombre);
    if (!def) {
        // No se pregunta ACA si la subaccion existe. Una subaccion puede llamar
        // a otra declarada mas abajo, y el parser lee el archivo de una sola
        // pasada: preguntarlo ahora daria "desconocido" por algo que aparece
        // diez renglones despues. Se anota la llamada y se verifica al final,
        // con el programa entero en la mano.
        if (p->instr_count >= PAED_MAX_INSTRS) {
            add_error(p, lineno, "demasiadas instrucciones (maximo %d)", PAED_MAX_INSTRS);
            return;
        }

        PAEDInstr *llam = &p->instrs[p->instr_count];
        memset(llam, 0, sizeof(*llam));
        llam->kind         = PAED_LLAMADA;
        llam->es_subaccion = 1;
        llam->salto        = -1;
        llam->line         = lineno;
        snprintf(llam->modo, sizeof(llam->modo), "%s", modo);
        strncpy(llam->proc, nombre, PAED_NAME_MAX - 1);

        // Los argumentos de una subaccion son POSICIONALES: el primero va al
        // primer parametro y asi. No son pares 'clave = valor' como los de los
        // procedimientos del lenguaje, porque los nombres de los parametros los
        // eligio el programador y no tienen por que ser publicos.
        char *partes[PAED_MAX_ARGS];
        int   n_partes = split_args(inner, partes, PAED_MAX_ARGS, p, lineno);
        int   malo     = 0;

        for (int i = 0; i < n_partes; i++) {
            char *a = trim(partes[i]);
            if (!*a) continue;
            if (llam->arg_count >= PAED_MAX_ARGS) {
                add_error(p, lineno, "demasiados argumentos en la llamada a '%s' (maximo %d)",
                          nombre, PAED_MAX_ARGS);
                malo = 1;
                break;
            }
            if (copiar_valor(p, lineno, llam->args[llam->arg_count].val, a, nombre) != 0) {
                malo = 1;
                continue;
            }
            llam->arg_count++;
        }

        if (!malo) p->instr_count++;
        return;
    }

    if (p->instr_count >= PAED_MAX_INSTRS) {
        add_error(p, lineno, "demasiadas instrucciones (maximo %d)", PAED_MAX_INSTRS);
        return;
    }

    // Se llena la ranura sin consumirla todavia: si algo falla mas abajo,
    // instr_count no avanza y la instruccion rota no queda en el programa.
    PAEDInstr *instr = &p->instrs[p->instr_count];
    memset(instr, 0, sizeof(*instr));
    instr->kind  = PAED_LLAMADA;
    instr->salto = -1;
    // Se guarda el nombre CANONICO, no el que escribio el usuario: si puso
    // 'Esc' o 'GRABAR', a partir de aca la instruccion dice 'ESCRIBIR'. El
    // interprete no conoce ni tiene que conocer las grafias de la catedra.
    strncpy(instr->proc, proc_canonico(def), PAED_NAME_MAX - 1);
    instr->line = lineno;
    snprintf(instr->modo, sizeof(instr->modo), "%s", modo);

    // 5. Argumentos
    char *partes[PAED_MAX_ARGS];
    int   n_partes = split_args(inner, partes, PAED_MAX_ARGS, p, lineno);
    int   variadico = proc_es_variadico(def);
    int   hubo_error = 0;

    // Si el procedimiento no declara parametros, sus argumentos son VALORES y
    // no pares 'clave = valor': ni se busca el separador. Un '=' ahi no separa
    // nada, y tampoco compara — se rechaza mas abajo.
    int clave_valor = proc_admite_clave_valor(def);

    for (int i = 0; i < n_partes; i++) {
        if (!clave_valor) {
            int largo = 0;
            const char *cmp = comparador_suelto(partes[i], &largo);
            if (cmp) {
                add_error(p, lineno,
                          "'%.*s' compara: una comparacion no va como argumento de %s, "
                          "va en la condicion de un SI o un MIENTRAS",
                          largo, cmp, nombre);
                hubo_error = 1;
                continue;
            }
        }

        char *igual = clave_valor ? igual_separador(partes[i]) : NULL;

        if (!igual) {
            // Nada se ignora en silencio: o es variadico, o es un error.
            if (variadico) {
                if (copiar_valor(p, lineno, instr->args[instr->arg_count].val,
                                 partes[i], nombre) != 0) {
                    hubo_error = 1;
                    continue;
                }
                instr->arg_count++;
                continue;
            }
            add_error(p, lineno,
                      "argumento '%s' sin 'clave = valor' en %s: la referencia siempre se escribe con nombre",
                      partes[i], nombre);
            hubo_error = 1;
            continue;
        }

        *igual = '\0';
        char *clave = trim(partes[i]);
        char *valor = trim(igual + 1);

        if (!*clave || !*valor) {
            add_error(p, lineno, "argumento incompleto en %s: se esperaba clave = valor", nombre);
            hubo_error = 1;
            continue;
        }

        cJSON *pd = param_def(def, clave);
        if (!pd && !variadico) {
            add_error(p, lineno, "parametro '%s' no existe en %s", clave, nombre);
            hubo_error = 1;
            continue;
        }

        if (pd) {
            cJSON *tipo = cJSON_GetObjectItem(pd, "tipo");
            if (cJSON_IsString(tipo) && !valida_valor(tipo->valuestring, valor)) {
                add_error(p, lineno, "'%s' no es un valor %s valido para %s de %s",
                          valor, tipo->valuestring, clave, nombre);
                hubo_error = 1;
                continue;
            }
            // Se guarda siempre con el nombre canonico, no con el alias.
            cJSON *canon = cJSON_GetObjectItem(pd, "nombre");
            if (cJSON_IsString(canon)) clave = canon->valuestring;
        }

        strncpy(instr->args[instr->arg_count].key, clave, PAED_KEY_MAX - 1);
        if (copiar_valor(p, lineno, instr->args[instr->arg_count].val, valor, nombre) != 0) {
            hubo_error = 1;
            continue;
        }
        instr->arg_count++;
    }

    // 6. Parametros obligatorios
    cJSON *params = cJSON_GetObjectItem(def, "params");
    cJSON *pp     = NULL;
    cJSON_ArrayForEach(pp, params) {
        if (!cJSON_IsTrue(cJSON_GetObjectItem(pp, "requerido"))) continue;
        cJSON *n = cJSON_GetObjectItem(pp, "nombre");
        if (!cJSON_IsString(n)) continue;
        if (!paed_get_arg(instr, n->valuestring)) {
            add_error(p, lineno, "falta el parametro obligatorio '%s' en %s",
                      n->valuestring, nombre);
            hubo_error = 1;
        }
    }

    if (resolver_forma(p, instr, def, lineno, nombre) != 0) hubo_error = 1;

    if (!hubo_error) p->instr_count++;
}

#include "paed/parser.h"
#include "cJSON.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "paed/plataforma.h"   // saber donde esta el binario, sin #ifdef aca

// ── Fuente unica de verdad: sintaxis.json ─────────────────────────────────────

static cJSON *g_syntax = NULL;   // el lenguaje: pseudocodigo AED puro
static cJSON *g_escena = NULL;   // libreria opcional de VimMon, no es el lenguaje

static cJSON *cargar_json(const char *path, int obligatorio) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (obligatorio) fprintf(stderr, "[paed] no se pudo abrir %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return NULL; }

    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t leidos = fread(buf, 1, (size_t)size, f);
    buf[leidos] = '\0';
    fclose(f);

    cJSON *raiz = cJSON_Parse(buf);
    free(buf);

    if (!raiz) fprintf(stderr, "[paed] %s tiene JSON invalido\n", path);
    return raiz;
}

// La definicion del lenguaje embebida en el binario. La genera el Makefile
// desde data/sintaxis.json, asi que no hay dos fuentes de verdad: hay
// una sola, y una copia que se rehace sola en cada build.
extern const char PAED_SINTAXIS_EMBEBIDA[];

// Donde `make install` deja los datos. Se puede pisar al compilar:
//   clang -DPAED_DATADIR='"/opt/paed/share"'
#ifndef PAED_DATADIR
#define PAED_DATADIR "/usr/local/share/paed"
#endif

// Nombre del archivo de la libreria extra que se cargo, para poder nombrarla en
// los mensajes de error. Vacio si no se cargo ninguna.
static char g_lib_nombre[64] = {0};

// Los datos AL LADO DEL BINARIO: <donde-esta-paed>/../share/paed
//
// Es lo que hace que el paquete se pueda descomprimir en cualquier lado y
// funcione. Sin esto, un binario compilado para /usr/local y descomprimido en
// ~/.local busca sus datos donde no estan, y falla con "no encuentro
// sintaxis.json" — que es exactamente lo que pasa si la ruta de instalacion se
// decide al COMPILAR y el usuario elige otra al INSTALAR.
//
// Como se pregunta "¿donde estoy?" lo resuelve plataforma.c, que es lo unico
// que cambia entre Linux y Windows.
//
// Deja el resultado en `out`. Devuelve 0 si pudo, -1 si no.
static int datadir_junto_al_binario(char *out, size_t n) {
    char exe[PAED_PATH_MAX];
    if (paed_ruta_ejecutable(exe, sizeof(exe)) != 0) return -1;

    if (paed_dirname(exe) != 0) return -1;   // .../bin/paed  ->  .../bin
    if (paed_dirname(exe) != 0) return -1;   // .../bin       ->  ...

    snprintf(out, n, "%s" PAED_SEP "share" PAED_SEP "paed", exe);
    return 0;
}

const char *paed_datadir(void) {
    static char elegido[PAED_PATH_MAX] = {0};
    if (elegido[0]) return elegido;

    char junto[PAED_PATH_MAX] = {0};
    datadir_junto_al_binario(junto, sizeof(junto));

    const char *home = getenv("PAED_HOME");
    const char *candidatos[] = {
        home ? home : "",
        junto,                // el paquete descomprimido donde sea
        PAED_DATADIR,
        "data",               // parado en el repo de PAED
        "paed/data",          // parado en VimMon, que lo tiene adentro
    };

    for (size_t i = 0; i < sizeof(candidatos) / sizeof(candidatos[0]); i++) {
        if (!candidatos[i][0]) continue;

        char prueba[PAED_PATH_MAX];
        snprintf(prueba, sizeof(prueba), "%s/%s", candidatos[i], PAED_SYNTAX_FILE);

        FILE *f = fopen(prueba, "r");
        if (!f) continue;
        fclose(f);

        snprintf(elegido, sizeof(elegido), "%s", candidatos[i]);
        return elegido;
    }

    return NULL;
}

int paed_syntax_load(void) {
    if (g_syntax) return 0;

    const char *dir = paed_datadir();

    if (dir) {
        char path[PAED_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir, PAED_SYNTAX_FILE);
        g_syntax = cargar_json(path, 1);
        if (g_syntax) return 0;
    }

    // Ultima red: la copia que viene ADENTRO del binario.
    //
    // El archivo del disco gana cuando existe, y eso es a proposito: se puede
    // tocar la definicion del lenguaje y probarla sin recompilar nada. Lo
    // embebido es para que un `paed` bajado suelto, sin nada al lado, igual
    // arranque — un interprete que no sabe que es una palabra clave no sirve
    // para nada, y "instalaste mal" no es un mensaje de error aceptable.
    g_syntax = cJSON_Parse(PAED_SINTAXIS_EMBEBIDA);
    if (!g_syntax) {
        fprintf(stderr, "[paed] la definicion del lenguaje embebida esta rota\n");
        return -1;
    }
    return 0;
}

const char *paed_sintaxis_embebida(void) {
    return PAED_SINTAXIS_EMBEBIDA;
}

int paed_syntax_load_lib(const char *nombre) {
    if (!nombre || !*nombre) return -1;
    if (paed_syntax_load() != 0) return -1;

    const char *dir = paed_datadir();
    if (!dir) return -1;

    char path[PAED_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s.json", dir, nombre);

    cJSON *lib = cargar_json(path, 1);
    if (!lib) return -1;

    if (g_escena) cJSON_Delete(g_escena);
    g_escena = lib;
    snprintf(g_lib_nombre, sizeof(g_lib_nombre), "%s.json", nombre);
    return 0;
}

void paed_syntax_free(void) {
    if (g_syntax) { cJSON_Delete(g_syntax); g_syntax = NULL; }
    if (g_escena) { cJSON_Delete(g_escena); g_escena = NULL; }
}

static cJSON *buscar_proc(cJSON *raiz, const char *nombre) {
    if (!raiz) return NULL;
    cJSON *p = NULL;
    cJSON_ArrayForEach(p, cJSON_GetObjectItem(raiz, "procedimientos")) {
        cJSON *n = cJSON_GetObjectItem(p, "nombre");
        // El nombre del procedimiento tampoco distingue mayusculas:
        // ESCRIBIR y escribir son el mismo.
        if (cJSON_IsString(n) && strcasecmp(n->valuestring, nombre) == 0) return p;
    }
    return NULL;
}

// Primero el lenguaje, despues las librerias cargadas.
static cJSON *proc_def(const char *nombre) {
    cJSON *p = buscar_proc(g_syntax, nombre);
    return p ? p : buscar_proc(g_escena, nombre);
}

// Busca un parametro de un procedimiento por nombre o por alias.
static cJSON *param_def(cJSON *proc, const char *clave) {
    cJSON *params = cJSON_GetObjectItem(proc, "params");
    cJSON *p      = NULL;
    cJSON_ArrayForEach(p, params) {
        cJSON *n = cJSON_GetObjectItem(p, "nombre");
        cJSON *a = cJSON_GetObjectItem(p, "alias");
        if (cJSON_IsString(n) && strcasecmp(n->valuestring, clave) == 0) return p;
        if (cJSON_IsString(a) && strcasecmp(a->valuestring, clave) == 0) return p;
    }
    return NULL;
}

static int proc_es_variadico(cJSON *proc) {
    return cJSON_IsTrue(cJSON_GetObjectItem(proc, "variadico"));
}

// Los modos de apertura que admite el procedimiento, o NULL si no admite
// ninguno. Es un objeto { "E": "entrada", ... }: la clave es el modo
// normalizado y el valor es como se lo nombra en los mensajes.
//
// Que ABRIR lleve modo y LEER no sale de sintaxis.json y no de una lista en C,
// por la misma razon que todo lo demas: la definicion del lenguaje vive en un
// solo lado.
static cJSON *modos_def(cJSON *proc) {
    if (!proc) return NULL;
    cJSON *m = cJSON_GetObjectItem(proc, "modos");
    return cJSON_IsObject(m) ? m : NULL;
}

// Como se ESCRIBE un modo normalizado: "E" -> "E/", "ES" -> "E/S".
// Solo para los mensajes: adentro el modo viaja sin barra.
static const char *modo_escrito(const char *modo, char *buf, size_t n) {
    if (strlen(modo) <= 1) snprintf(buf, n, "%s/", modo);
    else                   snprintf(buf, n, "%c/%s", modo[0], modo + 1);
    return buf;
}

// La lista de modos validos, ya escrita, para poder decirla en un error:
//   "E/ (entrada), S/ (salida), E/S (entrada-salida)"
static void modos_listados(cJSON *modos, char *out, size_t n) {
    out[0] = '\0';
    size_t usado = 0;

    cJSON *m = NULL;
    cJSON_ArrayForEach(m, modos) {
        if (!m->string) continue;
        char escrito[PAED_MODO_MAX * 2];
        modo_escrito(m->string, escrito, sizeof(escrito));

        int puesto = snprintf(out + usado, n - usado, "%s%s (%s)",
                              usado ? ", " : "", escrito,
                              cJSON_IsString(m) ? m->valuestring : m->string);
        if (puesto < 0 || (size_t)puesto >= n - usado) return;
        usado += (size_t)puesto;
    }
}

// ── Utilidades de texto ───────────────────────────────────────────────────────

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char *fin = s + strlen(s) - 1;
    while (fin > s && isspace((unsigned char)*fin)) *fin-- = '\0';
    return s;
}

// Corta el comentario // (respetando comillas) y el salto de linea.
static void strip_comment(char *s) {
    int en_texto = 0;
    for (char *c = s; *c; c++) {
        if (*c == '"') en_texto = !en_texto;
        if (!en_texto && c[0] == '/' && c[1] == '/') { *c = '\0'; return; }
    }
}

static int es_identificador(const char *s) {
    if (!*s || (!isalpha((unsigned char)*s) && *s != '_')) return 0;
    for (const char *c = s; *c; c++)
        if (!isalnum((unsigned char)*c) && *c != '_') return 0;
    return 1;
}

// Igual que es_identificador, pero acepta el punto de acceso a campo:
// 'pori.vx' es un nombre valido y 'pori' tambien.
//
// El punto tiene que estar ENTRE dos partes validas. Asi '.x', 'pori.' y
// 'pori..vx' se siguen rechazando, y un numero como 1.5 nunca entra aca porque
// no empieza con letra.
static int es_campo(const char *s) {
    if (!*s || (!isalpha((unsigned char)*s) && *s != '_')) return 0;

    for (const char *c = s; *c; c++) {
        if (*c == '.') {
            // ni al principio, ni al final, ni dos seguidos
            if (c == s || !(isalpha((unsigned char)c[1]) || c[1] == '_')) return 0;
            continue;
        }
        if (!isalnum((unsigned char)*c) && *c != '_') return 0;
    }
    return 1;
}

// ── Errores ───────────────────────────────────────────────────────────────────

static void add_error(PAEDProgram *p, int line, const char *fmt, ...) {
    if (p->error_count >= PAED_MAX_ERRORS) return;
    PAEDError *e = &p->errors[p->error_count++];
    e->line = line;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->msg, sizeof(e->msg), fmt, ap);
    va_end(ap);
}

void paed_print_errors(const PAEDProgram *prog) {
    for (int i = 0; i < prog->error_count; i++)
        fprintf(stderr, "%s:%d: error: %s\n",
                prog->path, prog->errors[i].line, prog->errors[i].msg);
    if (prog->error_count >= PAED_MAX_ERRORS)
        fprintf(stderr, "%s: error: demasiados errores, se corto el reporte\n", prog->path);
}

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
    return 1;  // tipo desconocido: no bloquea
}

// ── Parseo de una instruccion: PROC(clave = valor, ...); ──────────────────────

// Parte el interior de los parentesis en argumentos, cortando SOLO en las comas
// de nivel 0. Asi (0,2,5) sigue siendo un unico valor y no tres argumentos.
static int split_args(char *inner, char *out[], int max, PAEDProgram *p, int line) {
    int  n = 0, depth = 0, en_texto = 0;
    char *inicio = inner;

    for (char *c = inner; ; c++) {
        if (*c == '"') en_texto = !en_texto;
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

// Reserva la proxima instruccion ya inicializada. salto = -1 significa "esta no
// salta a ningun lado"; las de bloque lo completan cuando se cierra el bloque.
static PAEDInstr *nueva_instr(PAEDProgram *p, PAEDKind kind, int lineno) {
    if (p->instr_count >= PAED_MAX_INSTRS) {
        add_error(p, lineno, "demasiadas instrucciones (maximo %d)", PAED_MAX_INSTRS);
        return NULL;
    }
    PAEDInstr *in = &p->instrs[p->instr_count++];
    memset(in, 0, sizeof(*in));
    in->kind  = kind;
    in->line  = lineno;
    in->salto = -1;
    return in;
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

    if (barras != 1) {
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

static void parse_instruction(PAEDProgram *p, char *linea, int lineno) {
    // 1. Toda instruccion termina en ';'
    size_t len = strlen(linea);
    if (len == 0 || linea[len - 1] != ';') {
        add_error(p, lineno, "falta ';' al final de la instruccion");
        return;
    }
    linea[len - 1] = '\0';
    linea = trim(linea);

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

    if (!lleva_barra) {
        char *esp = nombre;
        while (*esp && !isspace((unsigned char)*esp)) esp++;
        if (*esp) {
            char primera[PAED_NAME_MAX];
            snprintf(primera, sizeof(primera), "%.*s", (int)(esp - nombre), nombre);
            modo_a_medias = modos_def(proc_def(primera)) != NULL;
        }
    }

    if ((lleva_barra || modo_a_medias) &&
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

    // 4. El procedimiento tiene que existir en sintaxis.json
    cJSON *def = proc_def(nombre);
    if (!def) {
        // Se nombran los ARCHIVOS y no sus rutas: la ruta depende de donde se
        // instalo PAED, y el mismo programa daria mensajes distintos en dos
        // maquinas. Donde estan se pregunta una sola vez, con paed_datadir().
        if (g_lib_nombre[0])
            add_error(p, lineno, "procedimiento desconocido '%s' (no esta ni en %s ni en %s)",
                      nombre, PAED_SYNTAX_FILE, g_lib_nombre);
        else
            add_error(p, lineno, "procedimiento desconocido '%s' (no esta en %s)",
                      nombre, PAED_SYNTAX_FILE);
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
    strncpy(instr->proc, nombre, PAED_NAME_MAX - 1);
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

static void por_cada_sentencia(PAEDProgram *p, char *linea, int lineno,
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

static void parse_sentencias(PAEDProgram *p, char *linea, int lineno) {
    por_cada_sentencia(p, linea, lineno, una_instruccion, NULL);
}

// ── La pila de bloques ────────────────────────────────────────────────────────
//
// Una variable sola no sirve para el anidamiento. En
//
//     MIENTRAS (a) HACER          MIENTRAS (a) HACER
//         MIENTRAS (b) HACER          x := 1;
//         FIN_MIENTRAS            FIN_MIENTRAS
//     FIN_MIENTRAS                MIENTRAS (b) HACER
//                                 FIN_MIENTRAS
//
// los dos programas tienen 2 MIENTRAS y 2 FIN_MIENTRAS: contar no los
// distingue. Hace falta saber QUIEN abrio cada uno.
//
// Y es una PILA y no otra cosa porque los bloques cierran en el orden inverso
// al que se abrieron: el ultimo que se abrio es el primero que se cierra. Eso
// es LIFO, y eso es exactamente una pila. Es lo mismo que hace un compilador
// de C con las llaves.

typedef struct {
    PAEDKind kind;   // PAED_SI o PAED_MIENTRAS
    int      line;   // donde se abrio, para poder decirlo en el error
    int      instr;  // que instruccion lo abrio, para parchearle el salto
    int      sino;   // indice del SINO si ya aparecio; -1 si no
} Abierto;

typedef struct {
    Abierto items[PAED_MAX_BLOQUES];
    int     tope;
} Pila;

static const char *nombre_kind(PAEDKind k) {
    switch (k) {
        case PAED_SI:       return "SI";
        case PAED_MIENTRAS: return "MIENTRAS";
        case PAED_PARA:     return "PARA";
        default:            return "?";
    }
}

// Busca una palabra clave SUELTA dentro de una linea. "Suelta" = sin letras ni
// digitos pegados a los costados, para que un identificador como 'hastaFin' no
// se confunda con la palabra HASTA.
static char *palabra_en(char *s, const char *kw) {
    size_t n = strlen(kw);
    // Sin distinguir mayusculas y no strstr: las palabras clave no distinguen
    // (ver kw_es, mas abajo), asi que 'PARA i := 1 hasta 5 HACER' tambien vale.
    for (char *c = s; (c = paed_strcasestr(c, kw)) != NULL; c += n) {
        int izq = (c == s) || (!isalnum((unsigned char)c[-1]) && c[-1] != '_');
        int der = !isalnum((unsigned char)c[n]) && c[n] != '_';
        if (izq && der) return c;
    }
    return NULL;
}

// ¿Esta linea ES exactamente esta palabra clave, sin mirar mayusculas?
//
// La wiki escribe `ARREGLO` (wiki.txt:1791) y `arreglo[` (:1798) para lo mismo,
// y el ejemplo de catedra (AED_2021_UnI.pdf:10) declara los tipos en minuscula.
// Obligar a una sola forma seria inventar una regla que las fuentes no tienen.
//
// Solo aplica a las PALABRAS CLAVE. Los identificadores conservan su
// capitalizacion y se siguen comparando con strcmp: 'total' y 'Total' son dos
// variables distintas.
static int kw_es(const char *linea, const char *kw) {
    return strcasecmp(linea, kw) == 0;
}

// Saca la palabra clave del principio y el terminador del final.
// "SI (a = 1) ENTONCES" -> "(a = 1)". NULL si falta el terminador.
static char *cuerpo_cabecera(char *linea, const char *kw, const char *fin) {
    char  *c  = trim(linea + strlen(kw));
    size_t lc = strlen(c), nf = strlen(fin);

    if (lc < nf || strcasecmp(c + lc - nf, fin) != 0) return NULL;
    // El terminador tiene que ser una palabra suelta, no el final de otra:
    // sin esto, un identificador que termine en HACER pasaria por terminador.
    if (lc > nf && !isspace((unsigned char)c[lc - nf - 1]) && c[lc - nf - 1] != ')')
        return NULL;

    c[lc - nf] = '\0';
    return trim(c);
}

// ¿La linea empieza con esta palabra clave como palabra entera?
static int empieza_con(const char *linea, const char *kw) {
    size_t n = strlen(kw);
    if (strncasecmp(linea, kw, n) != 0) return 0;
    return linea[n] == '\0' || isspace((unsigned char)linea[n]) || linea[n] == '(';
}

// Abre un bloque: crea la instruccion, guarda la condicion y apila.
static void abrir(PAEDProgram *p, Pila *pila, PAEDKind kind,
                  const char *cond, int lineno) {
    PAEDInstr *in = nueva_instr(p, kind, lineno);
    if (!in) return;
    strncpy(in->cond, cond, PAED_COND_MAX - 1);

    if (pila->tope >= PAED_MAX_BLOQUES) {
        add_error(p, lineno, "demasiados bloques anidados (maximo %d)", PAED_MAX_BLOQUES);
        return;
    }
    pila->items[pila->tope++] = (Abierto){ kind, lineno, p->instr_count - 1, -1 };
}

// Devuelve 1 si la linea era de bloque (y ya la trato), 0 si no lo era.
static int parse_bloque(PAEDProgram *p, char *linea, int lineno, Pila *pila) {

    // ── SI (condicion) ENTONCES ──
    if (empieza_con(linea, "SI")) {
        char *cond = cuerpo_cabecera(linea, "SI", "ENTONCES");
        if (!cond)  { add_error(p, lineno, "se esperaba: SI <condicion> ENTONCES"); return 1; }
        if (!*cond) { add_error(p, lineno, "el SI no tiene condicion"); return 1; }
        abrir(p, pila, PAED_SI, cond, lineno);
        return 1;
    }

    // ── MIENTRAS (condicion) HACER ──
    if (empieza_con(linea, "MIENTRAS")) {
        char *cond = cuerpo_cabecera(linea, "MIENTRAS", "HACER");
        if (!cond)  { add_error(p, lineno, "se esperaba: MIENTRAS <condicion> HACER"); return 1; }
        if (!*cond) { add_error(p, lineno, "el MIENTRAS no tiene condicion"); return 1; }
        abrir(p, pila, PAED_MIENTRAS, cond, lineno);
        return 1;
    }

    // ── PARA <var> := <desde> HASTA <hasta>[; <paso>] HACER ──
    //
    // La forma sale de TEORIA_COMPLETA.txt:565-571, que dice textual:
    //     "Si el incremento es distinto de 1, debe indicarse."
    //     PARA contador := inicialización hasta fin; incremento HACER
    // O sea que el paso es OPCIONAL y por defecto vale 1. Aca se separa del
    // for de Pascal, que no tiene clausula de incremento.
    //
    // Todo se guarda en args (desde/hasta/paso) para que el interprete lo lea
    // con paed_get_arg, igual que cualquier otro argumento.
    if (empieza_con(linea, "PARA")) {
        char *cuerpo = cuerpo_cabecera(linea, "PARA", "HACER");
        if (!cuerpo) {
            add_error(p, lineno,
                      "se esperaba: PARA <var> := <desde> HASTA <hasta> HACER"
                      " (con '; <paso>' antes de HACER si el incremento no es 1)");
            return 1;
        }

        char *asig = strstr(cuerpo, ":=");
        if (!asig) {
            add_error(p, lineno, "al PARA le falta ':=' con el valor inicial");
            return 1;
        }
        *asig = '\0';
        char *var   = trim(cuerpo);
        char *resto = trim(asig + 2);

        char *h = palabra_en(resto, "HASTA");
        if (!h) {
            add_error(p, lineno, "al PARA le falta HASTA con el valor final");
            return 1;
        }
        *h = '\0';
        char *desde = trim(resto);
        char *hasta = trim(h + 5);

        // El paso viene despues de un ';', y es opcional: sin el, vale 1.
        const char *paso = "1";
        char *puntoycoma = strchr(hasta, ';');
        if (puntoycoma) {
            *puntoycoma = '\0';
            char *pval = trim(puntoycoma + 1);
            if (!*pval) {
                add_error(p, lineno, "el PARA tiene ';' pero no dice el incremento");
                return 1;
            }
            paso  = pval;
            hasta = trim(hasta);
        }

        if (!es_identificador(var)) {
            add_error(p, lineno, "variable de PARA invalida: '%s'", var);
            return 1;
        }
        if (!*desde) { add_error(p, lineno, "al PARA le falta el valor inicial"); return 1; }
        if (!*hasta) { add_error(p, lineno, "al PARA le falta el valor final");   return 1; }

        PAEDInstr *in = nueva_instr(p, PAED_PARA, lineno);
        if (!in) return 1;
        strncpy(in->proc, var, PAED_NAME_MAX - 1);
        strncpy(in->args[0].key, "desde", PAED_KEY_MAX - 1);
        strncpy(in->args[0].val, desde,   PAED_VAL_MAX - 1);
        strncpy(in->args[1].key, "hasta", PAED_KEY_MAX - 1);
        strncpy(in->args[1].val, hasta,   PAED_VAL_MAX - 1);
        strncpy(in->args[2].key, "paso",  PAED_KEY_MAX - 1);
        strncpy(in->args[2].val, paso,    PAED_VAL_MAX - 1);
        in->arg_count = 3;

        if (pila->tope >= PAED_MAX_BLOQUES) {
            add_error(p, lineno, "demasiados bloques anidados (maximo %d)", PAED_MAX_BLOQUES);
            return 1;
        }
        pila->items[pila->tope++] = (Abierto){ PAED_PARA, lineno, p->instr_count - 1, -1 };
        return 1;
    }

    // ── SINO ──
    if (kw_es(linea, "SINO")) {
        if (pila->tope == 0 || pila->items[pila->tope - 1].kind != PAED_SI) {
            add_error(p, lineno, "SINO sin un SI abierto");
            return 1;
        }
        Abierto *a = &pila->items[pila->tope - 1];
        if (a->sino >= 0) {
            add_error(p, lineno, "SINO repetido: el SI de la linea %d ya tiene uno", a->line);
            return 1;
        }
        PAEDInstr *in = nueva_instr(p, PAED_SINO, lineno);
        if (!in) return 1;
        int idx = p->instr_count - 1;

        // Condicion falsa -> arrancar justo despues del SINO.
        p->instrs[a->instr].salto = idx + 1;
        a->sino = idx;
        return 1;
    }

    // ── FIN_SI ──
    if (kw_es(linea, "FIN_SI")) {
        if (pila->tope == 0) { add_error(p, lineno, "FIN_SI sin un SI abierto"); return 1; }
        Abierto *a = &pila->items[pila->tope - 1];
        if (a->kind != PAED_SI) {
            // Este es el mensaje que la pila hace posible: se puede decir QUE
            // bloque quedo abierto y EN QUE LINEA.
            add_error(p, lineno, "FIN_SI cierra un %s abierto en la linea %d",
                      nombre_kind(a->kind), a->line);
            return 1;
        }
        PAEDInstr *in = nueva_instr(p, PAED_FIN_SI, lineno);
        if (!in) return 1;
        int idx = p->instr_count - 1;

        // Con SINO, el que salta al final es el SINO (el SI ya apunta a el).
        // Sin SINO, es el propio SI el que se saltea el cuerpo.
        if (a->sino >= 0) p->instrs[a->sino].salto  = idx + 1;
        else              p->instrs[a->instr].salto = idx + 1;

        pila->tope--;
        return 1;
    }

    // ── FIN_MIENTRAS y FIN_PARA ──
    // Los dos cierran un bucle y hacen exactamente lo mismo con los saltos, asi
    // que comparten el cierre en vez de tener dos copias que se desincronicen.
    if (kw_es(linea, "FIN_MIENTRAS") || kw_es(linea, "FIN_PARA")) {
        // toupper: con 'fin_para' en minuscula, linea[4] es 'p' y sin esto
        // el bucle se cerraria como si fuera un FIN_MIENTRAS.
        int      es_para = toupper((unsigned char)linea[4]) == 'P';
        PAEDKind abre    = es_para ? PAED_PARA     : PAED_MIENTRAS;
        PAEDKind cierra  = es_para ? PAED_FIN_PARA : PAED_FIN_MIENTRAS;

        if (pila->tope == 0) {
            add_error(p, lineno, "%s sin un %s abierto", linea, nombre_kind(abre));
            return 1;
        }
        Abierto *a = &pila->items[pila->tope - 1];
        if (a->kind != abre) {
            add_error(p, lineno, "%s cierra un %s abierto en la linea %d",
                      linea, nombre_kind(a->kind), a->line);
            return 1;
        }
        PAEDInstr *in = nueva_instr(p, cierra, lineno);
        if (!in) return 1;
        int idx = p->instr_count - 1;

        in->salto                 = a->instr;  // volver al principio del bucle
        p->instrs[a->instr].salto = idx + 1;   // terminado -> salir del bucle

        pila->tope--;
        return 1;
    }

    return 0;   // no era una linea de bloque
}

// ── La organizacion de un archivo ─────────────────────────────────────────────
//
// Las organizaciones salen de sintaxis.json y NO de una lista en C, igual que
// todo lo demas: el asistente del editor tiene que ofrecer las mismas que el
// parser acepta, y con dos listas un dia dicen cosas distintas.
static cJSON *organizaciones(void) {
    cJSON *a = cJSON_GetObjectItem(g_syntax, "archivos");
    if (!cJSON_IsObject(a)) return NULL;
    cJSON *o = cJSON_GetObjectItem(a, "organizaciones");
    return cJSON_IsArray(o) ? o : NULL;
}

static const char *org_str(cJSON *org, const char *campo) {
    cJSON *v = cJSON_GetObjectItem(org, campo);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}

// ¿`resto` arranca con esta clausula? Devuelve lo que viene DESPUES, o NULL.
//
// La clausula son dos palabras ("ORDENADO POR") y entre ellas puede haber
// cualquier cantidad de espacios, asi que no alcanza con comparar la cadena
// entera: se van consumiendo palabra por palabra.
//
// Cada palabra tiene que terminar donde termina: sin ese recaudo 'ORDENADOS'
// entraria por 'ORDENADO', que es el mismo agujero que tapa el isspace de
// tipo_tras_DE con el 'DE' y 'DEUDA'.
static char *tras_clausula(char *resto, const char *clausula) {
    const char *c = clausula;

    while (*c) {
        while (isspace((unsigned char)*c)) c++;
        size_t n = 0;
        while (c[n] && !isspace((unsigned char)c[n])) n++;
        if (n == 0) break;

        while (isspace((unsigned char)*resto)) resto++;
        if (strncasecmp(resto, c, n) != 0) return NULL;
        if (resto[n] && !isspace((unsigned char)resto[n])) return NULL;

        resto += n;
        c     += n;
    }
    return trim(resto);
}

// Parte la clave en campos. El corpus separa con coma y ademas mete una 'y'
// antes del ultimo: "clave, tipo_novedad y f_novedad".
//
// La 'y' se toma como separador SOLO cuando es palabra suelta. Sin ese
// recaudo, un campo llamado 'hoy' o 'ley' se partiria al medio.
static int partir_clave(PAEDProgram *p, int lineno, char *lista,
                        PAEDDecl *d, const char *nombre) {
    char *c = lista;

    while (*c) {
        while (isspace((unsigned char)*c) || *c == ',') c++;
        if (!*c) break;

        // La 'y' suelta es separador, no un campo.
        if ((*c == 'y' || *c == 'Y') &&
            (isspace((unsigned char)c[1]) || c[1] == ',')) {
            c++;
            continue;
        }

        size_t n = 0;
        while (c[n] && c[n] != ',' && !isspace((unsigned char)c[n])) n++;

        if (d->clave_count >= PAED_MAX_CLAVE) {
            add_error(p, lineno,
                      "la clave de '%s' tiene demasiados campos (maximo %d)",
                      nombre, PAED_MAX_CLAVE);
            return -1;
        }

        char campo[PAED_NAME_MAX];
        snprintf(campo, sizeof(campo), "%.*s", (int)n, c);
        if (!es_identificador(campo)) {
            add_error(p, lineno, "'%s' no es un nombre de campo valido", campo);
            return -1;
        }
        snprintf(d->clave[d->clave_count++], PAED_NAME_MAX, "%s", campo);
        c += n;
    }

    if (d->clave_count == 0) {
        add_error(p, lineno, "a '%s' le faltan los campos de la clave", nombre);
        return -1;
    }
    return 0;
}

// Lee la clausula de organizacion que sigue al tipo, si la hay.
//
// `resto` es lo que quedo despues de 'ARCHIVO DE': el tipo y, opcionalmente,
// la clausula. Recorta `resto` dejando solo el tipo.
//
// Sin este corte el tipo se comeria el renglon entero y `d->type` terminaria
// valiendo "remedio ORDENADO POR farmacia" — un archivo de un tipo que no
// existe, y el error saldria mucho despues hablando de otra cosa.
static int separar_organizacion(PAEDProgram *p, int lineno, char *resto,
                                PAEDDecl *d, const char *nombre) {
    // El tipo es UNA palabra: termina en el primer espacio.
    char *esp = resto;
    while (*esp && !isspace((unsigned char)*esp)) esp++;
    if (!*esp) return 0;              // sin clausula: secuencial

    *esp = '\0';
    char *cola = trim(esp + 1);
    if (!*cola) return 0;

    cJSON *orgs = organizaciones(), *o = NULL;
    cJSON_ArrayForEach(o, orgs) {
        const char *clausula = org_str(o, "clausula");
        if (!clausula) continue;      // la secuencial no tiene clausula

        char *lista = tras_clausula(cola, clausula);
        if (!lista) continue;

        const char *org = org_str(o, "nombre");
        snprintf(d->org, sizeof(d->org), "%s", org ? org : "");

        if (!*lista) {
            add_error(p, lineno, "a '%s' le faltan los campos despues de '%s'",
                      nombre, clausula);
            return -1;
        }
        if (partir_clave(p, lineno, lista, d, nombre) != 0) return -1;

        // 'INDEXADO POR' lleva UN campo: el indice es una sola clave de
        // acceso, no una clave compuesta como la del ordenamiento.
        const char *campos = org_str(o, "campos");
        if (campos && strcmp(campos, "uno") == 0 && d->clave_count != 1) {
            add_error(p, lineno,
                      "'%s' lleva un solo campo y '%s' tiene %d",
                      clausula, nombre, d->clave_count);
            return -1;
        }
        return 0;
    }

    add_error(p, lineno,
              "no se entiende '%s' en la declaracion de '%s': despues del tipo "
              "solo va ORDENADO POR o INDEXADO POR", cola, nombre);
    return -1;
}

// ── Parseo de una declaracion del AMBIENTE: nombre : TIPO; ────────────────────

// Lee el `DE <tipo>` que llevan tanto ARREGLO como ARCHIVO y devuelve el tipo
// de adentro, o NULL si no hay un 'DE' suelto.
//
// Lo usan las dos ramas: son la misma sintaxis, y tenerla escrita dos veces es
// garantizar que un dia se arregle una y la otra no.
// Devuelve NULL si no hay 'DE', y una cadena VACIA si hay 'DE' pero nada
// despues. Son dos errores distintos y cada uno tiene su mensaje: decir "falta
// DE" cuando el DE esta escrito manda a mirar la parte que ya esta bien.
static char *tipo_tras_DE(char *resto) {
    if (strncasecmp(resto, "DE", 2) != 0) return NULL;

    // 'DE' con nada atras: el tipo es lo que falta, no el DE.
    if (resto[2] == '\0') return resto + 2;

    // 'DE' tiene que ser palabra suelta: sin esto, un tipo llamado 'DEUDA'
    // pasaria por 'DE' seguido de 'UDA'.
    if (!isspace((unsigned char)resto[2])) return NULL;
    return trim(resto + 2);
}

// Declara UNA variable, con el nombre y el tipo ya separados.
//
// `tipo` se recibe como buffer propio y modificable a proposito: las ramas de
// abajo lo trocean en el lugar (trim, tipo_tras_DE, separar_organizacion).
static void declarar_una(PAEDProgram *p, char *nombre, char *tipo, int lineno) {
    if (!es_identificador(nombre)) {
        add_error(p, lineno, "nombre de variable invalido: '%s'", nombre);
        return;
    }
    if (p->decl_count >= PAED_MAX_DECLS) {
        add_error(p, lineno, "demasiadas declaraciones (maximo %d)", PAED_MAX_DECLS);
        return;
    }

    PAEDDecl *d = &p->decls[p->decl_count++];
    memset(d, 0, sizeof(*d));
    strncpy(d->name, nombre, PAED_NAME_MAX - 1);
    d->line = lineno;

    // ARREGLO[desde..hasta] DE TIPO
    // Los limites se exigen constantes: en AED el tamaño de un arreglo se
    // conoce al declararlo, no depende de una variable que todavia no existe.
    if (strncasecmp(tipo, "ARREGLO", 7) == 0) {
        int   desde = 0, hasta = 0, leidos = 0;
        char  base[PAED_NAME_MAX] = {0};

        // Ya se sabe que empieza con ARREGLO: se saltan esas 7 letras y se lee
        // el rango. Los espacios del formato admiten "[1..10]" y "[ 1 .. 10 ]".
        if (sscanf(tipo + 7, " [ %d .. %d ]%n", &desde, &hasta, &leidos) != 2 || leidos == 0) {
            add_error(p, lineno,
                      "arreglo mal declarado en '%s': se esperaba "
                      "ARREGLO[desde..hasta] DE TIPO", nombre);
            p->decl_count--;
            return;
        }
        if (hasta < desde) {
            add_error(p, lineno, "el arreglo '%s' tiene los limites al reves: [%d..%d]",
                      nombre, desde, hasta);
            p->decl_count--;
            return;
        }

        char *base_p = tipo_tras_DE(trim(tipo + 7 + leidos));
        if (!base_p) {
            add_error(p, lineno, "al arreglo '%s' le falta 'DE <tipo>'", nombre);
            p->decl_count--;
            return;
        }
        snprintf(base, sizeof(base), "%s", base_p);
        if (!*base) {
            add_error(p, lineno, "al arreglo '%s' le falta el tipo despues de 'DE'", nombre);
            p->decl_count--;
            return;
        }

        d->es_arreglo = 1;
        d->desde      = desde;
        d->hasta      = hasta;
        strncpy(d->type, base, PAED_NAME_MAX - 1);
        return;
    }

    // ARCHIVO DE TIPO
    //
    // El isspace del final NO es decorativo: sin el, un tipo llamado
    // 'ARCHIVOS' entraria por esta rama. La rama de ARREGLO de arriba tiene
    // ese mismo agujero y quedo anotado en el KANBAN.
    if (strncasecmp(tipo, "ARCHIVO", 7) == 0 &&
        (tipo[7] == '\0' || isspace((unsigned char)tipo[7]))) {

        char *base_p = tipo_tras_DE(trim(tipo + 7));
        if (!base_p) {
            add_error(p, lineno, "al archivo '%s' le falta 'DE <tipo>'", nombre);
            p->decl_count--;
            return;
        }
        if (!*base_p) {
            add_error(p, lineno, "al archivo '%s' le falta el tipo despues de 'DE'", nombre);
            p->decl_count--;
            return;
        }

        // Corta `base_p` dejando solo el tipo y se queda con la organizacion.
        // Va ANTES de copiar el tipo, no despues: si no, el tipo ya se llevo
        // la clausula adentro.
        if (separar_organizacion(p, lineno, base_p, d, nombre) != 0) {
            p->decl_count--;
            return;
        }

        d->es_archivo = 1;
        strncpy(d->type, base_p, PAED_NAME_MAX - 1);
        return;
    }

    // SECUENCIA DE TIPO   /   SECUENCIA DE SALIDA
    //
    // El isspace del final es el mismo recaudo que en ARCHIVO: sin el, un tipo
    // llamado 'SECUENCIAL' entraria por esta rama.
    if (strncasecmp(tipo, "SECUENCIA", 9) == 0 &&
        (tipo[9] == '\0' || isspace((unsigned char)tipo[9]))) {

        char *base_p = tipo_tras_DE(trim(tipo + 9));
        if (!base_p) {
            add_error(p, lineno, "a la secuencia '%s' le falta 'DE <tipo>'", nombre);
            p->decl_count--;
            return;
        }
        if (!*base_p) {
            add_error(p, lineno, "a la secuencia '%s' le falta el tipo despues de 'DE'", nombre);
            p->decl_count--;
            return;
        }

        d->es_secuencia = 1;
        // SALIDA no es un tipo de dato: es la direccion en la que va la
        // secuencia. Por eso se guarda como bandera y no como `type`.
        d->es_salida = (strcasecmp(base_p, "SALIDA") == 0);
        strncpy(d->type, base_p, PAED_NAME_MAX - 1);
        return;
    }

    // VENTANA DE TIPO
    //
    // La ventana es la variable donde AVZ deja el elemento actual. En el
    // corpus se declara aparte (`vent: VENTANA DE CARACTER;`) pero se usa
    // como una variable comun — `avz(sec, vent)` la llena y `vent <> '#'` la
    // compara. Asi que se guarda como escalar del tipo de adentro, y no hace
    // falta ningun tratamiento especial en el interprete.
    if (strncasecmp(tipo, "VENTANA", 7) == 0 &&
        (tipo[7] == '\0' || isspace((unsigned char)tipo[7]))) {

        char *base_p = tipo_tras_DE(trim(tipo + 7));
        if (!base_p || !*base_p) {
            add_error(p, lineno, "a la ventana '%s' le falta 'DE <tipo>'", nombre);
            p->decl_count--;
            return;
        }
        strncpy(d->type, base_p, PAED_NAME_MAX - 1);
        return;
    }

    strncpy(d->type, tipo, PAED_NAME_MAX - 1);
}

// ── Una declaracion puede nombrar VARIAS variables ────────────────────────────
//
// `a, doble: entero;` declara las dos. Sale de la catedra, que lo escribe asi
// en el ejemplo canonico (TEORIA_COMPLETA.txt:440).
//
// wiki_paed.txt:149-150 lo tenia como pregunta abierta, contestada con un
// "Frankly dice NO". Frankly no decide: docs/PAED.md:1319 deja escrito que es
// permisivo y que correr ahi no hace correcto a un archivo — o sea que tampoco
// hace incorrecto a lo que rechaza. Manda la catedra, y la catedra lo usa.
//
// El corte es por coma y NO colapsa los vacios: `a,,b` y `a,b,` caen en
// es_identificador("") y dan error con el nombre vacio a la vista. Tragarse una
// coma de mas seria justo lo que este parser promete no hacer.
static void parse_decl(PAEDProgram *p, char *linea, int lineno) {
    size_t len = strlen(linea);
    if (len == 0 || linea[len - 1] != ';') {
        add_error(p, lineno, "falta ';' al final de la declaracion");
        return;
    }
    linea[len - 1] = '\0';

    char *dosp = strchr(linea, ':');
    if (!dosp) {
        add_error(p, lineno, "declaracion invalida: se esperaba nombre: TIPO;");
        return;
    }
    *dosp = '\0';
    char *nombres = trim(linea);
    char *tipo    = trim(dosp + 1);

    if (!*tipo) {
        add_error(p, lineno, "falta el tipo de '%s'", nombres);
        return;
    }

    // El tipo se COPIA para cada nombre: declarar_una lo trocea en el lugar, y
    // sin copia la segunda variable se encontraria el tipo ya comido.
    for (char *n = nombres;;) {
        char *coma = strchr(n, ',');
        if (coma) *coma = '\0';

        char copia[PAED_LINEA_MAX];
        snprintf(copia, sizeof(copia), "%s", tipo);
        declarar_una(p, trim(n), copia, lineno);

        if (!coma) break;
        n = coma + 1;
    }
}

// ── Analisis del archivo completo ─────────────────────────────────────────────

// Cierre de la ACCION. Se aceptan DOS formas, decididas el 2026-08-10:
//
//   FIN_ACCION   la de la wiki, y la que usan todos los .paed del repo
//   FINACCION    la misma sin el guion bajo
//
// Las dos son una sola palabra, asi que cuestan un solo strcmp y ningun
// lookahead. La forma de la catedra (`FIN ACCION`, con ESPACIO, en
// AED_2021_UnI.pdf pagina 10) queda AFUERA por decision del autor: partida en
// dos palabras obligaria a mirar la siguiente antes de decidir.
static int es_fin_accion(const char *linea) {
    return kw_es(linea, "FIN_ACCION") ||
           kw_es(linea, "FINACCION");
}

// ── El bloque AMBIENTE, que puede tener REGISTROS adentro ─────────────────────
//
//     AMBIENTE
//         vector2 = REGISTRO      <- abre un tipo
//             vx: REAL;           <- campo
//             vy: REAL;
//         FIN_REGISTRO            <- lo cierra
//         pori: vector2;          <- variable de ese tipo
//
// `reg` apunta al registro que se esta llenando, o es NULL si no hay ninguno
// abierto. El que llama lo mantiene entre lineas: es el sub-estado del bloque.
//
// Esta funcion ve UNA declaracion. Varias en el mismo renglon las parte
// por_cada_sentencia, que es el que llama — misma regla que en el PROCESO.
// ¿Esta linea abre un REGISTRO? `<nombre> = REGISTRO`, SIN tocar la linea.
//
// Hay una version que si la toca (parte el '=' para quedarse con el nombre) mas
// abajo, en la rama que lo abre de verdad. Esta existe para poder PREGUNTAR
// antes de decidir: mientras hay un registro abierto, la respuesta cambia el
// error que corresponde.
static int abre_registro(const char *linea) {
    const char *ig = strchr(linea, '=');
    if (!ig) return 0;

    const char *r = ig + 1;
    while (*r && isspace((unsigned char)*r)) r++;

    size_t n = strlen(r);
    while (n > 0 && isspace((unsigned char)r[n - 1])) n--;

    return n == 8 && strncasecmp(r, "REGISTRO", 8) == 0;
}

static void parse_ambiente_una(PAEDProgram *p, char *linea, int lineno, PAEDRegistro **reg) {
    // ── Dentro de un REGISTRO ──
    if (*reg) {
        if (kw_es(linea, "FIN_REGISTRO") || kw_es(linea, "FINREGISTRO")) {
            if ((*reg)->campo_count == 0)
                add_error(p, lineno, "el registro '%s' no tiene ningun campo", (*reg)->name);
            *reg = NULL;
            return;
        }

        // Otro REGISTRO que arranca con uno todavia abierto. Lo que falta es el
        // FIN_REGISTRO del anterior, y hay que decir ESO.
        //
        // Sin este caso, `alumno = REGISTRO` entraba como un CAMPO del registro
        // de arriba, y el error era "falta ';' al final de la declaracion" en
        // ESTA linea — mandando a mirar la unica linea que estaba bien, mientras
        // el problema real quedaba varios renglones mas arriba.
        if (abre_registro(linea)) {
            add_error(p, lineno,
                      "falta FIN_REGISTRO: el registro '%s' de la linea %d sigue "
                      "abierto y aca ya empieza otro", (*reg)->name, (*reg)->line);
            // Se lo cierra igual y la linea sigue viaje a la rama que abre el
            // nuevo, mas abajo. Dejarlo abierto haria que todos los campos del
            // segundo registro cayeran en el primero, y cada uno sumaria su
            // propio error por algo que ya se reporto una vez.
            *reg = NULL;
        }
    }

    // ── Un campo del REGISTRO que sigue abierto ──
    if (*reg) {
        if ((*reg)->campo_count >= PAED_MAX_CAMPOS) {
            add_error(p, lineno, "el registro '%s' tiene demasiados campos (maximo %d)",
                      (*reg)->name, PAED_MAX_CAMPOS);
            return;
        }

        // Un campo se declara igual que una variable, asi que se reusa el mismo
        // parseo en vez de tener dos copias que se desincronicen.
        PAEDDecl *destino = &(*reg)->campos[(*reg)->campo_count];
        int antes = p->decl_count;
        parse_decl(p, linea, lineno);

        if (p->decl_count > antes) {
            // parse_decl lo dejo en la tabla general: se mueve al registro,
            // porque un campo no es una variable del programa.
            *destino = p->decls[--p->decl_count];

            // Un registro vive en MEMORIA: es el molde de lo que se lee o se
            // graba. Un archivo adentro no tiene sentido, y aceptarlo callado
            // dejaria pasar una declaracion que despues explota sin motivo
            // visible.
            if (destino->es_archivo) {
                add_error(p, lineno,
                          "un campo de registro no puede ser un archivo: '%s' "
                          "(un registro vive en memoria)", destino->name);
                return;
            }
            (*reg)->campo_count++;
        }
        return;
    }

    // ── ¿Abre un REGISTRO? `<nombre> = REGISTRO` ──
    // Se mira el '=' antes que nada: una declaracion normal lleva ':' y esta no.
    if (abre_registro(linea)) {
        *strchr(linea, '=') = '\0';   // abre_registro ya garantizo que esta
        char *nombre = trim(linea);

        if (!es_identificador(nombre)) {
            add_error(p, lineno, "nombre de registro invalido: '%s'", nombre);
            return;
        }
        if (p->registro_count >= PAED_MAX_REGISTROS) {
            add_error(p, lineno, "demasiados registros (maximo %d)", PAED_MAX_REGISTROS);
            return;
        }
        for (int i = 0; i < p->registro_count; i++)
            if (kw_es(p->registros[i].name, nombre)) {
                add_error(p, lineno, "el registro '%s' ya se declaro en la linea %d",
                          nombre, p->registros[i].line);
                return;
            }

        PAEDRegistro *nuevo = &p->registros[p->registro_count++];
        memset(nuevo, 0, sizeof(*nuevo));
        strncpy(nuevo->name, nombre, PAED_NAME_MAX - 1);
        nuevo->line = lineno;
        *reg = nuevo;
        return;
    }

    // ── Declaracion normal de variable ──
    parse_decl(p, linea, lineno);
}

static void una_declaracion(PAEDProgram *p, char *texto, int lineno, void *ctx) {
    parse_ambiente_una(p, texto, lineno, (PAEDRegistro **)ctx);
}

// El AMBIENTE, renglon completo. Puede traer mas de una declaracion:
//
//     a: ENTERO; b: ENTERO; c: ENTERO;
//
// El REGISTRO que este abierto viaja como contexto, porque el corte no cambia
// quien es el destino de cada campo: sigue siendo el mismo registro para todas
// las declaraciones del renglon.
static void parse_ambiente(PAEDProgram *p, char *linea, int lineno, PAEDRegistro **reg) {
    por_cada_sentencia(p, linea, lineno, una_declaracion, reg);
}

// ── ¿Se puede leer y grabar en lo que el ABRIR dejo abierto? ──────────────────
//
// El modo no es un adorno: `ABRIR E/(arch)` dice que ese archivo es de SOLO
// LECTURA (TEMAS_7-10_Registros_Archivos.md:132). Grabar ahi es exactamente el
// error que el modo existe para evitar, y se puede ver SIN EJECUTAR NADA:
// alcanza con juntar los ABRIR de cada archivo y mirar que se hace con el
// despues.
//
// Solo se miran los archivos que ESCRIBIERON su modo. Sin modo no hay nada que
// corroborar, y suponerle una direccion seria inventar media declaracion.
//
// Los modos de todos los ABRIR del mismo archivo se SUMAN: un archivo que se
// abre E/, se cierra y se vuelve a abrir S/ se usa de las dos formas, y cada una
// en su momento es correcta. Aca no hay orden ni flujo — eso lo sabe el
// interprete, no el parser — asi que la duda juega a favor del programa.
static const char *direccion(const char *letras) {
    if (strcmp(letras, "E") == 0) return "ENTRADA";
    if (strcmp(letras, "S") == 0) return "SALIDA";
    return "ENTRADA-SALIDA";
}

static void chequear_modos(PAEDProgram *p) {
    struct {
        const char *arch;
        char        letras[PAED_MODO_MAX];   // union de los modos de sus ABRIR
        int         line;                    // el primer ABRIR que dijo el modo
    } abiertos[PAED_MAX_DECLS];
    int n = 0;

    for (int i = 0; i < p->instr_count; i++) {
        const PAEDInstr *in = &p->instrs[i];
        if (in->kind != PAED_LLAMADA || in->forma != PAED_FORMA_ARCHIVO) continue;

        // CREAR tambien ABRE el archivo, y lo abre para SALIDA — el archivo
        // nace vacio, no hay nada que leerle.
        //
        // Sin este caso, un programa que CREA un archivo, lo carga, lo cierra y
        // despues lo vuelve a abrir con ABRIR E/ para recorrerlo daba error en
        // todos sus ESCRIBIR: el chequeo veia un solo ABRIR, el de lectura, y
        // creia que el archivo nunca se habia abierto para grabar. Es el patron
        // mas comun de todos, y estaba rechazado.
        const char *letras = NULL;
        if (strcasecmp(in->proc, "CREAR") == 0)      letras = "S";
        else if (strcasecmp(in->proc, "ABRIR") == 0) letras = in->modo;
        if (!letras || !letras[0]) continue;

        const char *arch = paed_get_arg(in, "archivo");
        if (!arch) continue;

        int k = 0;
        while (k < n && strcmp(abiertos[k].arch, arch) != 0) k++;
        if (k == n) {
            if (n >= PAED_MAX_DECLS) break;
            abiertos[n].arch      = arch;
            abiertos[n].letras[0] = '\0';
            abiertos[n].line      = in->line;
            n++;
        }

        for (const char *c = letras; *c; c++) {
            if (strchr(abiertos[k].letras, *c)) continue;
            size_t largo = strlen(abiertos[k].letras);
            if (largo + 1 >= sizeof(abiertos[k].letras)) break;
            abiertos[k].letras[largo]     = *c;
            abiertos[k].letras[largo + 1] = '\0';
        }
    }

    if (n == 0) return;

    for (int i = 0; i < p->instr_count; i++) {
        const PAEDInstr *in = &p->instrs[i];
        if (in->kind != PAED_LLAMADA || in->forma != PAED_FORMA_ARCHIVO) continue;

        int lee   = strcasecmp(in->proc, "LEER")     == 0;
        int graba = strcasecmp(in->proc, "ESCRIBIR") == 0;
        if (!lee && !graba) continue;

        const char *arch = paed_get_arg(in, "archivo");
        if (!arch) continue;

        int k = 0;
        while (k < n && strcmp(abiertos[k].arch, arch) != 0) k++;
        if (k == n) continue;   // se abrio sin modo: no hay nada que decir

        char necesita = lee ? 'E' : 'S';
        if (strchr(abiertos[k].letras, necesita)) continue;

        add_error(p, in->line,
                  "el archivo '%s' se abrio para %s en la linea %d: %s necesita que "
                  "este abierto para %s (ABRIR %c/ o E/S)",
                  arch, direccion(abiertos[k].letras), abiertos[k].line, in->proc,
                  lee ? "ENTRADA" : "SALIDA", necesita);
    }
}

// Los campos de la clave de un archivo tienen que existir en su REGISTRO.
//
// Va en una PASADA APARTE, cuando el AMBIENTE ya se leyo entero, y no adentro
// de parse_decl: el archivo puede estar declarado ANTES que el registro, y en
// ese momento el registro todavia no existe. Validar ahi daria "campo
// inexistente" por el solo hecho de haber escrito las declaraciones en otro
// orden.
//
// Sin esta validacion la clausula seria decorativa: se aceptaria cualquier
// nombre inventado, y el sintoma recien aparece mucho despues, como datos
// desordenados en la salida — que no se parece en nada a la causa.
static void chequear_claves(PAEDProgram *p) {
    for (int i = 0; i < p->decl_count; i++) {
        PAEDDecl *d = &p->decls[i];
        if (!d->es_archivo || d->clave_count == 0) continue;

        const PAEDRegistro *reg = NULL;
        for (int r = 0; r < p->registro_count; r++)
            if (kw_es(p->registros[r].name, d->type)) { reg = &p->registros[r]; break; }

        if (!reg) {
            add_error(p, d->line,
                      "'%s' no se puede ordenar por campos: '%s' no es un REGISTRO "
                      "declarado en el AMBIENTE", d->name, d->type);
            continue;
        }

        for (int k = 0; k < d->clave_count; k++) {
            int existe = 0;
            for (int c = 0; c < reg->campo_count && !existe; c++)
                if (kw_es(reg->campos[c].name, d->clave[k])) existe = 1;

            if (!existe) {
                add_error(p, d->line, "el registro '%s' no tiene un campo '%s'",
                          reg->name, d->clave[k]);
                continue;
            }
            // Repetir un campo en la clave no ordena por nada nuevo: el
            // segundo desempata lo que el primero ya dejo igual.
            for (int j = 0; j < k; j++)
                if (kw_es(d->clave[j], d->clave[k]))
                    add_error(p, d->line,
                              "el campo '%s' esta dos veces en la clave de '%s'",
                              d->clave[k], d->name);
        }
    }
}

typedef enum { FUERA, CABECERA, AMBIENTE, PROCESO, CERRADO } Bloque;

int paed_parse_file(const char *path, PAEDProgram *out) {
    memset(out, 0, sizeof(*out));
    strncpy(out->path, path, PAED_PATH_MAX - 1);

    if (paed_syntax_load() != 0) {
        add_error(out, 0, "no se pudo cargar la definicion del lenguaje (%s)", PAED_SYNTAX_FILE);
        return -1;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        add_error(out, 0, "no se pudo abrir el archivo");
        return -1;
    }

    char   buf[PAED_LINEA_MAX];
    int    lineno = 0;
    Bloque bloque = FUERA;
    Pila   pila   = { .tope = 0 };   // bloques abiertos dentro del PROCESO

    // Sub-estado del AMBIENTE: mientras hay un REGISTRO abierto, cada linea es
    // un CAMPO del tipo y no una declaracion de variable. NULL = no hay ninguno.
    PAEDRegistro *reg = NULL;

    while (fgets(buf, sizeof(buf), f)) {
        lineno++;
        strip_comment(buf);
        char *linea = trim(buf);
        if (!*linea) continue;

        if (strncasecmp(linea, "ACCION", 6) == 0 && (linea[6] == ' ' || linea[6] == '\t')) {
            if (bloque != FUERA) {
                add_error(out, lineno, "ACCION anidada: un archivo .paed tiene una sola ACCION");
                continue;
            }
            char nombre[PAED_NAME_MAX] = {0}, es[8] = {0};
            // El literal "ACCION" dentro de sscanf tambien distingue
            // mayusculas. Ya se comprobo arriba con strncasecmp, asi que se
            // saltean esas 6 letras y se lee desde el nombre.
            if (sscanf(linea + 6, "%63s %7s", nombre, es) != 2 || !kw_es(es, "ES")) {
                add_error(out, lineno, "se esperaba: ACCION <nombre> ES");
                continue;
            }
            strncpy(out->name, nombre, PAED_NAME_MAX - 1);
            bloque = CABECERA;
            continue;
        }

        if (kw_es(linea, "AMBIENTE")) {
            if (bloque != CABECERA)
                add_error(out, lineno, "AMBIENTE va justo despues de ACCION ... ES y antes de PROCESO");
            bloque = AMBIENTE;
            continue;
        }

        if (kw_es(linea, "PROCESO")) {
            if (bloque != CABECERA && bloque != AMBIENTE)
                add_error(out, lineno, "PROCESO fuera de lugar");
            // Un REGISTRO no puede quedar abierto cruzando al PROCESO. Se dice
            // ACA, en la linea donde se nota, y no al final del archivo: el
            // ultimo renglon del .paed no tiene nada que ver con el problema.
            if (reg) {
                add_error(out, lineno,
                          "falta FIN_REGISTRO: el registro '%s' de la linea %d quedo "
                          "abierto y aca ya empieza el PROCESO", reg->name, reg->line);
                reg = NULL;
            }
            bloque = PROCESO;
            continue;
        }

        // 'FACCION' se entiende, pero abreviar 'FIN' a 'F' deja el cierre
        // incompleto y se rechaza. Lleva mensaje propio porque sin este caso
        // caeria como instruccion suelta y el error seria "falta ';'", que no
        // ayuda a nadie. Igual se CIERRA el bloque: se sabe que quiso cerrar,
        // y seguir con el PROCESO abierto haria cascar errores en todas las
        // lineas que vengan despues.
        if (kw_es(linea, "FACCION")) {
            add_error(out, lineno,
                      "'FACCION' esta incompleto: el cierre se escribe "
                      "FIN_ACCION o FINACCION");
        }

        // 'FIN ACCION' partido en dos es la forma de la catedra
        // (AED_2021_UnI.pdf:10). No se acepta, pero se reconoce para poder
        // decirlo: es la forma que uno copia del apunte, y sin este caso el
        // error seria "falta ';'", que manda a buscar el problema al lugar
        // equivocado.
        if (kw_es(linea, "FIN ACCION")) {
            add_error(out, lineno,
                      "el apunte escribe 'FIN ACCION' con espacio, pero en PAED "
                      "el cierre es una sola palabra: FIN_ACCION o FINACCION");
        }

        // Las formas rechazadas igual CIERRAN el bloque: ya se reporto el
        // motivo, y seguir con el PROCESO abierto haria cascar un error mas en
        // cada linea que venga despues.
        if (es_fin_accion(linea) ||
            kw_es(linea, "FACCION") ||
            kw_es(linea, "FIN ACCION")) {
            if (bloque != PROCESO)
                add_error(out, lineno, "el cierre de ACCION no tiene un bloque PROCESO abierto");
            // FIN_ACCION no puede cerrar bloques que quedaron abiertos: se avisa
            // aca, con la linea de apertura, y no cuando ya no se sabe nada.
            for (int i = pila.tope - 1; i >= 0; i--)
                add_error(out, lineno, "falta FIN_%s: el %s de la linea %d quedo abierto",
                          nombre_kind(pila.items[i].kind),
                          nombre_kind(pila.items[i].kind), pila.items[i].line);
            pila.tope = 0;
            bloque = CERRADO;
            continue;
        }

        switch (bloque) {
            case AMBIENTE: parse_ambiente(out, linea, lineno, &reg); break;
            case PROCESO:
                // Primero los bloques: sus cabeceras NO llevan ';', asi que
                // tienen que reconocerse antes de que parse_instruction lo exija.
                if (!parse_bloque(out, linea, lineno, &pila))
                    parse_sentencias(out, linea, lineno);
                break;
            case FUERA:
                add_error(out, lineno, "instruccion antes de ACCION");
                break;
            case CABECERA:
                add_error(out, lineno, "instruccion fuera de AMBIENTE y de PROCESO");
                break;
            case CERRADO:
                add_error(out, lineno, "instruccion despues de FIN_ACCION");
                break;
        }
    }

    fclose(f);

    // Va con el programa completo en la mano: el ABRIR que decide si un archivo
    // se puede grabar puede estar despues del ESCRIBIR que lo usa, y linea por
    // linea eso no se puede saber.
    chequear_claves(out);
    chequear_modos(out);

    if (bloque == FUERA)  add_error(out, lineno, "falta ACCION <nombre> ES");
    if (bloque == CABECERA || bloque == AMBIENTE) add_error(out, lineno, "falta PROCESO");
    if (reg) add_error(out, lineno, "falta FIN_REGISTRO: el registro '%s' de la linea %d quedo abierto",
                       reg->name, reg->line);
    if (bloque == PROCESO) add_error(out, lineno, "falta el cierre: FIN_ACCION o FINACCION");

    return out->error_count == 0 ? 0 : -1;
}

const char *paed_get_arg(const PAEDInstr *instr, const char *key) {
    for (int i = 0; i < instr->arg_count; i++)
        // El parser guarda la clave TAL CUAL la escribio el usuario, y el
        // interprete la pide en minuscula. Sin esto, NOMBRE = x no se
        // encontraria y el procedimiento diria que le falta el parametro.
        if (strcasecmp(instr->args[i].key, key) == 0)
            return instr->args[i].val;
    return NULL;
}

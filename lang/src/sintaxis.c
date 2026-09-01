// El lenguaje: sintaxis.json, y todo lo que se le pregunta.
//
// Este archivo tiene UNA responsabilidad: cargar la definicion del lenguaje y
// contestar preguntas sobre ella. Es el unico que toca g_syntax y las
// librerias, y por eso es el unico que sabe como esta armado el JSON por
// dentro. El resto del parser le pregunta y no mira adentro.
//
// Salio de parser.c, que tenia 3558 lineas y diecinueve temas mezclados. Se
// eligio este bloque para empezar porque es el que tiene el ESTADO: mientras
// g_syntax viviera junto al parseo, cualquier otro corte iba a arrastrarlo.
//
// Lo publico de verdad esta en paed/parser.h (paed_syntax_load y compania). Lo
// que solo necesita el parser esta en sintaxis.h, al lado de este archivo y no
// en include/paed/: parser.h NO expone cJSON a proposito, y estas funciones
// devuelven cJSON*.

#include "paed/parser.h"
#include "sintaxis.h"
#include "cJSON.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "paed/plataforma.h"   // saber donde esta el binario, sin #ifdef aca

// ── Fuente unica de verdad: sintaxis.json ─────────────────────────────────────

static cJSON *g_syntax = NULL;   // el lenguaje: pseudocodigo AED puro
// Librerias cargadas con USAR. VARIAS a la vez y no una sola: la idea es que
// cada una traiga un tema (el mundo, el actor, el suelo) y el programa arme el
// suyo combinandolas, como cualquier lenguaje con modulos.
#define PAED_MAX_LIBS 8
static cJSON *g_libs[PAED_MAX_LIBS];
static char   g_libs_nombre[PAED_MAX_LIBS][64];
static int    g_lib_count = 0;

// El estado de este modulo —g_syntax y las librerias— no sale de aca. Estas
// dos funciones son toda su superficie hacia el parser: si manana el JSON
// cambia de forma, el unico archivo que se toca es este.

// Una seccion de primer nivel de sintaxis.json, o NULL si no esta.
cJSON *syn_seccion(const char *nombre) {
    return cJSON_GetObjectItem(g_syntax, nombre);
}

// Los nombres de las librerias cargadas, separados por coma, para poder
// nombrarlas en un mensaje de error. Devuelve cuantas hay.
int syn_libs_nombres(char *out, size_t n) {
    if (n == 0) return g_lib_count;
    out[0] = '\0';

    size_t usado = 0;
    for (int k = 0; k < g_lib_count && usado < n - 1; k++)
        usado += (size_t)snprintf(out + usado, n - usado,
                                  "%s%s", k ? ", " : "", g_libs_nombre[k]);
    return g_lib_count;
}

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

    // Cargar dos veces la misma libreria no es un error: dos modulos pueden
    // pedir la misma y ninguno tiene por que saber del otro.
    for (int i = 0; i < g_lib_count; i++) {
        if (strcmp(g_libs_nombre[i], nombre) == 0) {
            cJSON_Delete(lib);
            return 0;
        }
    }

    if (g_lib_count >= PAED_MAX_LIBS) {
        cJSON_Delete(lib);
        fprintf(stderr, "[paed] no entran mas librerias (maximo %d)\n", PAED_MAX_LIBS);
        return -1;
    }

    g_libs[g_lib_count] = lib;
    snprintf(g_libs_nombre[g_lib_count], sizeof(g_libs_nombre[0]), "%s", nombre);
    g_lib_count++;
    return 0;
}

void paed_syntax_free(void) {
    if (g_syntax) { cJSON_Delete(g_syntax); g_syntax = NULL; }
    for (int i = 0; i < g_lib_count; i++) {
        cJSON_Delete(g_libs[i]);
        g_libs[i] = NULL;
        g_libs_nombre[i][0] = '\0';
    }
    g_lib_count = 0;
}

// ── Consultar las categorias de sintaxis.json ────────────────────────────────
//
// El parser no necesita esto para parsear: sabe que 'MIENTRAS' abre un bucle
// porque lo compara donde corresponde. Lo necesita el RESALTADOR, que hace la
// pregunta al reves — "esta palabra suelta, ¿que es?" — y no puede tener su
// propia lista de keywords, porque esa fue exactamente la desincronizacion que
// mato a la version anterior (ver docs/LECCIONES.md).
//
// Vive aca, y no en colores.c, por un motivo simple: `g_syntax` es static de
// este archivo. Sacarlo afuera para que otro lo lea seria abrir la puerta a que
// cualquiera modifique la definicion del lenguaje.

// Devuelve el nombre de la categoria a la que pertenece la palabra
// ('estructura', 'tipos', 'bucles'...), o NULL si no es palabra del lenguaje.
// No distingue mayusculas: en PAED 'MIENTRAS' y 'mientras' son la misma.
const char *paed_categoria_de_palabra(const char *palabra) {
    if (!palabra || !*palabra || paed_syntax_load() != 0) return NULL;

    cJSON *cat = NULL;
    cJSON_ArrayForEach(cat, cJSON_GetObjectItem(g_syntax, "categorias")) {
        cJSON *nombre = cJSON_GetObjectItem(cat, "nombre");
        if (!cJSON_IsString(nombre)) continue;

        cJSON *p = NULL;
        cJSON_ArrayForEach(p, cJSON_GetObjectItem(cat, "palabras")) {
            if (cJSON_IsString(p) && strcasecmp(p->valuestring, palabra) == 0)
                return nombre->valuestring;
        }
    }
    return NULL;
}

// Igual que la de arriba, pero para los SIMBOLOS: busca la palabra mas LARGA de
// sintaxis.json que sea prefijo de `s` y la devuelve por `largo`.
//
// El largo importa: ':=' y ':' arrancan igual, y '<=' y '<' tambien. Si ganara
// la corta, el resaltado partiria el operador al medio y pintaria ':' de un
// color y '=' de otro. Es el mismo problema que 'FIN_SI' contra 'SI'.
const char *paed_categoria_de_simbolo(const char *s, int *largo) {
    if (largo) *largo = 0;
    if (!s || !*s || paed_syntax_load() != 0) return NULL;

    const char *mejor_cat = NULL;
    size_t      mejor_len = 0;

    cJSON *cat = NULL;
    cJSON_ArrayForEach(cat, cJSON_GetObjectItem(g_syntax, "categorias")) {
        cJSON *nombre = cJSON_GetObjectItem(cat, "nombre");
        if (!cJSON_IsString(nombre)) continue;

        cJSON *p = NULL;
        cJSON_ArrayForEach(p, cJSON_GetObjectItem(cat, "palabras")) {
            if (!cJSON_IsString(p)) continue;
            // Solo simbolos: las palabras con letras las resuelve la funcion de
            // arriba, que ademas exige que la palabra este entera.
            if (isalpha((unsigned char)p->valuestring[0]) || p->valuestring[0] == '_')
                continue;

            size_t len = strlen(p->valuestring);
            if (len > mejor_len && strncmp(s, p->valuestring, len) == 0) {
                mejor_len = len;
                mejor_cat = nombre->valuestring;
            }
        }
    }

    if (largo) *largo = (int)mejor_len;
    return mejor_cat;
}

// El color que sintaxis.json le puso a una categoria ('azul', 'gris'...), o
// NULL. Es un NOMBRE, no un codigo: como se dibuja ese nombre lo decide quien
// pinta — la terminal con ANSI, o editorBim con lo que use.
const char *paed_categoria_color(const char *categoria) {
    if (!categoria || paed_syntax_load() != 0) return NULL;

    cJSON *cat = NULL;
    cJSON_ArrayForEach(cat, cJSON_GetObjectItem(g_syntax, "categorias")) {
        cJSON *nombre = cJSON_GetObjectItem(cat, "nombre");
        if (cJSON_IsString(nombre) && strcmp(nombre->valuestring, categoria) == 0) {
            cJSON *color = cJSON_GetObjectItem(cat, "color");
            return cJSON_IsString(color) ? color->valuestring : NULL;
        }
    }
    return NULL;
}

// Las organizaciones de archivo, tal como las define sintaxis.json.
//
// Un enum en C seria mas rapido de escribir y estaria mal: el dia que la
// catedra sume una organizacion habria que tocar el enum, el parser y el
// asistente, y las tres se desincronizan igual que se desincronizaron las tres
// listas de keywords que mataron a la version anterior.
int paed_organizaciones(PAEDOrganizacion *out, int max) {
    if (!out || max <= 0 || paed_syntax_load() != 0) return -1;

    cJSON *archivos = cJSON_GetObjectItem(g_syntax, "archivos");
    cJSON *orgs     = cJSON_GetObjectItem(archivos, "organizaciones");

    int n = 0;
    cJSON *o = NULL;
    cJSON_ArrayForEach(o, orgs) {
        if (n >= max) break;

        cJSON *nombre = cJSON_GetObjectItem(o, "nombre");
        if (!cJSON_IsString(nombre)) continue;   // entrada rota: se saltea

        cJSON *etiq  = cJSON_GetObjectItem(o, "etiqueta");
        cJSON *claus = cJSON_GetObjectItem(o, "clausula");
        cJSON *camp  = cJSON_GetObjectItem(o, "campos");
        cJSON *desc  = cJSON_GetObjectItem(o, "descripcion");

        out[n].nombre      = nombre->valuestring;
        out[n].etiqueta    = cJSON_IsString(etiq) ? etiq->valuestring : nombre->valuestring;
        // 'clausula': null en el JSON — es el caso de secuencial, que no lleva
        // ninguna. cJSON lo entrega como un nodo de tipo NULL, no como string.
        out[n].clausula    = cJSON_IsString(claus) ? claus->valuestring : NULL;
        out[n].campos      = cJSON_IsString(camp)  ? camp->valuestring  : "ninguno";
        out[n].descripcion = cJSON_IsString(desc)  ? desc->valuestring  : "";
        out[n].implementado = cJSON_IsTrue(cJSON_GetObjectItem(o, "implementado"));
        out[n].en_asistente = cJSON_IsTrue(cJSON_GetObjectItem(o, "en_asistente"));
        n++;
    }
    return n;
}

static cJSON *buscar_proc(cJSON *raiz, const char *nombre) {
    if (!raiz) return NULL;
    cJSON *p = NULL;
    cJSON_ArrayForEach(p, cJSON_GetObjectItem(raiz, "procedimientos")) {
        cJSON *n = cJSON_GetObjectItem(p, "nombre");
        // El nombre del procedimiento tampoco distingue mayusculas:
        // ESCRIBIR y escribir son el mismo.
        if (cJSON_IsString(n) && strcasecmp(n->valuestring, nombre) == 0) return p;

        // Y las grafias alternativas de la catedra: Esc y GRABAR son ESCRIBIR.
        // La lista vive en sintaxis.json y no aca por la misma razon que las
        // organizaciones de archivo: con dos listas, un dia el parser y el
        // asistente del editor dicen cosas distintas.
        cJSON *a = NULL;
        cJSON_ArrayForEach(a, cJSON_GetObjectItem(p, "alias"))
            if (cJSON_IsString(a) && strcasecmp(a->valuestring, nombre) == 0) return p;
    }
    return NULL;
}

// El nombre CANONICO de un procedimiento, o NULL si no existe. El parser guarda
// siempre este y nunca el alias que escribio el usuario: asi el interprete
// compara contra "ESCRIBIR" y no tiene que conocer ninguna variante.
const char *proc_canonico(cJSON *def) {
    cJSON *n = def ? cJSON_GetObjectItem(def, "nombre") : NULL;
    return cJSON_IsString(n) ? n->valuestring : NULL;
}

// Primero el lenguaje, despues las librerias cargadas, en el orden en que se
// pidieron con USAR. Que el lenguaje vaya PRIMERO es la regla de siempre: una
// libreria extiende PAED, no lo redefine — nadie puede registrar su propio
// ESCRIBIR y tapar al de la catedra.
cJSON *proc_def(const char *nombre) {
    cJSON *p = buscar_proc(g_syntax, nombre);
    if (p) return p;
    for (int i = 0; i < g_lib_count; i++) {
        p = buscar_proc(g_libs[i], nombre);
        if (p) return p;
    }
    return NULL;
}

// Busca un parametro de un procedimiento por nombre o por alias.
cJSON *param_def(cJSON *proc, const char *clave) {
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

int proc_es_variadico(cJSON *proc) {
    return cJSON_IsTrue(cJSON_GetObjectItem(proc, "variadico"));
}

// Los modos de apertura que admite el procedimiento, o NULL si no admite
// ninguno. Es un objeto { "E": "entrada", ... }: la clave es el modo
// normalizado y el valor es como se lo nombra en los mensajes.
//
// Que ABRIR lleve modo y LEER no sale de sintaxis.json y no de una lista en C,
// por la misma razon que todo lo demas: la definicion del lenguaje vive en un
// solo lado.
cJSON *modos_def(cJSON *proc) {
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
void modos_listados(cJSON *modos, char *out, size_t n) {
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


#include "paed/parser.h"
#include "paed/errores.h"
#include "cJSON.h"

#include <ctype.h>
#include <stdarg.h>
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
static const char *proc_canonico(cJSON *def) {
    cJSON *n = def ? cJSON_GetObjectItem(def, "nombre") : NULL;
    return cJSON_IsString(n) ? n->valuestring : NULL;
}

// Primero el lenguaje, despues las librerias cargadas, en el orden en que se
// pidieron con USAR. Que el lenguaje vaya PRIMERO es la regla de siempre: una
// libreria extiende PAED, no lo redefine — nadie puede registrar su propio
// ESCRIBIR y tapar al de la catedra.
static cJSON *proc_def(const char *nombre) {
    cJSON *p = buscar_proc(g_syntax, nombre);
    if (p) return p;
    for (int i = 0; i < g_lib_count; i++) {
        p = buscar_proc(g_libs[i], nombre);
        if (p) return p;
    }
    return NULL;
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

// Corta el comentario y el salto de linea. La catedra escribe comentarios de
// TRES formas, y las tres estan en material oficial:
//
//     // hasta fin de linea      la wiki, y todos los .paed del repo
//     { entre llaves }           templates de UTN-FRRe/isi-aed/Pseudocodigo
//     * entre asteriscos *       diapositivas de los Temas 12 y 13
//
// Las dos ultimas solo se reconocen cuando ABREN LA LINEA, y no es una
// limitacion caprichosa: los dos caracteres ya significan otra cosa en el medio
// de una linea.
//
//     d: {1..31};        <- '{' despues de ':' es un tipo RANGO, no un comentario
//     a := b * c;        <- '*' entre operandos es multiplicacion
//     x := 2 ** 3;       <- y '**' es potencia
//
// Todos los comentarios con llave del material oficial ocupan su propio renglon
// ({Asigno el valor a retornar}, {Avanzo el guion}), asi que la regla no pierde
// ningun caso real y no puede comerse un tipo ni una cuenta.
static void strip_comment(char *s) {
    // El '{' o el '* ' de apertura pueden venir con sangria adelante.
    char *ini = s;
    while (*ini == ' ' || *ini == '\t') ini++;

    if (*ini == '{') {
        char *cierre = strchr(ini, '}');
        // Sin '}' el comentario se come el resto de la linea, que es lo que el
        // que lo escribio quiso decir.
        if (cierre) memmove(ini, cierre + 1, strlen(cierre + 1) + 1);
        else        *ini = '\0';
        // Puede quedar codigo despues del comentario: se vuelve a mirar.
        strip_comment(s);
        return;
    }

    // '* ' con espacio: '*p' es desreferencia y '**' es potencia, ninguno abre
    // comentario. Se pide el espacio para no confundirlos.
    if (ini[0] == '*' && (ini[1] == ' ' || ini[1] == '\t')) { *ini = '\0'; return; }

    char comilla = 0;
    for (char *c = s; *c; c++) {
        if (!comilla && (*c == '"' || *c == '\'')) comilla = *c;
        else if (comilla && *c == comilla)          comilla = 0;
        if (!comilla && c[0] == '/' && c[1] == '/') { *c = '\0'; return; }
    }
}

// ¿Este byte puede ser parte de un nombre?
//
// Las letras ASCII, los digitos y el '_' de siempre, MAS cualquier byte de
// UTF-8 multibyte (>= 0x80). La catedra escribe campos con ñ y con tildes —
// 'año' aparece en REGISTRO.txt, ARCHIVO_CREAR.txt y ARCHIVO_LEER.txt, los tres
// templates oficiales — y rechazarlos obligaria a reescribir sus programas para
// correrlos.
//
// Se acepta el byte crudo sin decodificar el punto de codigo: los nombres se
// comparan y se guardan como bytes, asi que alcanza con dejarlos pasar enteros.
// Las palabras clave siguen siendo ASCII puro, asi que ningun nombre con ñ
// puede chocar con una.
static int byte_de_nombre(unsigned char c) {
    return isalnum(c) || c == '_' || c >= 0x80;
}

// Igual, pero para el PRIMER byte: un nombre no puede empezar con digito.
static int byte_inicial_de_nombre(unsigned char c) {
    return isalpha(c) || c == '_' || c >= 0x80;
}

static int es_identificador(const char *s) {
    if (!*s || !byte_inicial_de_nombre((unsigned char)*s)) return 0;
    for (const char *c = s; *c; c++)
        if (!byte_de_nombre((unsigned char)*c)) return 0;
    return 1;
}

// Igual que es_identificador, pero acepta el punto de acceso a campo:
// 'pori.vx' es un nombre valido y 'pori' tambien.
//
// El punto tiene que estar ENTRE dos partes validas. Asi '.x', 'pori.' y
// 'pori..vx' se siguen rechazando, y un numero como 1.5 nunca entra aca porque
// no empieza con letra.
static int es_campo(const char *s) {
    if (!*s || !byte_inicial_de_nombre((unsigned char)*s)) return 0;

    for (const char *c = s; *c; c++) {
        if (*c == '.') {
            // ni al principio, ni al final, ni dos seguidos
            if (c == s || !byte_inicial_de_nombre((unsigned char)c[1])) return 0;
            continue;
        }
        if (!byte_de_nombre((unsigned char)*c)) return 0;
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
    for (int i = 0; i < prog->error_count; i++) {
        // El codigo de la familia a la que pertenece el mensaje, para poder
        // buscarlo en xasolError/. Vacio si todavia no esta catalogado, y ahi
        // el error sale como salia antes. Ver lang/src/errores.c.
        const char *cod = paed_codigo_error(prog->errors[i].msg);
        fprintf(stderr, "%s:%d: error%s%s: %s\n",
                prog->path, prog->errors[i].line,
                cod[0] ? " " : "", cod, prog->errors[i].msg);
    }
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
    in->siguiente = -1;
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

static void parse_instruction(PAEDProgram *p, char *linea, int lineno) {
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
    PAEDKind kind;   // PAED_SI, PAED_MIENTRAS, PAED_PARA, PAED_REPETIR o PAED_SEGUN
    int      line;   // donde se abrio, para poder decirlo en el error
    int      instr;  // que instruccion lo abrio, para parchearle el salto
    int      sino;   // indice del SINO si ya aparecio; -1 si no
    int      ultimo_caso; // SEGUN: indice del ultimo CASO, para encadenar las ramas
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
        case PAED_REPETIR:  return "REPETIR";
        case PAED_SEGUN:    return "SEGUN";
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
    pila->items[pila->tope++] = (Abierto){ kind, lineno, p->instr_count - 1, -1, -1 };
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

        // El paso es OPCIONAL: sin el, vale 1. Lo separa una coma o un ';'.
        //
        // La catedra usa la COMA — 'Para c := 1 hasta 10, 1 hacer' aparece en
        // Para.txt, SUBSECUENCIA.txt y ARREGLOS_Conceptos.txt, y ni una sola
        // vez con ';'. PAED habia elegido ';'; desde el 2026-08-17 acepta las
        // dos, con la coma como forma de catedra.
        //
        // Se busca el separador de NIVEL SUPERIOR, no el primero que aparezca:
        // en 'hasta f(a, b), 1' la coma de adentro del parentesis es del
        // argumento y no del paso. Sin esta cuenta, ese PARA se partiria mal y
        // el error saldria en un lugar que no tiene nada que ver.
        const char *paso = "1";
        char *sep = NULL;
        int   prof = 0;
        for (char *c = hasta; *c; c++) {
            if      (*c == '(' || *c == '[') prof++;
            else if (*c == ')' || *c == ']') prof--;
            else if (prof == 0 && (*c == ',' || *c == ';')) { sep = c; break; }
        }
        if (sep) {
            char corte = *sep;
            *sep = '\0';
            char *pval = trim(sep + 1);
            if (!*pval) {
                add_error(p, lineno, "el PARA tiene '%c' pero no dice el incremento", corte);
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
        pila->items[pila->tope++] = (Abierto){ PAED_PARA, lineno, p->instr_count - 1, -1, -1 };
        return 1;
    }

    // ── SEGUN <expresion> HACER ──
    //
    // La seleccion multiple de la catedra. Confirmada por dos templates
    // oficiales: Segun.txt y ACT INDEX [TEMPLATE].txt.
    if (empieza_con(linea, "SEGUN")) {
        char *cuerpo = cuerpo_cabecera(linea, "SEGUN", "HACER");
        if (!cuerpo) {
            add_error(p, lineno, "se esperaba: SEGUN <expresion> HACER");
            return 1;
        }
        if (!*cuerpo) {
            add_error(p, lineno, "al SEGUN le falta la expresion a comparar");
            return 1;
        }
        abrir(p, pila, PAED_SEGUN, cuerpo, lineno);
        return 1;
    }

    // ── FIN_SEGUN ──
    if (kw_es(linea, "FIN_SEGUN")) {
        if (pila->tope == 0 || pila->items[pila->tope - 1].kind != PAED_SEGUN) {
            add_error(p, lineno, "FIN_SEGUN sin un SEGUN abierto");
            return 1;
        }
        Abierto *a = &pila->items[pila->tope - 1];

        PAEDInstr *in = nueva_instr(p, PAED_FIN_SEGUN, lineno);
        if (!in) return 1;
        int fin = p->instr_count - 1;

        // Sin ninguna rama, el SEGUN salta derecho al final.
        if (a->ultimo_caso < 0) p->instrs[a->instr].salto = fin;

        // Cada rama sale por el final, y la ultima ademas cierra la cadena.
        for (int k = a->instr + 1; k <= fin; k++)
            if (p->instrs[k].kind == PAED_CASO) p->instrs[k].salto = fin;
        if (a->ultimo_caso >= 0) p->instrs[a->ultimo_caso].siguiente = -1;

        // La rama por defecto (SINO / CONTRARIO) se guardo en `sino`.
        if (a->sino >= 0) p->instrs[a->sino].salto = fin;

        pila->tope--;
        return 1;
    }

    // ── Una rama del SEGUN: <etiqueta>[, <etiqueta>]: <sentencias> ──
    //
    // Se reconoce SOLO adentro de un SEGUN. Afuera, una linea con ':' es una
    // declaracion o un error, y robarsela aca daria un mensaje incomprensible.
    if (pila->tope > 0 && pila->items[pila->tope - 1].kind == PAED_SEGUN) {
        Abierto *a = &pila->items[pila->tope - 1];

        // La rama por defecto. 'CONTRARIO' ya llego aca convertido en 'SINO'
        // por normalizar_catedra; 'CONTRARIO:' con dos puntos es la forma del
        // template de ACT INDEX, asi que se acepta con y sin ellos.
        if (kw_es(linea, "SINO") || kw_es(linea, "SINO:") ||
            kw_es(linea, "CONTRARIO") || kw_es(linea, "CONTRARIO:")) {
            if (a->sino >= 0) {
                add_error(p, lineno, "el SEGUN de la linea %d ya tiene una rama por defecto",
                          a->line);
                return 1;
            }
            PAEDInstr *in = nueva_instr(p, PAED_CASO, lineno);
            if (!in) return 1;
            int idx = p->instr_count - 1;
            in->cond[0] = '\0';                 // sin etiquetas = rama por defecto
            in->siguiente = -1;
            if (a->ultimo_caso >= 0) p->instrs[a->ultimo_caso].siguiente = idx;
            else                     p->instrs[a->instr].salto = idx;
            a->ultimo_caso = idx;
            a->sino = idx;
            return 1;
        }

        // Una rama con etiquetas. Los dos puntos que la abren son los primeros
        // de NIVEL SUPERIOR: en "'a': ESCRIBIR('x: y')" el segundo esta adentro
        // de un texto y no separa nada.
        int prof = 0;
        char comilla_et = 0;
        char *dosp = NULL;
        for (char *c = linea; *c; c++) {
            if (!comilla_et && (*c == '\'' || *c == '"')) { comilla_et = *c; continue; }
            if (comilla_et) { if (*c == comilla_et) comilla_et = 0; continue; }
            if      (*c == '(' || *c == '[') prof++;
            else if (*c == ')' || *c == ']') prof--;
            else if (*c == ':' && prof == 0) {
                if (c[1] == '=') break;          // ':=' es una asignacion, no una rama
                dosp = c;
                break;
            }
        }

        if (dosp) {
            *dosp = '\0';
            char *etiquetas = trim(linea);
            char *resto     = trim(dosp + 1);

            if (!*etiquetas) {
                add_error(p, lineno, "a la rama del SEGUN le faltan las etiquetas antes de ':'");
                return 1;
            }

            // 'CONTRARIO: <sentencia>' — la rama por defecto con los dos puntos
            // pegados, como en ACT INDEX [TEMPLATE].txt. Sin este caso,
            // 'CONTRARIO' se leeria como el nombre de una variable y el error
            // saldria recien al ejecutar.
            if (kw_es(etiquetas, "SINO") || kw_es(etiquetas, "CONTRARIO")) {
                if (a->sino >= 0) {
                    add_error(p, lineno, "el SEGUN de la linea %d ya tiene una rama por defecto",
                              a->line);
                    return 1;
                }
                PAEDInstr *d = nueva_instr(p, PAED_CASO, lineno);
                if (!d) return 1;
                int didx = p->instr_count - 1;
                d->cond[0]  = '\0';
                d->siguiente = -1;
                if (a->ultimo_caso >= 0) p->instrs[a->ultimo_caso].siguiente = didx;
                else                     p->instrs[a->instr].salto = didx;
                a->ultimo_caso = didx;
                a->sino = didx;
                if (*resto) parse_sentencias(p, resto, lineno);
                return 1;
            }

            PAEDInstr *in = nueva_instr(p, PAED_CASO, lineno);
            if (!in) return 1;
            int idx = p->instr_count - 1;
            strncpy(in->cond, etiquetas, PAED_COND_MAX - 1);
            in->siguiente = -1;
            if (a->ultimo_caso >= 0) p->instrs[a->ultimo_caso].siguiente = idx;
            else                     p->instrs[a->instr].salto = idx;
            a->ultimo_caso = idx;

            // El cuerpo puede venir en la misma linea (que es como lo escriben
            // los dos templates) o en las de abajo. Si viene, se parsea igual
            // que cualquier otra sentencia.
            if (*resto) parse_sentencias(p, resto, lineno);
            return 1;
        }
    }

    // ── REPETIR ──
    //
    // El ciclo POST-TEST de la catedra (template Repetir.txt, ver PAED.md 0 para
    // donde viven los templates). Es una cabecera
    // pelada: no lleva condicion, porque la condicion vive en el HASTA de abajo.
    if (kw_es(linea, "REPETIR")) {
        abrir(p, pila, PAED_REPETIR, "", lineno);
        return 1;
    }

    // ── HASTA [QUE] <condicion> ──
    //
    // Cierra el REPETIR. Se aceptan las dos formas del material:
    //
    //     Hasta que c > 10;     template Repetir.txt
    //     HASTA (condicion)     la wiki
    //
    // No se pisa con el HASTA del PARA: ese vive DENTRO de la cabecera del
    // PARA, que ya se trato mas arriba y nunca llega aca.
    if (empieza_con(linea, "HASTA")) {
        char *cond = trim(linea + 5);

        // 'que' es opcional y no aporta nada mas que leerse bien.
        if (empieza_con(cond, "QUE")) cond = trim(cond + 3);

        // 'Hasta que c > 10;' trae el ';' de la catedra. Es un cierre de bloque,
        // asi que se lo saca como a cualquier otro.
        for (size_t n = strlen(cond); n > 0 &&
             (cond[n - 1] == ';' || isspace((unsigned char)cond[n - 1])); n = strlen(cond))
            cond[n - 1] = '\0';

        if (!*cond) {
            add_error(p, lineno, "al HASTA le falta la condicion: HASTA QUE <condicion>");
            return 1;
        }
        if (pila->tope == 0 || pila->items[pila->tope - 1].kind != PAED_REPETIR) {
            add_error(p, lineno, "HASTA sin un REPETIR abierto");
            return 1;
        }

        Abierto *a = &pila->items[pila->tope - 1];
        PAEDInstr *in = nueva_instr(p, PAED_HASTA, lineno);
        if (!in) return 1;
        strncpy(in->cond, cond, PAED_COND_MAX - 1);

        // Condicion FALSA -> volver al cuerpo, que arranca justo despues del
        // REPETIR. Es al reves que el MIENTRAS a proposito: aca la condicion
        // dice cuando TERMINAR, no cuando seguir.
        in->salto = a->instr + 1;
        pila->tope--;
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
    // El ';' final es opcional, por el mismo motivo que en las instrucciones:
    // la catedra lo escribe de las dos formas. El AMBIENTE de SECUENCIA.txt no
    // lo lleva y el de REGISTRO.txt si.
    size_t len = strlen(linea);
    if (len > 0 && linea[len - 1] == ';') linea[len - 1] = '\0';
    if (!*trim(linea)) return;

    // 'HV = 99999999;' es como los templates de mezcla y de actualizacion
    // declaran el centinela. Se ACEPTA y se IGNORA, y eso ultimo es lo
    // importante: en PAED, HV es un valor de alto propio del lenguaje y tiene
    // que seguir siendolo. Las claves de los parciales son TEXTO, y
    // strcmp("99999999", "F1-Ibuprofeno") daria que HV es MENOR — justo al
    // reves de lo que HV significa (ver PAED.md 2.8).
    //
    // Asi el programa del parcial se escribe TAL CUAL y ademas compara bien.
    {
        char copia[PAED_LINEA_MAX];
        snprintf(copia, sizeof(copia), "%s", linea);
        char *ig = strchr(copia, '=');
        if (ig) {
            *ig = '\0';
            if (kw_es(trim(copia), "HV")) return;
        }
    }

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
static char *normalizar_catedra(char *linea, size_t espacio) {
    sacar_terminador(linea);

    for (size_t i = 0; i < sizeof(GRAFIAS) / sizeof(GRAFIAS[0]); i++)
        if (kw_es(linea, GRAFIAS[i].catedra)) {
            if (strlen(GRAFIAS[i].paed) + 1 > espacio) return linea;   // no entra: se deja como vino
            memcpy(linea, GRAFIAS[i].paed, strlen(GRAFIAS[i].paed) + 1);
            return linea;
        }

    return linea;
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

// ¿Esta linea declara un CONJUNTO? `<nombre> = { ... }`, SIN tocar la linea.
//
// Se mira que despues del '=' venga una llave. Con eso alcanza para separarlo
// de `x = REGISTRO` y de una declaracion normal, que lleva ':' y no '='.
static int abre_conjunto(const char *linea) {
    const char *ig = strchr(linea, '=');
    if (!ig) return 0;

    const char *r = ig + 1;
    while (*r && isspace((unsigned char)*r)) r++;
    return *r == '{';
}

// Carga los elementos de `{a, b, c}` en el conjunto.
//
// Se guardan como TEXTO y sin las comillas: el que compara es el evaluador, con
// las reglas del '=' del lenguaje. Aca no se decide de que tipo es nada.
//
// El corte es a mano y no con strtok por dos motivos, y el segundo es el que
// importa: strtok COLAPSA los separadores seguidos, asi que `{1,,2}` le pasaba
// como si fueran dos elementos y `{1, , 2}` daba error — el mismo tipeo con dos
// resultados distintos, que es peor que cualquiera de los dos. Ademas strtok
// guarda estado global, y un parser es el ultimo lugar donde uno quiere eso.
static void parse_elementos(PAEDProgram *p, PAEDConjunto *cj, char *dentro,
                            int lineno) {
    // `{}` no es un elemento vacio: es un conjunto vacio, y eso lo reporta el
    // que llama con su propio mensaje. Sin este corte, la coma que no esta se
    // leeria como un elemento en blanco y el error hablaria de otra cosa.
    if (*trim(dentro) == '\0') return;

    char *item = dentro;

    for (;;) {
        char *coma = strchr(item, ',');
        if (coma) *coma = '\0';

        char *e = trim(item);

        // Las comillas son del literal, no del dato: "A" y 'A' y A son el
        // mismo elemento. Sacarlas aca deja una sola forma para comparar.
        size_t n = strlen(e);
        if (n >= 2 && (e[0] == '"' || e[0] == '\'') && e[n - 1] == e[0]) {
            e[n - 1] = '\0';
            e++;
        }

        if (*e == '\0') {
            add_error(p, lineno, "el conjunto '%s' tiene un elemento vacio",
                      cj->name);
            return;
        }
        if (cj->elem_count >= PAED_MAX_ELEMENTOS) {
            add_error(p, lineno, "el conjunto '%s' no entra: maximo %d elementos",
                      cj->name, PAED_MAX_ELEMENTOS);
            return;
        }

        strncpy(cj->elems[cj->elem_count], e, PAED_NAME_MAX - 1);
        cj->elems[cj->elem_count][PAED_NAME_MAX - 1] = '\0';
        cj->elem_count++;

        if (!coma) return;
        item = coma + 1;
    }
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

    // ── ¿Declara un CONJUNTO? `<nombre> = { ... }` ──
    // Va antes del REGISTRO porque los dos empiezan igual, con '=', y este se
    // reconoce por la llave.
    if (abre_conjunto(linea)) {
        char *llave = strchr(linea, '{');
        char *cierra = strrchr(linea, '}');
        if (!cierra) {
            add_error(p, lineno,
                      "falta '}' para cerrar el conjunto: se escribe "
                      "nombre = {a, b, c};");
            return;
        }

        *strchr(linea, '=') = '\0';
        char *nombre = trim(linea);
        *cierra = '\0';

        if (!es_identificador(nombre)) {
            add_error(p, lineno, "nombre de conjunto invalido: '%s'", nombre);
            return;
        }
        if (p->conjunto_count >= PAED_MAX_CONJUNTOS) {
            add_error(p, lineno, "demasiados conjuntos (maximo %d)",
                      PAED_MAX_CONJUNTOS);
            return;
        }
        for (int i = 0; i < p->conjunto_count; i++)
            if (kw_es(p->conjuntos[i].name, nombre)) {
                add_error(p, lineno,
                          "el conjunto '%s' ya estaba declarado en la linea %d",
                          nombre, p->conjuntos[i].line);
                return;
            }

        PAEDConjunto *cj = &p->conjuntos[p->conjunto_count++];
        memset(cj, 0, sizeof(*cj));
        strncpy(cj->name, nombre, PAED_NAME_MAX - 1);
        cj->line = lineno;
        parse_elementos(p, cj, llave + 1, lineno);

        if (cj->elem_count == 0)
            add_error(p, lineno,
                      "conjunto sin elementos: '%s' no tiene ninguno", nombre);
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

// ── Subacciones ─────────────────────────────────────────────────────────────
//
// Ver parser.h para la forma que tienen y por que el cuerpo va en el mismo
// instrs[] que el PROCESO principal.

const PAEDSubaccion *paed_subaccion(const PAEDProgram *prog, const char *nombre) {
    for (int i = 0; i < prog->subaccion_count; i++)
        if (strcasecmp(prog->subacciones[i].name, nombre) == 0)
            return &prog->subacciones[i];
    return NULL;
}

// El modo de un parametro, tal como lo escribe la catedra ADELANTE del nombre:
// 'E a: ENTERO'. Devuelve -1 si la palabra no es un modo.
//
// VAR es 'por referencia' y es la forma que mas aparece en los templates
// oficiales. Por referencia significa que entra con valor y sale modificado,
// que es exactamente ES: no hace falta un cuarto modo.
static int modo_de_param(const char *p) {
    if (strcasecmp(p, "E")   == 0) return PAED_PARAM_E;
    if (strcasecmp(p, "S")   == 0) return PAED_PARAM_S;
    if (strcasecmp(p, "ES")  == 0) return PAED_PARAM_ES;
    if (strcasecmp(p, "VAR") == 0) return PAED_PARAM_ES;
    return -1;
}

// Un parametro suelto: '[MODO] nombre: TIPO'.
//
// El TIPO se guarda como texto y no se valida, igual que en el AMBIENTE: puede
// traer espacios ('SECUENCIA de caracter') o parentesis ('AN(20)').
static void parse_param(PAEDProgram *p, PAEDSubaccion *sub, char *texto, int lineno) {
    char *t = trim(texto);
    if (!*t) return;

    if (sub->param_count >= PAED_MAX_PARAMS) {
        add_error(p, lineno, "'%s' tiene mas de %d parametros",
                  sub->name, PAED_MAX_PARAMS);
        return;
    }

    char *dosp = strchr(t, ':');
    if (!dosp) {
        add_error(p, lineno,
                  "parametro sin tipo en '%s': se escribe 'E nombre: TIPO' y falta ': TIPO' en '%s'",
                  sub->name, t);
        return;
    }
    *dosp = '\0';
    char *tipo = trim(dosp + 1);
    char *izq  = trim(t);

    // A la izquierda de los dos puntos puede haber una palabra (el nombre) o
    // dos (el modo y el nombre). Se parte por el primer espacio.
    PAEDParam *pa = &sub->params[sub->param_count];
    memset(pa, 0, sizeof(*pa));
    pa->modo = PAED_PARAM_E;   // sin modo escrito, entra por valor

    char *esp = izq;
    while (*esp && !isspace((unsigned char)*esp)) esp++;

    if (*esp) {
        *esp = '\0';
        int m = modo_de_param(izq);
        if (m < 0) {
            add_error(p, lineno,
                      "'%s' no es un modo de parametro: los modos son E, S, ES y VAR",
                      izq);
            return;
        }
        pa->modo = (PAEDModoParam)m;
        izq = trim(esp + 1);
    }

    if (!*izq) {
        add_error(p, lineno, "parametro sin nombre en '%s'", sub->name);
        return;
    }
    if (!es_identificador(izq)) {
        add_error(p, lineno, "nombre de parametro invalido: '%s'", izq);
        return;
    }
    for (int i = 0; i < sub->param_count; i++)
        if (strcasecmp(sub->params[i].name, izq) == 0) {
            add_error(p, lineno, "'%s' tiene dos parametros llamados '%s'", sub->name, izq);
            return;
        }
    if (!*tipo) {
        add_error(p, lineno, "el parametro '%s' no dice de que tipo es", izq);
        return;
    }

    snprintf(pa->name, PAED_NAME_MAX, "%s", izq);
    snprintf(pa->type, PAED_NAME_MAX, "%s", tipo);
    sub->param_count++;
}

// ¿La linea abre una subaccion? Devuelve la palabra clave que uso, o NULL.
static const char *abre_subaccion(const char *linea) {
    if (empieza_con(linea, "FUNCION"))       return "FUNCION";
    if (empieza_con(linea, "PROCEDIMIENTO")) return "PROCEDIMIENTO";
    if (empieza_con(linea, "SUBACCION"))     return "SUBACCION";
    return NULL;
}

// La cabecera entera:
//
//     FUNCION sumar(E a: ENTERO; E b: ENTERO): ENTERO
//     PROCEDIMIENTO saludar(E nombre: AN(20))
//     Procedimiento InicializarSecuencia(VAR sec: SECUENCIA de caracter);
//
// Devuelve la subaccion recien creada, o NULL si la cabecera no sirve.
static PAEDSubaccion *parse_subaccion_cabecera(PAEDProgram *p, const char *kw,
                                               char *linea, int lineno) {
    if (p->subaccion_count >= PAED_MAX_SUBACCIONES) {
        add_error(p, lineno, "demasiadas subacciones (maximo %d)", PAED_MAX_SUBACCIONES);
        return NULL;
    }

    char cab[PAED_LINEA_MAX];
    snprintf(cab, sizeof(cab), "%s", trim(linea + strlen(kw)));

    // Los templates oficiales terminan la firma con ';' y algunos con '.'.
    // Ninguno de los dos aporta nada: se sacan antes de leer.
    for (size_t n = strlen(cab); n > 0 &&
         (cab[n - 1] == ';' || cab[n - 1] == '.' ||
          isspace((unsigned char)cab[n - 1])); n = strlen(cab))
        cab[n - 1] = '\0';

    PAEDSubaccion *sub = &p->subacciones[p->subaccion_count];
    memset(sub, 0, sizeof(*sub));
    sub->line       = lineno;
    sub->es_funcion = (strcasecmp(kw, "FUNCION") == 0);
    sub->inicio     = -1;
    sub->fin        = -1;

    // El nombre llega hasta el '(' de los parametros, o hasta el ':' del tipo
    // de retorno, o hasta el final si no tiene ninguno de los dos.
    char *par = strchr(cab, '(');
    char *fin_nombre = par;
    if (!fin_nombre) fin_nombre = strchr(cab, ':');

    char nombre[PAED_LINEA_MAX];
    if (fin_nombre) {
        snprintf(nombre, sizeof(nombre), "%.*s", (int)(fin_nombre - cab), cab);
    } else {
        snprintf(nombre, sizeof(nombre), "%s", cab);
    }
    char *n = trim(nombre);

    // 'SUBACCION corte_1 ES' — el 'ES' es de la catedra y es OPCIONAL, igual que
    // en la cabecera de la ACCION. Se saca antes de leer el nombre para que no
    // se le pegue y termine dando "nombre de subaccion invalido: 'corte_1 ES'".
    size_t ln = strlen(n);
    if (ln > 3 && strcasecmp(n + ln - 3, " ES") == 0) {
        n[ln - 3] = '\0';
        n = trim(n);
    }

    if (!*n) {
        add_error(p, lineno, "falta el nombre despues de %s", kw);
        return NULL;
    }
    if (!es_identificador(n)) {
        add_error(p, lineno, "nombre de subaccion invalido: '%s'", n);
        return NULL;
    }
    if (paed_subaccion(p, n)) {
        add_error(p, lineno, "ya hay una subaccion llamada '%s'", n);
        return NULL;
    }
    snprintf(sub->name, PAED_NAME_MAX, "%s", n);

    // ── Los parametros ──
    char *resto = NULL;
    if (par) {
        char *cierra = strrchr(par, ')');
        if (!cierra) {
            add_error(p, lineno, "falta ')' en los parametros de '%s'", sub->name);
            return NULL;
        }
        *cierra = '\0';
        resto   = cierra + 1;

        // Los parametros se separan con ';' en la catedra. Se acepta ',' porque
        // aparece igual en el corpus y confundirlos no cambia el significado.
        char *lista = par + 1;
        char *inicio = lista;
        for (char *c = lista; ; c++) {
            if (*c == ';' || *c == ',' || *c == '\0') {
                char guardado = *c;
                *c = '\0';
                parse_param(p, sub, inicio, lineno);
                if (guardado == '\0') break;
                inicio = c + 1;
            }
        }
    } else {
        resto = strchr(cab, ':');
    }

    // ── El tipo de retorno ──
    if (resto) {
        char *dosp = strchr(resto, ':');
        if (dosp) {
            char *tipo = trim(dosp + 1);
            if (!*tipo) {
                add_error(p, lineno, "'%s' dice que devuelve algo pero no dice de que tipo",
                          sub->name);
            } else if (!sub->es_funcion) {
                // Un PROCEDIMIENTO que declara tipo casi siempre queria ser una
                // FUNCION. Decirlo asi ahorra la vuelta de "por que no devuelve".
                add_error(p, lineno,
                          "un PROCEDIMIENTO no devuelve nada, y '%s' declara que devuelve '%s': "
                          "si tiene que devolver un valor es una FUNCION",
                          sub->name, tipo);
            } else {
                snprintf(sub->retorno, PAED_NAME_MAX, "%s", tipo);
            }
        }
    }

    if (sub->es_funcion && !*sub->retorno) {
        add_error(p, lineno,
                  "'%s' es una FUNCION y no dice que tipo devuelve: se escribe "
                  "FUNCION %s(...): TIPO", sub->name, sub->name);
    }

    p->subaccion_count++;
    return sub;
}

// Los errores se reportan en el orden en que se ENCONTRARON, y algunos no se
// encuentran leyendo: las claves, los modos y las llamadas a subacciones se
// verifican al final, con el programa entero en la mano. Sin ordenar, esos
// errores caen todos juntos despues de los demas, y quien lee el reporte
// arregla la linea 30, vuelve a compilar, y recien ahi se entera de que la 9
// tambien estaba mal.
//
// Es una insercion y no un qsort porque tiene que ser ESTABLE: dos errores de
// la misma linea son independientes, y el orden entre ellos es el orden en que
// se detectaron, que es el que mejor se lee.
static void ordenar_errores(PAEDProgram *p) {
    for (int i = 1; i < p->error_count; i++) {
        PAEDError e = p->errors[i];
        int j = i - 1;
        while (j >= 0 && p->errors[j].line > e.line) {
            p->errors[j + 1] = p->errors[j];
            j--;
        }
        p->errors[j + 1] = e;
    }
}

// Verifica TODAS las llamadas a subacciones de una sola vez, con el programa
// entero leido. Va aparte del parseo linea por linea porque recien aca se sabe
// que subacciones existen: una puede llamar a otra declarada mas abajo.
static void chequear_subacciones(PAEDProgram *p) {
    for (int i = 0; i < p->instr_count; i++) {
        PAEDInstr *in = &p->instrs[i];
        if (in->kind != PAED_LLAMADA || !in->es_subaccion) continue;

        const PAEDSubaccion *sub = paed_subaccion(p, in->proc);
        if (!sub) {
            if (g_lib_count > 0) {
                // Nombrar las librerias que SI se cargaron es media respuesta:
                // el que se olvido un USAR ve cual falta comparando.
                char cargadas[256] = {0};
                size_t usado = 0;
                for (int k = 0; k < g_lib_count && usado < sizeof(cargadas) - 1; k++)
                    usado += (size_t)snprintf(cargadas + usado, sizeof(cargadas) - usado,
                                              "%s%s", k ? ", " : "", g_libs_nombre[k]);
                add_error(p, in->line,
                          "procedimiento desconocido '%s' (no esta en %s, ni en las "
                          "librerias que pediste con USAR (%s), ni es una subaccion "
                          "de este programa)",
                          in->proc, PAED_SYNTAX_FILE, cargadas);
            } else
                add_error(p, in->line,
                          "procedimiento desconocido '%s' (no esta en %s ni es una "
                          "subaccion de este programa)", in->proc, PAED_SYNTAX_FILE);
            continue;
        }

        // Una FUNCION devuelve un valor: llamarla como instruccion suelta tira
        // ese valor a la basura, y casi siempre significa que se quiso asignar.
        if (sub->es_funcion)
            add_error(p, in->line,
                      "'%s' es una FUNCION y devuelve un valor: va adentro de una "
                      "expresion (x := %s(...)), no como instruccion suelta",
                      sub->name, sub->name);

        if (in->arg_count != sub->param_count)
            add_error(p, in->line,
                      "'%s' lleva %d argumento(s) y se le pasaron %d",
                      sub->name, sub->param_count, in->arg_count);

        // El nombre canonico: la subaccion se guarda como la declaro el
        // programador, no como la escribio quien la llama.
        strncpy(in->proc, sub->name, PAED_NAME_MAX - 1);
    }
}

// Los tres estados SUB_* son los mismos tres de arriba pero DENTRO de una
// subaccion. Se duplican en vez de llevar una bandera aparte porque cada linea
// del archivo cae en exactamente uno, y un estado que hay que cruzar con un
// booleano para saber que significa es dos estados escritos mal.
typedef enum { FUERA, CABECERA, AMBIENTE, PROCESO, CERRADO,
               SUB_CABECERA, SUB_AMBIENTE, SUB_PROCESO } Bloque;

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

    // La subaccion que se esta leyendo, NULL si ninguna. `bloque_antes_de_sub`
    // es a donde se vuelve al cerrarla — CABECERA o AMBIENTE, segun donde
    // estaba declarada.
    PAEDSubaccion *sub                = NULL;
    Bloque         bloque_antes_de_sub = CABECERA;

    // parse_ambiente escribe siempre en out->decls[]. Las declaraciones de una
    // subaccion son LOCALES, asi que se anota donde empiezan y al cerrarla se
    // mudan a sub->locales[]. Es mas barato que darle un destino configurable
    // a todo el parseo del AMBIENTE, y deja esa funcion sin saber que existen
    // las subacciones.
    int decl_base_sub = 0;

    while (fgets(buf, sizeof(buf), f)) {
        lineno++;
        strip_comment(buf);
        char *linea = trim(buf);
        if (!*linea) continue;
        // Las grafias de la catedra se traducen ACA, antes de que cualquier
        // otra parte del parser mire la linea. Asi la lista de variantes vive
        // en un solo lugar (GRAFIAS) y no repartida por cada bloque.
        linea = normalizar_catedra(linea, sizeof(buf) - (size_t)(linea - buf));

        // ── USAR <libreria>; ──
        //
        // NO es de la catedra: es una extension de PAED, igual que lo son CUBO
        // o BILLBOARD. AED no tiene modulos porque un ejercicio de parcial entra
        // en una hoja; un juego no.
        //
        // Va ANTES de la ACCION y se procesa mientras se lee la linea, no al
        // final: los procedimientos que trae la libreria tienen que estar
        // registrados antes de que el parser llegue a la primera llamada.
        if (strncasecmp(linea, "USAR", 4) == 0 &&
            (linea[4] == ' ' || linea[4] == '\t')) {
            if (bloque != FUERA) {
                add_error(out, lineno,
                          "USAR va ANTES de la ACCION: una libreria se pide al "
                          "empezar el archivo, no en medio del programa");
                continue;
            }

            char nombre[64] = {0};
            const char *c = linea + 4;
            while (*c == ' ' || *c == '\t') c++;
            size_t k = 0;
            while ((isalnum((unsigned char)*c) || *c == '_') && k < sizeof(nombre) - 1)
                nombre[k++] = *c++;
            nombre[k] = '\0';
            while (*c == ' ' || *c == '\t') c++;
            if (*c == ';') c++;
            while (*c == ' ' || *c == '\t') c++;

            if (!k) {
                add_error(out, lineno, "USAR necesita el nombre de una libreria: USAR mundo;");
                continue;
            }
            if (*c) {
                add_error(out, lineno,
                          "USAR pide UNA libreria por linea: 'USAR %s;' y la otra abajo",
                          nombre);
                continue;
            }
            if (paed_syntax_load_lib(nombre) != 0)
                add_error(out, lineno,
                          "no encontre la libreria '%s': falta %s.json en el "
                          "directorio de datos", nombre, nombre);
            continue;
        }

        if (strncasecmp(linea, "ACCION", 6) == 0 && (linea[6] == ' ' || linea[6] == '\t')) {
            if (bloque != FUERA) {
                add_error(out, lineno, "ACCION anidada: un archivo .paed tiene una sola ACCION");
                continue;
            }
            char nombre[PAED_NAME_MAX] = {0}, es[8] = {0};

            // 'Accion SUMA ES;' lleva ';' en los templates oficiales, y
            // 'FinAccion.' lleva punto. Ninguno aporta nada: se sacan antes de
            // leer el nombre para que no se peguen a la ultima palabra.
            char cab[PAED_LINEA_MAX];
            snprintf(cab, sizeof(cab), "%s", linea);
            for (size_t n = strlen(cab); n > 0 &&
                 (cab[n - 1] == ';' || cab[n - 1] == '.' ||
                  isspace((unsigned char)cab[n - 1])); n = strlen(cab))
                cab[n - 1] = '\0';

            // El literal "ACCION" dentro de sscanf tambien distingue
            // mayusculas. Ya se comprobo arriba con strncasecmp, asi que se
            // saltean esas 6 letras y se lee desde el nombre.
            //
            // El 'ES' es OPCIONAL: 'accion archivo_corte;' es la forma del
            // template CORTE DE CONTROL [TEMPLATE Rev2]. Si viene, tiene que
            // ser 'ES' y no otra palabra — una segunda palabra cualquiera casi
            // siempre es un nombre de accion con espacios, que sigue sin valer.
            int campos = sscanf(cab + 6, "%63s %7s", nombre, es);
            if (campos < 1) {
                add_error(out, lineno, "se esperaba: ACCION <nombre> ES");
                continue;
            }
            if (campos == 2 && !kw_es(es, "ES")) {
                add_error(out, lineno,
                          "se esperaba: ACCION <nombre> ES — '%s' no es 'ES'. "
                          "El nombre de la ACCION no puede llevar espacios", es);
                continue;
            }
            strncpy(out->name, nombre, PAED_NAME_MAX - 1);
            bloque = CABECERA;
            continue;
        }

        // ── Abre una subaccion ──
        //
        // Van declaradas ANTES del PROCESO principal: en la cabecera o dentro
        // del AMBIENTE, que es donde las escriben los parciales.
        const char *kw_sub = abre_subaccion(linea);
        if (kw_sub) {
            if (bloque == SUB_CABECERA || bloque == SUB_AMBIENTE || bloque == SUB_PROCESO) {
                add_error(out, lineno,
                          "falta FIN_%s: la subaccion '%s' de la linea %d quedo abierta "
                          "y aca ya empieza otra",
                          sub && !sub->es_funcion ? "PROCEDIMIENTO" : "FUNCION",
                          sub ? sub->name : "?", sub ? sub->line : 0);
                sub = NULL;
                bloque = bloque_antes_de_sub;
            }
            if (bloque != CABECERA && bloque != AMBIENTE) {
                add_error(out, lineno,
                          "las subacciones van despues de ACCION y ANTES del PROCESO principal");
                continue;
            }
            if (reg) {
                add_error(out, lineno,
                          "falta FIN_REGISTRO: el registro '%s' de la linea %d quedo "
                          "abierto y aca ya empieza una subaccion", reg->name, reg->line);
                reg = NULL;
            }
            bloque_antes_de_sub = bloque;
            decl_base_sub       = out->decl_count;
            sub                 = parse_subaccion_cabecera(out, kw_sub, linea, lineno);
            // Si la cabecera no sirvio igual se entra al estado de subaccion:
            // el motivo ya se reporto, y leer el cuerpo como si fuera del
            // programa principal soltaria un error por cada linea de adentro.
            bloque = SUB_CABECERA;
            continue;
        }

        // ── Cierra una subaccion ──
        // 'Fin;' a secas es de catedra y cierra una subaccion. NO esta en
        // GRAFIAS[] a proposito: ahi se traduciria SIEMPRE, y 'Fin;' tambien
        // cierra la ACCION entera en algunos templates. Se resuelve por
        // CONTEXTO — solo cuenta como cierre de subaccion si hay una abierta.
        int cierra_fin_solo = kw_es(linea, "FIN") &&
                              (bloque == SUB_CABECERA || bloque == SUB_AMBIENTE ||
                               bloque == SUB_PROCESO);

        if (cierra_fin_solo || kw_es(linea, "FIN_FUNCION") ||
            kw_es(linea, "FIN_PROCEDIMIENTO") || kw_es(linea, "FIN_SUBACCION")) {
            if (bloque != SUB_CABECERA && bloque != SUB_AMBIENTE && bloque != SUB_PROCESO) {
                add_error(out, lineno, "'%s' no cierra ninguna subaccion abierta", linea);
                continue;
            }
            // El cierre tiene que decir lo mismo que la apertura. FIN_SUBACCION
            // vale para las dos: es la forma generica de la catedra.
            if (sub && !kw_es(linea, "FIN_SUBACCION") && !cierra_fin_solo) {
                int cierra_funcion = kw_es(linea, "FIN_FUNCION");
                if (cierra_funcion != sub->es_funcion)
                    add_error(out, lineno,
                              "'%s' es %s y se cierra con FIN_%s, no con %s",
                              sub->name,
                              sub->es_funcion ? "una FUNCION" : "un PROCEDIMIENTO",
                              sub->es_funcion ? "FUNCION" : "PROCEDIMIENTO",
                              linea);
            }
            for (int i = pila.tope - 1; i >= 0; i--)
                add_error(out, lineno,
                          "falta FIN_%s: el %s de la linea %d quedo abierto dentro de la subaccion",
                          nombre_kind(pila.items[i].kind), nombre_kind(pila.items[i].kind),
                          pila.items[i].line);
            pila.tope = 0;

            if (sub) {
                if (sub->inicio < 0) {
                    add_error(out, lineno, "la subaccion '%s' no tiene bloque PROCESO", sub->name);
                    sub->inicio = out->instr_count;
                }
                sub->fin = out->instr_count;

                // Las declaraciones que entraron mientras la subaccion estaba
                // abierta son SUS locales: se mudan y se sacan de la tabla del
                // programa principal.
                for (int i = decl_base_sub; i < out->decl_count; i++) {
                    if (sub->local_count >= PAED_MAX_LOCALES) {
                        add_error(out, out->decls[i].line,
                                  "la subaccion '%s' declara mas de %d variables locales",
                                  sub->name, PAED_MAX_LOCALES);
                        break;
                    }
                    sub->locales[sub->local_count++] = out->decls[i];
                }
                out->decl_count = decl_base_sub;
            }

            bloque = bloque_antes_de_sub;
            sub    = NULL;
            continue;
        }

        if (kw_es(linea, "AMBIENTE")) {
            if (bloque == SUB_CABECERA) {
                decl_base_sub = out->decl_count;
                bloque = SUB_AMBIENTE;
                continue;
            }
            if (bloque != CABECERA)
                add_error(out, lineno, "AMBIENTE va justo despues de ACCION ... ES y antes de PROCESO");
            bloque = AMBIENTE;
            continue;
        }

        if (kw_es(linea, "PROCESO")) {
            // El PROCESO de una subaccion: acá empieza SU cuerpo dentro del
            // instrs[] compartido.
            if (bloque == SUB_CABECERA || bloque == SUB_AMBIENTE) {
                if (sub) sub->inicio = out->instr_count;
                bloque = SUB_PROCESO;
                continue;
            }
            if (bloque != CABECERA && bloque != AMBIENTE)
                add_error(out, lineno, "PROCESO fuera de lugar");
            // Donde arranca el programa de verdad. Todo lo que quedo antes en
            // instrs[] es cuerpo de alguna subaccion y no se ejecuta solo.
            out->proceso_inicio = out->instr_count;
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
            for (int i = pila.tope - 1; i >= 0; i--) {
                // El REPETIR no cierra con FIN_REPETIR: cierra con HASTA. Decir
                // "falta FIN_REPETIR" mandaria a escribir una palabra que no
                // existe en el lenguaje.
                if (pila.items[i].kind == PAED_REPETIR)
                    add_error(out, lineno,
                              "falta HASTA: el REPETIR de la linea %d quedo abierto",
                              pila.items[i].line);
                else
                    add_error(out, lineno, "falta FIN_%s: el %s de la linea %d quedo abierto",
                              nombre_kind(pila.items[i].kind),
                              nombre_kind(pila.items[i].kind), pila.items[i].line);
            }
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
            case SUB_AMBIENTE: parse_ambiente(out, linea, lineno, &reg); break;
            case SUB_PROCESO:
                if (!parse_bloque(out, linea, lineno, &pila))
                    parse_sentencias(out, linea, lineno);
                break;
            case SUB_CABECERA:
                add_error(out, lineno,
                          "instruccion antes del PROCESO de la subaccion '%s'",
                          sub ? sub->name : "?");
                break;
        }
    }

    fclose(f);

    // Va con el programa completo en la mano: el ABRIR que decide si un archivo
    // se puede grabar puede estar despues del ESCRIBIR que lo usa, y linea por
    // linea eso no se puede saber.
    chequear_claves(out);
    chequear_modos(out);
    chequear_subacciones(out);
    ordenar_errores(out);

    if (bloque == FUERA)  add_error(out, lineno, "falta ACCION <nombre> ES");
    if (bloque == CABECERA || bloque == AMBIENTE) add_error(out, lineno, "falta PROCESO");
    if (reg) add_error(out, lineno, "falta FIN_REGISTRO: el registro '%s' de la linea %d quedo abierto",
                       reg->name, reg->line);
    if (bloque == PROCESO) add_error(out, lineno, "falta el cierre: FIN_ACCION o FINACCION");
    if (sub) add_error(out, lineno,
                       "falta FIN_%s: la subaccion '%s' de la linea %d nunca se cerro",
                       sub->es_funcion ? "FUNCION" : "PROCEDIMIENTO", sub->name, sub->line);

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

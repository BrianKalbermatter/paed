#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "paed/archivo.h"

static Archivo g_archivos[PAED_MAX_ARCHIVOS];
static int     g_count = 0;
static char    g_error[PAED_MSG_MAX] = {0};

const char *arch_error(void) { return g_error; }

static int falla(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_error, sizeof(g_error), fmt, ap);
    va_end(ap);
    return -1;
}

void arch_reset(void) {
    for (int i = 0; i < g_count; i++)
        if (g_archivos[i].f) fclose(g_archivos[i].f);
    memset(g_archivos, 0, sizeof(g_archivos));
    g_count     = 0;
    g_error[0]  = '\0';
}

Archivo *arch_buscar(const char *nombre) {
    for (int i = 0; i < g_count; i++)
        if (strcasecmp(g_archivos[i].nombre, nombre) == 0) return &g_archivos[i];
    return NULL;
}

Archivo *arch_declarar(const char *nombre, const char *dir) {
    if (g_count >= PAED_MAX_ARCHIVOS) {
        falla("no entran mas archivos abiertos (maximo %d)", PAED_MAX_ARCHIVOS);
        return NULL;
    }
    Archivo *a = &g_archivos[g_count++];
    memset(a, 0, sizeof(*a));
    snprintf(a->nombre, sizeof(a->nombre), "%s", nombre);

    // El .csv va al lado del .paed y se llama como la variable. Un parcial es
    // una carpeta con el programa y sus datos.
    if (dir && *dir) snprintf(a->ruta, sizeof(a->ruta), "%s/%s.csv", dir, nombre);
    else             snprintf(a->ruta, sizeof(a->ruta), "%s.csv", nombre);
    return a;
}

int arch_set_campos(Archivo *a, const char campos[][PAED_NAME_MAX], int n) {
    if (n <= 0)                return falla("el registro de '%s' no tiene campos", a->nombre);
    if (n > PAED_MAX_CAMPOS)   return falla("el registro de '%s' tiene demasiados campos", a->nombre);
    for (int i = 0; i < n; i++)
        snprintf(a->campos[i], PAED_NAME_MAX, "%s", campos[i]);
    a->campo_count = n;
    return 0;
}

// ── CSV ──────────────────────────────────────────────────────────────────────
//
// RFC 4180 con ';' en vez de ',': un campo que contiene el separador, comillas
// o un salto va entre comillas dobles, y las comillas de adentro se duplican.
//
// Las comillas DOBLES quedan libres para esto porque PAED usa la comilla SIMPLE
// para sus textos (§10.4). No se pisan.

static int necesita_comillas(const char *s) {
    if (!*s) return 0;
    if (isspace((unsigned char)s[0]) || isspace((unsigned char)s[strlen(s) - 1])) return 1;
    for (const char *c = s; *c; c++)
        if (*c == PAED_CSV_SEP || *c == '"' || *c == '\n' || *c == '\r') return 1;
    return 0;
}

static void escribir_campo(FILE *f, const char *s) {
    if (!necesita_comillas(s)) { fputs(s, f); return; }

    fputc('"', f);
    for (const char *c = s; *c; c++) {
        if (*c == '"') fputc('"', f);   // la de adentro se duplica
        fputc(*c, f);
    }
    fputc('"', f);
}

// Parte una linea de CSV en campos. Devuelve cuantos encontro.
//
// El '\r' final se descarta: un archivo guardado en Windows termina cada linea
// con \r\n, y ese \r invisible convertiria el numero 12 en el texto "12\r". El
// bug seria mudo y la culpa se la llevaria el interprete — es la misma trampa
// que ya habia mordido en el LEER de consola.
static int partir_csv(const char *linea, char valores[][PAED_VAL_MAX], int max) {
    int n = 0;
    const char *c = linea;

    while (n < max) {
        char  *dst = valores[n];
        size_t k   = 0;
        int    entre_comillas = 0;

        if (*c == '"') { entre_comillas = 1; c++; }

        while (*c) {
            if (entre_comillas) {
                if (*c == '"') {
                    if (c[1] == '"') { if (k < PAED_VAL_MAX - 1) dst[k++] = '"'; c += 2; continue; }
                    entre_comillas = 0; c++; continue;
                }
            } else {
                if (*c == PAED_CSV_SEP) break;
                if (*c == '\n' || *c == '\r') break;
            }
            if (k < PAED_VAL_MAX - 1) dst[k++] = *c;
            c++;
        }
        dst[k] = '\0';
        n++;

        if (*c != PAED_CSV_SEP) break;
        c++;
    }
    return n;
}

// ── Abrir, crear, cerrar ─────────────────────────────────────────────────────

static int ya_abierto(const Archivo *a) {
    return a->abierto && !a->cerrado;
}

int arch_crear(Archivo *a, const char *modo) {
    if (ya_abierto(a)) return falla("el archivo '%s' ya esta abierto", a->nombre);
    if (a->campo_count == 0)
        return falla("'%s' no se puede crear: su tipo no es un REGISTRO declarado", a->nombre);

    // "w" PISA el que exista. Es a proposito: en la actualizacion secuencial el
    // maestro nuevo se crea en cada corrida, y fallar la segunda vez obligaria
    // a borrarlo a mano entre corrida y corrida.
    FILE *f = fopen(a->ruta, "w");
    if (!f) return falla("no se pudo crear '%s'", a->ruta);

    for (int i = 0; i < a->campo_count; i++) {
        if (i) fputc(PAED_CSV_SEP, f);
        escribir_campo(f, a->campos[i]);
    }
    fputc('\n', f);

    a->f       = f;
    a->abierto = 1;
    a->cerrado = 0;
    a->fin     = 0;
    snprintf(a->modo, sizeof(a->modo), "%s", modo ? modo : "S");
    return 0;
}

int arch_abrir(Archivo *a, const char *modo) {
    if (ya_abierto(a)) return falla("el archivo '%s' ya esta abierto", a->nombre);
    if (a->campo_count == 0)
        return falla("'%s' no se puede abrir: su tipo no es un REGISTRO declarado", a->nombre);

    FILE *f = fopen(a->ruta, "r");
    if (!f)
        return falla("no existe el archivo '%s': hay que crearlo con CREAR(%s) antes de abrirlo",
                     a->ruta, a->nombre);

    // El encabezado se VALIDA. Sin esto, abrir el archivo equivocado se lee sin
    // protestar y devuelve basura con forma de dato valido — el programa no
    // falla, da mal, que es mucho peor.
    char linea[PAED_CSV_LINEA];
    if (!fgets(linea, sizeof(linea), f)) {
        fclose(f);
        return falla("'%s' esta vacio: le falta la fila de encabezado", a->ruta);
    }

    char cab[PAED_MAX_CAMPOS][PAED_VAL_MAX];
    int  n = partir_csv(linea, cab, PAED_MAX_CAMPOS);

    if (n != a->campo_count) {
        fclose(f);
        return falla("'%s' tiene %d columnas y el registro de '%s' tiene %d campos",
                     a->ruta, n, a->nombre, a->campo_count);
    }
    for (int i = 0; i < n; i++) {
        if (strcasecmp(cab[i], a->campos[i]) == 0) continue;
        fclose(f);
        return falla("'%s' no es el archivo de '%s': la columna %d se llama '%s' "
                     "y el registro dice '%s'",
                     a->ruta, a->nombre, i + 1, cab[i], a->campos[i]);
    }

    a->f       = f;
    a->abierto = 1;
    a->cerrado = 0;
    a->fin     = 0;
    snprintf(a->modo, sizeof(a->modo), "%s", modo ? modo : "E");
    return 0;
}

int arch_cerrar(Archivo *a) {
    if (!ya_abierto(a)) return falla("el archivo '%s' no esta abierto", a->nombre);
    fclose(a->f);
    a->f       = NULL;
    a->cerrado = 1;
    a->abierto = 0;
    return 0;
}

// ── Leer y grabar ────────────────────────────────────────────────────────────

int arch_leer(Archivo *a, char valores[][PAED_VAL_MAX], int *hay) {
    *hay = 0;
    if (!ya_abierto(a)) return falla("hay que abrir '%s' antes de leerlo", a->nombre);

    // Leer despues del fin es un error, no un dato mas: el bucle corta con
    // NFDA, asi que un LEER de mas significa que el corte esta mal escrito.
    // Devolver algo inventado dejaria el sintoma tres instrucciones mas abajo,
    // sin relacion aparente con la causa. Misma regla que AVZ en secuencias.
    if (a->fin) return falla("'%s' ya se termino: hay un LEER despues de que FDA(%s) "
                             "quedo en verdadero", a->nombre, a->nombre);

    char linea[PAED_CSV_LINEA];
    if (!fgets(linea, sizeof(linea), a->f)) { a->fin = 1; return 0; }

    // Una linea en blanco al final del archivo es lo que deja cualquier editor
    // al guardar. No es un registro vacio: es el fin.
    char *c = linea;
    while (*c == '\n' || *c == '\r') c++;
    if (!*c) { a->fin = 1; return 0; }

    int n = partir_csv(linea, valores, a->campo_count);
    if (n != a->campo_count)
        return falla("en '%s' hay una fila con %d columnas y el registro tiene %d campos",
                     a->ruta, n, a->campo_count);

    *hay = 1;
    return 0;
}

int arch_grabar(Archivo *a, const char valores[][PAED_VAL_MAX]) {
    if (!ya_abierto(a)) return falla("hay que abrir '%s' antes de grabarlo", a->nombre);

    for (int i = 0; i < a->campo_count; i++) {
        if (i) fputc(PAED_CSV_SEP, a->f);
        escribir_campo(a->f, valores[i]);
    }
    fputc('\n', a->f);
    return 0;
}

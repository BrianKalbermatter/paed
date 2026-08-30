#include "paed/secuencia.h"

#include "paed/plataforma.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>   // strcasecmp: los tipos no distinguen mayusculas
#include <ctype.h>

// Tabla plana, sin malloc, como el resto del proyecto.
static Secuencia g_secs[PAED_MAX_SECUENCIAS];
static int       g_count = 0;

static char g_error[PAED_MSG_MAX];

const char *sec_error(void) { return g_error; }

static int falla(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_error, sizeof(g_error), fmt, ap);
    va_end(ap);
    return -1;
}

// ── Donde viven los datos ───────────────────────────────────────────────────
//
// El porque de la convencion esta en secuencia.h. Aca solo se arma la ruta y
// se mueve el texto.

// El nombre del programa: el del archivo, sin carpeta y sin ".paed".
//
// Se usa el nombre del ARCHIVO y no el de la ACCION a proposito: el archivo es
// lo que se ve en la carpeta y lo que el editor tiene abierto, y una ACCION se
// puede renombrar sin que nadie se entere de que sus cintas quedaron
// huerfanas.
static void nombre_programa(const char *path_paed, char *out, size_t n) {
    const char *base = strrchr(path_paed, '/');
#ifdef _WIN32
    const char *contra = strrchr(path_paed, '\\');
    if (contra > base) base = contra;
#endif
    base = base ? base + 1 : path_paed;

    snprintf(out, n, "%s", base);
    char *punto = strrchr(out, '.');
    if (punto && punto != out) *punto = '\0';
}

static void carpeta_datos(const char *path_paed, char *out, size_t n) {
    char prog[PAED_NAME_MAX * 2];
    nombre_programa(path_paed, prog, sizeof(prog));

    const char *barra = strrchr(path_paed, '/');
#ifdef _WIN32
    const char *contra = strrchr(path_paed, '\\');
    if (contra > barra) barra = contra;
#endif
    if (barra) {
        int dir = (int)(barra - path_paed);
        snprintf(out, n, "%.*s/%s/%s", dir, path_paed, PAED_DIR_SECUENCIAS, prog);
    } else {
        snprintf(out, n, "%s/%s", PAED_DIR_SECUENCIAS, prog);
    }
}

void sec_ruta_datos(const char *path_paed, const char *nombre,
                    char *out, size_t n) {
    char dir[512];
    carpeta_datos(path_paed, dir, sizeof(dir));
    snprintf(out, n, "%s/%s.txt", dir, nombre);
}

int sec_leer_datos(const char *path_paed, const char *nombre,
                   char *buf, size_t n) {
    if (!path_paed || !nombre || !buf || n == 0) return -1;

    char ruta[600];
    sec_ruta_datos(path_paed, nombre, ruta, sizeof(ruta));

    FILE *f = fopen(ruta, "r");
    if (!f) return -1;

    size_t leidos = fread(buf, 1, n - 1, f);
    fclose(f);
    buf[leidos] = '\0';

    // Solo los saltos del FINAL: el archivo termina en '\n' porque lo escribio
    // un editor, no porque la cinta tenga uno.
    while (leidos > 0 && (buf[leidos - 1] == '\n' || buf[leidos - 1] == '\r'))
        buf[--leidos] = '\0';

    return 0;
}

int sec_guardar_datos(const char *path_paed, const char *nombre,
                      const char *datos) {
    if (!path_paed || !nombre || !datos) return -1;

    // Las DOS carpetas: secuencias_paed/ y la del programa adentro. paed_mkdir
    // es de un solo nivel, y crear la de adentro sin la de afuera falla.
    char dir[512];
    carpeta_datos(path_paed, dir, sizeof(dir));

    char *ultima = strrchr(dir, '/');
    if (ultima) {
        *ultima = '\0';
        paed_mkdir(dir);          // secuencias_paed/
        *ultima = '/';
    }
    paed_mkdir(dir);              // secuencias_paed/<programa>/

    char ruta[600];
    sec_ruta_datos(path_paed, nombre, ruta, sizeof(ruta));

    FILE *f = fopen(ruta, "w");
    if (!f) return -1;

    // Un '\n' al final y nada mas: la cinta es una linea, y el salto es para
    // que el archivo se pueda abrir con cualquier editor sin que se queje.
    fprintf(f, "%s\n", datos);
    fclose(f);
    return 0;
}

void sec_reset(void) {
    g_count = 0;
    g_error[0] = '\0';
    memset(g_secs, 0, sizeof(g_secs));
}

Secuencia *sec_buscar(const char *nombre) {
    for (int i = 0; i < g_count; i++)
        if (strcmp(g_secs[i].nombre, nombre) == 0) return &g_secs[i];
    return NULL;
}

Secuencia *sec_declarar(const char *nombre, const char *tipo_base, int es_salida) {
    if (g_count >= PAED_MAX_SECUENCIAS) {
        falla("no entran mas secuencias (maximo %d)", PAED_MAX_SECUENCIAS);
        return NULL;
    }

    Secuencia *s = &g_secs[g_count++];
    memset(s, 0, sizeof(*s));
    snprintf(s->nombre, sizeof(s->nombre), "%s", nombre);
    s->es_salida = es_salida;

    // CARACTER y CARACTERES son el mismo tipo: el corpus escribe las dos, y
    // comparar con strncasecmp los 8 primeros caracteres las cubre a las dos
    // sin tener que mantener una tabla de plurales.
    s->de_caracteres = (strncasecmp(tipo_base, "CARACTER", 8) == 0);
    return s;
}

int sec_cargar(Secuencia *s, const char *datos) {
    size_t n = strlen(datos);
    if (n >= PAED_SEC_MAX)
        return falla("los datos de la secuencia '%s' no entran (maximo %d caracteres)",
                     s->nombre, PAED_SEC_MAX - 1);

    memcpy(s->datos, datos, n + 1);
    s->largo = (int)n;
    s->pos   = 0;
    return 0;
}

int sec_arrancar(Secuencia *s) {
    if (s->es_salida)
        return falla("'%s' es una SECUENCIA DE SALIDA: se abre con CREAR, no con ARR", s->nombre);

    // Volver a arrancar una secuencia ya recorrida es legitimo: la deja lista
    // para un segundo pasada desde el principio.
    s->pos     = 0;
    s->abierta = 1;
    s->cerrada = 0;
    s->fin     = 0;
    return 0;
}

int sec_crear(Secuencia *s) {
    if (!s->es_salida)
        return falla("CREAR es para una SECUENCIA DE SALIDA, y '%s' es de entrada: "
                     "para recorrerla va ARR(%s)", s->nombre, s->nombre);

    s->largo    = 0;
    s->datos[0] = '\0';
    s->abierta  = 1;
    s->cerrada  = 0;
    return 0;
}

int sec_cerrar(Secuencia *s) {
    if (!s->abierta)
        return falla("'%s' nunca se abrio: falta %s(%s)",
                     s->nombre, s->es_salida ? "CREAR" : "ARR", s->nombre);
    s->cerrada = 1;
    s->abierta = 0;
    return 0;
}

int sec_avanzar(Secuencia *s, char *buf, size_t n, int *hay) {
    *hay = 0;

    if (s->es_salida)
        return falla("'%s' es una SECUENCIA DE SALIDA: se le graba con ESCRIBIR, no se avanza",
                     s->nombre);
    if (!s->abierta)
        return falla("hay que arrancar '%s' antes de avanzarla: falta ARR(%s)",
                     s->nombre, s->nombre);

    // Ya estaba en FDS y se avanzo igual. Es un error del programa: el bucle
    // se paso del final. Dejarlo pasar devolveria un dato inventado y el
    // sintoma aparecería mucho despues, sin relacion aparente con la causa.
    if (s->fin)
        return falla("'%s' ya llego al fin de secuencia: NFDS(%s) era falso antes de este AVZ",
                     s->nombre, s->nombre);

    if (s->de_caracteres) {
        // Un elemento = un caracter, TAL CUAL, espacios incluidos: en
        // 'Perez Juan#' el espacio del medio es parte del nombre. Los saltos
        // de linea si se saltean, porque son del formato del archivo de datos
        // y no de la secuencia.
        while (s->pos < s->largo &&
               (s->datos[s->pos] == '\n' || s->datos[s->pos] == '\r'))
            s->pos++;

        if (s->pos >= s->largo) { s->fin = 1; return 0; }

        if (n < 2) return falla("no hay lugar para el elemento de '%s'", s->nombre);
        buf[0] = s->datos[s->pos++];
        buf[1] = '\0';
        *hay = 1;
        return 0;
    }

    // Un elemento = un token separado por espacios. Es la forma de las
    // secuencias de numeros: "101 1 204 2" son cuatro elementos.
    while (s->pos < s->largo && isspace((unsigned char)s->datos[s->pos])) s->pos++;
    if (s->pos >= s->largo) { s->fin = 1; return 0; }

    size_t k = 0;
    while (s->pos < s->largo && !isspace((unsigned char)s->datos[s->pos])) {
        if (k < n - 1) buf[k++] = s->datos[s->pos];
        s->pos++;
    }
    buf[k] = '\0';
    *hay = 1;
    return 0;
}

int sec_grabar(Secuencia *s, const char *texto) {
    if (!s->es_salida)
        return falla("'%s' es una secuencia de entrada: no se le puede grabar", s->nombre);
    if (!s->abierta)
        return falla("hay que crear '%s' antes de grabarle: falta CREAR(%s)",
                     s->nombre, s->nombre);

    size_t n = strlen(texto);
    if (s->largo + (int)n >= PAED_SEC_MAX)
        return falla("la secuencia de salida '%s' se lleno (maximo %d caracteres)",
                     s->nombre, PAED_SEC_MAX - 1);

    memcpy(s->datos + s->largo, texto, n + 1);
    s->largo += (int)n;
    return 0;
}

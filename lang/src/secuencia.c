#include "paed/secuencia.h"

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

// Las subacciones: FUNCION y PROCEDIMIENTO.
//
//     FUNCION sumar(a, b: ENTERO): ENTERO
//     PROCEDIMIENTO saludar(nombre: AN(20))
//
// Reconocer que una linea abre una subaccion, parsear sus parametros y anotarla
// en el programa. Si una que se llamo desde otro lado existe o no NO se decide
// aca: eso necesita el archivo entero leido y vive en chequeos.c.

#include "subacciones.h"
#include "reporte.h"
#include "texto.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

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
void parse_param(PAEDProgram *p, PAEDSubaccion *sub, char *texto, int lineno) {
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
const char *abre_subaccion(const char *linea) {
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
PAEDSubaccion *parse_subaccion_cabecera(PAEDProgram *p, const char *kw,
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

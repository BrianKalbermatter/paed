#include "paed/expr.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── Entorno ───────────────────────────────────────────────────

void env_init(Entorno *e) {
    memset(e, 0, sizeof(*e));
}

// Busca la ENTRADA de la tabla, no su valor: env_buscar devuelve el escalar, y
// para un arreglo eso no sirve (hay que llegar a los limites y al offset).
static Variable *env_entrada(Entorno *e, const char *nombre) {
    for (int i = 0; i < e->count; i++)
        if (strcmp(e->items[i].nombre, nombre) == 0)
            return &e->items[i];
    return NULL;
}

Valor *env_buscar(Entorno *e, const char *nombre) {
    Variable *var = env_entrada(e, nombre);
    // Un arreglo usado sin corchetes no tiene un valor escalar que devolver.
    // Se avisa acá y no más adelante con un valor inventado en 0.
    if (var && var->es_arreglo) {
        snprintf(e->error, PAED_MSG_MAX,
                 "'%s' es un arreglo: falta el indice, se esperaba %s[i]", nombre, nombre);
        return NULL;
    }
    return var ? &var->valor : NULL;
}

int env_set(Entorno *e, const char *nombre, Valor v) {
    Variable *ya = env_entrada(e, nombre);
    if (ya) {
        if (ya->es_arreglo) {
            snprintf(e->error, PAED_MSG_MAX,
                     "'%s' es un arreglo: no se le puede asignar de una sola vez", nombre);
            return -1;
        }
        ya->valor = v;
        return 0;
    }
    if (e->count >= PAED_MAX_VARS) return -1;

    Variable *nueva = &e->items[e->count++];
    memset(nueva, 0, sizeof(*nueva));
    snprintf(nueva->nombre, PAED_NAME_MAX, "%s", nombre);
    nueva->valor = v;
    return 0;
}

int env_existe(Entorno *e, const char *nombre) {
    return env_entrada(e, nombre) != NULL;
}

int env_declarar_arreglo(Entorno *e, const char *nombre, int desde, int hasta) {
    if (hasta < desde) {
        snprintf(e->error, PAED_MSG_MAX,
                 "el arreglo '%s' tiene los limites al reves: [%d..%d]", nombre, desde, hasta);
        return -1;
    }
    int largo = hasta - desde + 1;

    if (e->count >= PAED_MAX_VARS) {
        snprintf(e->error, PAED_MSG_MAX, "no entran mas variables");
        return -1;
    }
    if (e->pool_usado + largo > PAED_MAX_ELEMS) {
        snprintf(e->error, PAED_MSG_MAX,
                 "no entran los %d elementos de '%s' (quedan %d de %d)",
                 largo, nombre, PAED_MAX_ELEMS - e->pool_usado, PAED_MAX_ELEMS);
        return -1;
    }

    Variable *var = &e->items[e->count++];
    memset(var, 0, sizeof(*var));
    snprintf(var->nombre, PAED_NAME_MAX, "%s", nombre);
    var->es_arreglo = 1;
    var->desde      = desde;
    var->hasta      = hasta;
    var->off        = e->pool_usado;
    e->pool_usado  += largo;

    // Arrancan en 0 y no en basura: leer A[3] antes de cargarlo tiene que dar
    // algo previsible, no lo que hubiera quedado en esa memoria.
    for (int i = 0; i < largo; i++) {
        memset(&e->pool[var->off + i], 0, sizeof(Valor));
        e->pool[var->off + i].tipo = VAL_NUM;
    }
    return 0;
}

Valor *env_elem(Entorno *e, const char *nombre, int indice) {
    Variable *var = env_entrada(e, nombre);
    if (!var) {
        snprintf(e->error, PAED_MSG_MAX, "el arreglo '%s' no esta declarado", nombre);
        return NULL;
    }
    if (!var->es_arreglo) {
        snprintf(e->error, PAED_MSG_MAX, "'%s' no es un arreglo, no se puede indexar", nombre);
        return NULL;
    }
    // El chequeo de limites es LO que hace util a un arreglo con rango
    // declarado. En C, A[99] sobre un arreglo de 10 pisa memoria ajena en
    // silencio; acá se corta con el indice y los limites a la vista.
    if (indice < var->desde || indice > var->hasta) {
        snprintf(e->error, PAED_MSG_MAX,
                 "indice %d fuera de rango: '%s' va de %d a %d",
                 indice, nombre, var->desde, var->hasta);
        return NULL;
    }
    return &e->pool[var->off + (indice - var->desde)];
}

int valor_verdadero(const Valor *v) {
    switch (v->tipo) {
        case VAL_LOGICO: return v->logico != 0;
        case VAL_NUM:    return v->num != 0.0;
        case VAL_TEXTO:  return v->texto[0] != '\0';
    }
    return 0;
}

void valor_a_texto(const Valor *v, char *out, size_t out_size) {
    switch (v->tipo) {
        case VAL_TEXTO:
            snprintf(out, out_size, "%s", v->texto);
            break;
        case VAL_LOGICO:
            snprintf(out, out_size, "%s", v->logico ? "V" : "F");
            break;
        case VAL_NUM:
            // Un entero se muestra sin coma: 4 y no 4.000000. En AED la
            // division real y la entera son operadores distintos, asi que el
            // usuario ya sabe cual pidio.
            if (v->num == floor(v->num) && fabs(v->num) < 1e15)
                snprintf(out, out_size, "%.0f", v->num);
            else
                snprintf(out, out_size, "%g", v->num);
            break;
    }
}

// ── Contexto del recorrido ────────────────────────────────────

typedef struct {
    const char *p;
    Entorno    *env;
    int         fallo;
} Ctx;

static Valor NUM(double n)   { Valor v = {0}; v.tipo = VAL_NUM;    v.num = n;    return v; }
static Valor LOG(int b)      { Valor v = {0}; v.tipo = VAL_LOGICO; v.logico = !!b; return v; }

static void falla(Ctx *c, const char *fmt, ...) {
    if (c->fallo) return;          // se queda con el PRIMER error, que es el util
    c->fallo = 1;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(c->env->error, PAED_MSG_MAX, fmt, ap);
    va_end(ap);
}

static void espacios(Ctx *c) {
    while (*c->p && isspace((unsigned char)*c->p)) c->p++;
}

// Consume un simbolo si esta. Los mas largos se prueban primero desde el que
// llama: si '<' se probara antes que '<=', "a <= b" se leeria como "a < (= b)".
static int simbolo(Ctx *c, const char *s) {
    espacios(c);
    size_t n = strlen(s);
    if (strncmp(c->p, s, n) != 0) return 0;
    c->p += n;
    return 1;
}

// Consume una palabra clave (Y, O, NO, DIV, MOD...) solo si esta suelta:
// sin esto, la variable 'Ymax' empezaria con el operador Y.
static int palabra(Ctx *c, const char *kw) {
    espacios(c);
    size_t n = strlen(kw);
    if (strncasecmp(c->p, kw, n) != 0) return 0;
    char sig = c->p[n];
    if (isalnum((unsigned char)sig) || sig == '_') return 0;
    c->p += n;
    return 1;
}

// ── Numeros y textos ──────────────────────────────────────────

static Valor eval_o(Ctx *c);   // el nivel mas bajo, para los parentesis

static Valor primario(Ctx *c) {
    espacios(c);

    if (*c->p == '\0') { falla(c, "la expresion termina antes de tiempo"); return NUM(0); }

    // ( ... )
    if (*c->p == '(') {
        c->p++;
        Valor v = eval_o(c);
        espacios(c);
        if (*c->p != ')') { falla(c, "falta ')' en la expresion"); return v; }
        c->p++;
        return v;
    }

    // Texto: "hola" o 'a'
    if (*c->p == '"' || *c->p == '\'') {
        char comilla = *c->p++;
        Valor v = {0};
        v.tipo = VAL_TEXTO;
        size_t n = 0;
        while (*c->p && *c->p != comilla) {
            if (n < PAED_VAL_MAX - 1) v.texto[n++] = *c->p;
            c->p++;
        }
        if (*c->p != comilla) { falla(c, "falta la comilla de cierre"); return v; }
        c->p++;
        v.texto[n] = '\0';
        return v;
    }

    // Numero
    if (isdigit((unsigned char)*c->p) ||
        (*c->p == '.' && isdigit((unsigned char)c->p[1]))) {
        char *fin = NULL;
        double n = strtod(c->p, &fin);
        c->p = fin;
        return NUM(n);
    }

    // Identificador: literal logico, funcion o variable
    if (isalpha((unsigned char)*c->p) || *c->p == '_') {
        char nombre[PAED_NAME_MAX];
        size_t n = 0;
        while (isalnum((unsigned char)*c->p) || *c->p == '_') {
            if (n < PAED_NAME_MAX - 1) nombre[n++] = *c->p;
            c->p++;

            // Acceso a campo de registro: 'pori.vx' es UN nombre, no dos.
            // El campo aparece en el entorno como "pori.vx", asi que alcanza
            // con dejar que el punto forme parte del nombre.
            //
            // Un numero como 1.5 nunca llega aca: se lee mas arriba, porque
            // empieza con digito. Y se exige letra despues del punto, asi que
            // 'pori.' o 'pori.1' cortan el nombre en vez de tragarse el punto.
            if (*c->p == '.' && (isalpha((unsigned char)c->p[1]) || c->p[1] == '_')) {
                if (n < PAED_NAME_MAX - 1) nombre[n++] = '.';
                c->p++;
            }
        }
        nombre[n] = '\0';

        // Literales logicos (TEORIA_COMPLETA.txt:350-356)
        if (strcmp(nombre, "V") == 0 || strcasecmp(nombre, "VERDADERO") == 0) return LOG(1);
        if (strcmp(nombre, "F") == 0 || strcasecmp(nombre, "FALSO")     == 0) return LOG(0);

        espacios(c);

        // A[i] — el indice es una EXPRESION completa, no solo un numero: asi
        // valen A[i], A[i+1] y A[med] sin ningun caso especial.
        if (*c->p == '[') {
            c->p++;
            Valor idx = eval_o(c);
            espacios(c);
            if (*c->p != ']') { falla(c, "falta ']' al indexar %s", nombre); return NUM(0); }
            c->p++;
            if (c->fallo) return NUM(0);

            if (idx.tipo != VAL_NUM) {
                falla(c, "el indice de %s tiene que ser un numero", nombre);
                return NUM(0);
            }
            // Un indice con coma es un error del programa, no algo para
            // redondear por atras: A[2.5] no existe.
            if (idx.num != floor(idx.num)) {
                falla(c, "el indice de %s no puede tener decimales (%g)", nombre, idx.num);
                return NUM(0);
            }

            Valor *elem = env_elem(c->env, nombre, (int)idx.num);
            if (!elem) {
                // El motivo ya esta en env->error, pero NO se puede pasar
                // directo a falla(): adentro hace vsnprintf SOBRE ese mismo
                // buffer, y escribir y leer el mismo string a la vez es
                // comportamiento indefinido (deja el mensaje vacio). Se copia.
                char motivo[PAED_MSG_MAX];
                snprintf(motivo, sizeof(motivo), "%s", c->env->error);
                falla(c, "%s", motivo);
                return NUM(0);
            }
            return *elem;
        }

        if (*c->p == '(') {   // llamada a funcion
            c->p++;

            // Las funciones que preguntan por algo que el interprete todavia
            // no tiene se resuelven ANTES de mirar el argumento. Si no, el
            // argumento se evalua primero y el error que sale es el suyo: en
            // `FDA(arch)`, 'arch' es un archivo y no una variable con valor,
            // asi que salia "la variable 'arch' no tiene valor todavia" y el
            // nombre de la funcion no aparecia por ningun lado.
            //
            // El cursor igual tiene que avanzar hasta el ')', asi que se
            // recorre el argumento con el contexto marcado como fallado —
            // el mismo truco que usa el cortocircuito de Y y O.
            int sin_soporte =
                strcmp(nombre, "FDS")  == 0 || strcmp(nombre, "NFDS") == 0 ||
                strcmp(nombre, "FDA")  == 0 || strcmp(nombre, "NFDA") == 0;

            if (sin_soporte) {
                espacios(c);
                if (*c->p != ')') {
                    Ctx basura = *c;
                    basura.fallo = 1;
                    eval_o(&basura);
                    c->p = basura.p;
                }
                espacios(c);
                if (*c->p == ')') c->p++;

                int de_archivo = strcmp(nombre, "FDA") == 0 || strcmp(nombre, "NFDA") == 0;
                falla(c, "%s() necesita %s, que todavia no existen en el interprete",
                      nombre, de_archivo ? "archivos" : "secuencias");
                return LOG(0);
            }

            Valor arg = {0};
            espacios(c);
            int hay_arg = (*c->p != ')');
            if (hay_arg) arg = eval_o(c);
            espacios(c);
            if (*c->p != ')') { falla(c, "falta ')' al cerrar %s", nombre); return NUM(0); }
            c->p++;

            if (strcmp(nombre, "TRUNC")  == 0) return NUM(trunc(arg.num));
            if (strcmp(nombre, "ABSO")   == 0) return NUM(fabs(arg.num));
            if (strcmp(nombre, "REDOND") == 0) return NUM(round(arg.num));

            falla(c, "funcion desconocida '%s'", nombre);
            return NUM(0);
        }

        Valor *v = env_buscar(c->env, nombre);
        if (!v) {
            // env_buscar deja un motivo mejor cuando el nombre SI existe pero
            // es un arreglo usado sin indice. Solo si no dijo nada se cae al
            // mensaje generico. Se copia antes de pasarlo: falla() escribe en
            // ese mismo buffer (ver el comentario del indexado, mas arriba).
            if (c->env->error[0]) {
                char motivo[PAED_MSG_MAX];
                snprintf(motivo, sizeof(motivo), "%s", c->env->error);
                falla(c, "%s", motivo);
            } else {
                falla(c, "la variable '%s' no tiene valor todavia", nombre);
            }
            return NUM(0);
        }
        return *v;
    }

    falla(c, "no entiendo '%c' en la expresion", *c->p);
    return NUM(0);
}

// ── Prioridad 1 y 2: unarios y potencia ───────────────────────

static Valor unario(Ctx *c) {
    espacios(c);
    if (palabra(c, "NO")) { Valor v = unario(c); return LOG(!valor_verdadero(&v)); }
    if (simbolo(c, "-"))  { Valor v = unario(c); return NUM(-v.num); }
    if (simbolo(c, "+"))  { return unario(c); }
    return primario(c);
}

// ** es asociativa a DERECHA: 2**3**2 es 2**(3**2), no (2**3)**2.
// Por eso el lado derecho se llama a si mismo y no al nivel de abajo.
static Valor potencia(Ctx *c) {
    Valor base = unario(c);
    if (simbolo(c, "**")) {
        Valor exp = potencia(c);
        return NUM(pow(base.num, exp.num));
    }
    return base;
}

// ── Prioridad 3: * / DIV MOD ──────────────────────────────────

static Valor producto(Ctx *c) {
    Valor izq = potencia(c);
    for (;;) {
        espacios(c);
        // Se mira sin consumir: un '*' seguido de otro '*' es potencia y le
        // toca a potencia(), no aca.
        if (c->p[0] == '*' && c->p[1] != '*') {
            c->p++;
            Valor der = potencia(c);
            izq = NUM(izq.num * der.num);
        } else if (simbolo(c, "/")) {
            Valor der = potencia(c);
            if (der.num == 0.0) { falla(c, "division por cero"); return NUM(0); }
            izq = NUM(izq.num / der.num);
        } else if (palabra(c, "DIV")) {
            Valor der = potencia(c);
            if (der.num == 0.0) { falla(c, "division entera por cero"); return NUM(0); }
            izq = NUM(trunc(izq.num / der.num));
        } else if (palabra(c, "MOD")) {
            Valor der = potencia(c);
            if (der.num == 0.0) { falla(c, "MOD por cero"); return NUM(0); }
            izq = NUM(fmod(izq.num, der.num));
        } else {
            return izq;
        }
        if (c->fallo) return izq;
    }
}

// ── Prioridad 4 y 5: suma, resta y concatenacion ──────────────

static Valor suma(Ctx *c) {
    Valor izq = producto(c);
    for (;;) {
        espacios(c);
        if (simbolo(c, "+")) {
            Valor der = producto(c);
            // El '+' tambien concatena (prioridad 5 de la teoria). Se decide
            // por el tipo: si alguno es texto, se pegan.
            if (izq.tipo == VAL_TEXTO || der.tipo == VAL_TEXTO) {
                char a[PAED_VAL_MAX], b[PAED_VAL_MAX];
                valor_a_texto(&izq, a, sizeof(a));
                valor_a_texto(&der, b, sizeof(b));
                Valor v = {0};
                v.tipo = VAL_TEXTO;
                snprintf(v.texto, PAED_VAL_MAX, "%s%s", a, b);
                izq = v;
            } else {
                izq = NUM(izq.num + der.num);
            }
        } else if (simbolo(c, "-")) {
            Valor der = producto(c);
            izq = NUM(izq.num - der.num);
        } else {
            return izq;
        }
        if (c->fallo) return izq;
    }
}

// ── Prioridad 6 y 7: relacionales ─────────────────────────────

// Compara dos valores. Los textos van por ASCII, como dice la teoria:
// 'A' < 'K' es verdadero. Devuelve <0, 0 o >0.
static int comparar(const Valor *a, const Valor *b) {
    if (a->tipo == VAL_TEXTO || b->tipo == VAL_TEXTO) {
        char x[PAED_VAL_MAX], y[PAED_VAL_MAX];
        valor_a_texto(a, x, sizeof(x));
        valor_a_texto(b, y, sizeof(y));
        return strcmp(x, y);
    }
    double x = (a->tipo == VAL_LOGICO) ? a->logico : a->num;
    double y = (b->tipo == VAL_LOGICO) ? b->logico : b->num;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

// Prioridad 6: < <= > >=
static Valor relacional(Ctx *c) {
    Valor izq = suma(c);
    for (;;) {
        espacios(c);
        // Se MIRA antes de consumir. Dos trampas:
        //  - con '<' antes que '<=', "a <= b" se leeria "a < (= b)"
        //  - '<>' es de otro nivel (igualdad), asi que un '<' seguido de '>'
        //    NO es "menor que" y hay que dejarlo pasar
        int op = 0;
        if      (c->p[0] == '<' && c->p[1] == '=') { c->p += 2; op = 1; }
        else if (c->p[0] == '>' && c->p[1] == '=') { c->p += 2; op = 2; }
        else if (c->p[0] == '<' && c->p[1] != '>') { c->p += 1; op = 3; }
        else if (c->p[0] == '>')                   { c->p += 1; op = 4; }
        else return izq;

        Valor der = suma(c);
        int   cmp = comparar(&izq, &der);
        izq = LOG(op == 1 ? cmp <= 0 :
                  op == 2 ? cmp >= 0 :
                  op == 3 ? cmp <  0 :
                            cmp >  0);
        if (c->fallo) return izq;
    }
}

// Prioridad 7: = <>
// El '==' NO esta en la teoria (que solo define '='), pero aparece en los
// ejercicios del usuario ("SI (recorrido == P) ENTONCES"), asi que se acepta
// como sinonimo en vez de romperle los archivos. Queda anotado en el KANBAN.
static Valor igualdad(Ctx *c) {
    Valor izq = relacional(c);
    for (;;) {
        espacios(c);
        int distinto;
        if      (simbolo(c, "<>")) distinto = 1;
        else if (simbolo(c, "==")) distinto = 0;
        else if (simbolo(c, "="))  distinto = 0;
        else return izq;

        Valor der = relacional(c);
        int   cmp = comparar(&izq, &der);
        izq = LOG(distinto ? cmp != 0 : cmp == 0);
        if (c->fallo) return izq;
    }
}

// ── Prioridad 8 y 9: Y y O, con cortocircuito ─────────────────

static Valor eval_y(Ctx *c) {
    Valor izq = igualdad(c);
    for (;;) {
        espacios(c);
        if (!palabra(c, "Y") && !palabra(c, "AND")) return izq;

        // CORTOCIRCUITO. TEORIA_COMPLETA.txt dice textual: "En AND, si el
        // primer operando es Falso, el segundo no se evalua". No es una
        // optimizacion: cambia el comportamiento. Con `i > 0 Y A[i] = 3`, si
        // no cortara, evaluar A[i] con i invalido seria un error de verdad.
        if (!valor_verdadero(&izq)) {
            Ctx basura = *c;          // se recorre el lado derecho para
            basura.fallo = 1;         // avanzar el cursor, pero sin ejecutar
            igualdad(&basura);        // ni reportar errores de ahi
            c->p = basura.p;
            izq = LOG(0);
            continue;
        }
        Valor der = igualdad(c);
        izq = LOG(valor_verdadero(&der));
        if (c->fallo) return izq;
    }
}

static Valor eval_o(Ctx *c) {
    Valor izq = eval_y(c);
    for (;;) {
        espacios(c);
        if (!palabra(c, "O") && !palabra(c, "OR")) return izq;

        // Simetrico al Y: si el primero ya es verdadero, el resultado es
        // verdadero y el segundo no hace falta.
        if (valor_verdadero(&izq)) {
            Ctx basura = *c;
            basura.fallo = 1;
            eval_y(&basura);
            c->p = basura.p;
            izq = LOG(1);
            continue;
        }
        Valor der = eval_y(c);
        izq = LOG(valor_verdadero(&der));
        if (c->fallo) return izq;
    }
}

// ── Entrada ───────────────────────────────────────────────────

int expr_eval(const char *texto, Entorno *env, Valor *out) {
    Ctx c = { texto, env, 0 };
    env->error[0] = '\0';

    Valor v = eval_o(&c);
    if (c.fallo) return -1;

    espacios(&c);
    if (*c.p != '\0') {
        snprintf(env->error, PAED_MSG_MAX, "sobra '%s' al final de la expresion", c.p);
        return -1;
    }

    *out = v;
    return 0;
}

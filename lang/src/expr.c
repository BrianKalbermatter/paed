#include "paed/expr.h"
#include "paed/secuencia.h"   // FDS y NFDS preguntan por el estado de una secuencia
#include "paed/archivo.h"     // FDA y NFDA, por el de un archivo

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   // strcasecmp

// ── Entorno ───────────────────────────────────────────────────

void env_init(Entorno *e) {
    // memset() 
    memset(e, 0, sizeof(*e)); 
}

// Busca la ENTRADA de la tabla, no su valor: env_buscar devuelve el escalar, y
// para un arreglo eso no sirve (hay que llegar a los limites y al offset).
// Se recorre DESDE EL FINAL a proposito. La tabla es una pila: cuando una
// subaccion declara un parametro que se llama igual que una variable global, la
// entrada de la subaccion es la mas nueva, y buscar de atras para adelante hace
// que sea ella la que se encuentre. Eso es, exactamente, que una local tape a
// una global.
//
// Con nombres unicos — que es todo lo que habia antes de las subacciones — da
// el mismo resultado que recorrerla al derecho.
static Variable *env_entrada(Entorno *e, const char *nombre) {
    for (int i = e->count - 1; i >= 0; i--)
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

// Los ganchos al interprete para las FUNCION del propio programa. Ver expr.h.
static PaedFnExiste g_fn_existe = NULL;
static PaedFnLlamar g_fn_llamar = NULL;
static void        *g_fn_ud     = NULL;

// Los conjuntos del programa, para el operador EN. Ver expr.h.
static const PAEDProgram *g_prog = NULL;

void expr_set_conjuntos(const PAEDProgram *prog) { g_prog = prog; }

static const PAEDConjunto *conjunto_buscar(const char *nombre) {
    if (!g_prog) return NULL;
    for (int i = 0; i < g_prog->conjunto_count; i++)
        if (strcasecmp(g_prog->conjuntos[i].name, nombre) == 0)
            return &g_prog->conjuntos[i];
    return NULL;
}

void expr_set_funcion(PaedFnExiste existe, PaedFnLlamar llamar, void *ud) {
    g_fn_existe = existe;
    g_fn_llamar = llamar;
    g_fn_ud     = ud;
}

int env_push(Entorno *e, const char *nombre, Valor v) {
    if (e->count >= PAED_MAX_VARS) {
        snprintf(e->error, PAED_MSG_MAX,
                 "no entran mas variables (maximo %d): demasiadas llamadas anidadas?",
                 PAED_MAX_VARS);
        return -1;
    }
    Variable *nueva = &e->items[e->count++];
    memset(nueva, 0, sizeof(*nueva));
    snprintf(nueva->nombre, PAED_NAME_MAX, "%s", nombre);
    nueva->valor = v;
    return 0;
}

Valor *env_buscar_marco(Entorno *e, const char *nombre, int desde) {
    if (desde < 0) desde = 0;
    for (int i = e->count - 1; i >= desde; i--)
        if (strcmp(e->items[i].nombre, nombre) == 0)
            return &e->items[i].valor;
    return NULL;
}

void env_truncar(Entorno *e, int count, int pool_usado) {
    if (count < 0 || count > e->count) return;
    e->count      = count;
    e->pool_usado = pool_usado;
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
        case VAL_ALTO:   return 1;   // HV no es cero ni vacio: es el tope
        // Declarada y sin asignar. No deberia llegar aca: leerla ya dio "no
        // tiene valor todavia". Se contesta falso para no inventar nada.
        case VAL_VACIO:  return 0;
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
        // Se imprime con su nombre y no con un numero enorme: si un ESCRIBIR
        // muestra HV, lo que hay que ver es que el archivo se agoto — no un
        // 999999999 que hay que reconocer de memoria.
        case VAL_ALTO:
            snprintf(out, out_size, "HV");
            break;
        // Igual que arriba: si llegara a imprimirse, que se lea que no tiene
        // valor y no un 0 que parece un dato.
        case VAL_VACIO:
            snprintf(out, out_size, "(sin valor)");
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

// Copia CRUDO el texto del argumento que arranca en el cursor, hasta la ',' o
// el ')' que lo cierra. Cuenta los parentesis para no cortar en la coma de una
// llamada anidada, y respeta las comillas para no cortar en la coma de un texto.
//
// No se evalua nada aca: ver PaedFnLlamar en expr.h para el porque.
static int copiar_argumento(Ctx *c, char *out, size_t out_n, const char *nombre) {
    espacios(c);

    const char *ini    = c->p;
    int         hondo  = 0;
    char        comilla = 0;

    while (*c->p) {
        char ch = *c->p;

        if (comilla) {
            if (ch == comilla) comilla = 0;
        } else if (ch == '\'' || ch == '"') {
            comilla = ch;
        } else if (ch == '(' || ch == '[') {
            hondo++;
        } else if (ch == ')' || ch == ']') {
            if (ch == ')' && hondo == 0) break;
            hondo--;
        } else if (ch == ',' && hondo == 0) {
            break;
        }
        c->p++;
    }

    size_t largo = (size_t)(c->p - ini);
    while (largo > 0 && isspace((unsigned char)ini[largo - 1])) largo--;

    if (largo == 0) {
        falla(c, "argumento vacio en la llamada a %s", nombre);
        return -1;
    }
    if (largo >= out_n) {
        falla(c, "argumento demasiado largo en la llamada a %s", nombre);
        return -1;
    }

    memcpy(out, ini, largo);
    out[largo] = '\0';
    return 0;
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

// Las grafias de la catedra para el fin de archivo y de secuencia.
//
// El material escribe la negacion de tres formas, y las tres significan lo
// mismo: NFDA(arch) en los ejercicios 2.2.x, NOFDA(arch) en el ejemplo Youtube
// del Tema 8, y NoFDA(arch) en los templates de MEZCLA. ('No FDA(arch)', con
// espacio, ya andaba solo: 'NO' es el operador logico y FDA la funcion.)
//
// Decision 2026-08-17: la catedra tiene la razon, se reconocen todas.
static int es_fin_de(const char *nombre, const char *corta) {
    char negada[16], negada_larga[16];
    snprintf(negada,       sizeof(negada),       "N%s",  corta);   // NFDA
    snprintf(negada_larga, sizeof(negada_larga), "NO%s", corta);   // NOFDA
    return strcasecmp(nombre, corta)        == 0 ||
           strcasecmp(nombre, negada)       == 0 ||
           strcasecmp(nombre, negada_larga) == 0;
}

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
    if (isalpha((unsigned char)*c->p) || *c->p == '_' || (unsigned char)*c->p >= 0x80) {
        char nombre[PAED_NAME_MAX];
        size_t n = 0;
        while (isalnum((unsigned char)*c->p) || *c->p == '_' || (unsigned char)*c->p >= 0x80) {
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

        // HV — alto valor, el centinela de la mezcla de archivos. No se
        // declara, no se asigna y no ocupa una entrada de variable, igual que
        // V y F.
        //
        // Distingue mayusculas por el mismo motivo que 'V': una constante de
        // una o dos letras choca con nombres de variable comunes, y 'v' es
        // justo el que usa AVZ(sec, v) en todo el corpus. Escrita como la
        // escribe la catedra, en mayusculas, no se pisa con nada.
        if (strcmp(nombre, "HV") == 0) { Valor h = {0}; h.tipo = VAL_ALTO; return h; }

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
            // FDS(sec) y NFDS(sec) preguntan por el ESTADO de una secuencia, no
            // por el valor de una variable. El argumento se lee como NOMBRE y
            // no se evalua: 'secAlu' no tiene ningun valor que evaluar, y
            // pasarlo por el evaluador daria "la variable 'secAlu' no tiene
            // valor todavia" — un mensaje que manda a mirar al lugar equivocado.
            if (es_fin_de(nombre, "FDS")) {
                int quiere_fin = (strcasecmp(nombre, "FDS") == 0);

                espacios(c);
                char sec[PAED_NAME_MAX];
                size_t k = 0;
                while (isalnum((unsigned char)*c->p) || *c->p == '_' || (unsigned char)*c->p >= 0x80) {
                    if (k < PAED_NAME_MAX - 1) sec[k++] = *c->p;
                    c->p++;
                }
                sec[k] = '\0';
                espacios(c);

                if (*c->p != ')') {
                    falla(c, "%s lleva el nombre de una secuencia: %s(sec)", nombre, nombre);
                    return LOG(0);
                }
                c->p++;

                if (!k) { falla(c, "%s necesita el nombre de una secuencia", nombre); return LOG(0); }

                Secuencia *s = sec_buscar(sec);
                if (!s) {
                    falla(c, "'%s' no es una secuencia declarada en el AMBIENTE", sec);
                    return LOG(0);
                }
                if (!s->abierta && !s->cerrada) {
                    falla(c, "hay que arrancar '%s' antes de preguntarle por el fin: falta ARR(%s)",
                          sec, sec);
                    return LOG(0);
                }
                return LOG(quiere_fin ? s->fin : !s->fin);
            }

            // FDA(arch) y NFDA(arch) preguntan por el ESTADO de un archivo,
            // igual que FDS por el de una secuencia: el argumento se lee como
            // NOMBRE y no se evalua, porque 'arch' no tiene ningun valor.
            if (es_fin_de(nombre, "FDA")) {
                int quiere_fin = (strcasecmp(nombre, "FDA") == 0);

                espacios(c);
                char arch[PAED_NAME_MAX];
                size_t k = 0;
                while (isalnum((unsigned char)*c->p) || *c->p == '_' || (unsigned char)*c->p >= 0x80) {
                    if (k < PAED_NAME_MAX - 1) arch[k++] = *c->p;
                    c->p++;
                }
                arch[k] = '\0';
                espacios(c);

                if (*c->p != ')') {
                    falla(c, "%s lleva el nombre de un archivo: %s(arch)", nombre, nombre);
                    return LOG(0);
                }
                c->p++;

                if (!k) { falla(c, "%s necesita el nombre de un archivo", nombre); return LOG(0); }

                Archivo *a = arch_buscar(arch);
                if (!a) {
                    falla(c, "'%s' no es un archivo declarado en el AMBIENTE", arch);
                    return LOG(0);
                }
                if (!a->abierto && !a->cerrado) {
                    falla(c, "hay que abrir '%s' antes de preguntarle por el fin: "
                             "falta ABRIR(%s)", arch, arch);
                    return LOG(0);
                }
                return LOG(quiere_fin ? a->fin : !a->fin);
            }

            // ── Una FUNCION del propio programa ──
            //
            // Se pregunta ANTES que por las builtin porque estas pueden llevar
            // VARIOS argumentos, y el camino de las builtin lee uno solo: con
            // 'sumar(3, 5)' se comeria el 3 y despues se quejaria de que falta
            // el ')' que en realidad esta, mandando a mirar la linea equivocada.
            if (g_fn_existe && g_fn_existe(nombre, g_fn_ud)) {
                char        textos[PAED_MAX_ARGS][PAED_VAL_MAX];
                const char *args[PAED_MAX_ARGS];
                int         n_args = 0;

                espacios(c);
                if (*c->p != ')') {
                    for (;;) {
                        if (n_args >= PAED_MAX_ARGS) {
                            falla(c, "demasiados argumentos en la llamada a %s (maximo %d)",
                                  nombre, PAED_MAX_ARGS);
                            return NUM(0);
                        }
                        if (copiar_argumento(c, textos[n_args], PAED_VAL_MAX, nombre) != 0)
                            return NUM(0);
                        args[n_args] = textos[n_args];
                        n_args++;
                        if (*c->p != ',') break;
                        c->p++;
                    }
                }
                espacios(c);
                if (*c->p != ')') { falla(c, "falta ')' al cerrar %s", nombre); return NUM(0); }
                c->p++;

                Valor out;
                memset(&out, 0, sizeof(out));
                char motivo[PAED_MSG_MAX] = {0};
                if (!g_fn_llamar ||
                    g_fn_llamar(nombre, args, n_args, &out, motivo, sizeof(motivo), g_fn_ud) != 0) {
                    falla(c, "%s", motivo[0] ? motivo : "la funcion fallo");
                    return NUM(0);
                }
                return out;
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

        // Existe en la tabla pero todavia no le asignaron nada. NO es el
        // mismo error que "no existe", aunque antes se trataban igual:
        //
        //   fantasma   nunca se declaro          -> se escribio mal, o falta
        //                                          la declaracion
        //   b          declarada, sin asignar    -> falta el ':=' antes de
        //                                          usarla
        //
        // Decir "no tiene valor todavia" para los dos manda a buscar un ':='
        // que falta cuando en realidad lo que hay es un nombre mal escrito, y
        // eso hace perder mucho tiempo.
        if (v && v->tipo == VAL_VACIO) {
            falla(c, "'%s' esta declarado pero todavia no tiene valor: "
                     "hay que asignarle algo antes de usarlo", nombre);
            return NUM(0);
        }

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
                falla(c, "'%s' no esta declarado en el AMBIENTE", nombre);
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
static int comparar(const Valor *a, const Valor *b);

int expr_comparar(const Valor *a, const Valor *b) { return comparar(a, b); }

static int comparar(const Valor *a, const Valor *b) {
    // HV va PRIMERO, antes de mirar los tipos: es mayor que todo, y dos HV son
    // iguales entre si. Que este arriba de la rama de texto no es un detalle —
    // si cayera ahi, se compararia la cadena "HV" por ASCII y perderia contra
    // cualquier clave que empiece con una letra posterior a la H.
    if (a->tipo == VAL_ALTO || b->tipo == VAL_ALTO) {
        if (a->tipo == VAL_ALTO && b->tipo == VAL_ALTO) return 0;
        return a->tipo == VAL_ALTO ? 1 : -1;
    }

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
static int membresia(Ctx *c, Valor *izq);   // EN / NO EN, aca abajo

static Valor igualdad(Ctx *c) {
    Valor izq = relacional(c);
    for (;;) {
        espacios(c);

        if (membresia(c, &izq)) {
            if (c->fallo) return izq;
            continue;
        }

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

// ── Prioridad 7 tambien: EN y NO EN ─────────────────────────────────────────
//
//     SI (v EN vocales) ENTONCES
//     MIENTRAS (v NO EN separadores) HACER
//
// Preguntar si un valor esta en un conjunto es una COMPARACION, asi que va en
// el mismo nivel que '=' y '<>'. Que este aca y no en el SI ni en el MIENTRAS
// es a proposito: los dos evaluan su condicion con este mismo evaluador, asi
// que alcanza con escribirlo una vez para que funcione en los dos — y en el
// HASTA del REPETIR, que es la misma condicion.

// Un elemento del conjunto, convertido a valor. El texto se guardo tal cual se
// escribio, y recien aca se decide que es: un numero si se lee entero como
// numero, y texto en cualquier otro caso.
static Valor elemento_a_valor(const char *txt) {
    char *fin = NULL;
    double n = strtod(txt, &fin);
    if (fin && fin != txt) {
        while (*fin && isspace((unsigned char)*fin)) fin++;
        if (*fin == '\0') return NUM(n);
    }

    Valor v = {0};
    v.tipo = VAL_TEXTO;
    snprintf(v.texto, sizeof(v.texto), "%s", txt);
    return v;
}

static int conjunto_tiene(const PAEDConjunto *cj, const Valor *v) {
    for (int i = 0; i < cj->elem_count; i++) {
        Valor e = elemento_a_valor(cj->elems[i]);
        if (expr_comparar(v, &e) == 0) return 1;
    }
    return 0;
}

// Consume `EN <conjunto>` o `NO EN <conjunto>` y deja el resultado en *izq.
// Devuelve 1 si habia uno; 0 si la expresion seguia por otro lado, sin haber
// tocado la posicion de lectura.
static int membresia(Ctx *c, Valor *izq) {
    const char *guardado = c->p;

    int negado = 0;
    if (palabra(c, "NO")) negado = 1;

    if (!palabra(c, "EN")) {
        c->p = guardado;   // el 'NO' no era de esto: se devuelve intacto
        return 0;
    }

    espacios(c);
    char nombre[PAED_NAME_MAX];
    size_t n = 0;
    while ((isalnum((unsigned char)*c->p) || *c->p == '_') &&
           n < sizeof(nombre) - 1)
        nombre[n++] = *c->p++;
    nombre[n] = '\0';

    if (n == 0) {
        falla(c, "despues de EN va el nombre de un conjunto");
        return 1;
    }

    const PAEDConjunto *cj = conjunto_buscar(nombre);
    if (!cj) {
        falla(c, "'%s' no es un conjunto declarado: se declara en el AMBIENTE "
                 "con %s = {a, b, c};", nombre, nombre);
        return 1;
    }

    int adentro = conjunto_tiene(cj, izq);
    *izq = LOG(negado ? !adentro : adentro);
    return 1;
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

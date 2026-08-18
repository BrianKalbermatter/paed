#include "paed/interpreter.h"
#include "paed/expr.h"
#include "paed/secuencia.h"
#include "paed/archivo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>    // isalpha/isalnum: validar el destino de un LEER
#include <strings.h>  // strcasecmp: los nombres de procedimiento no distinguen mayusculas
#include <math.h>     // floor: un indice de arreglo tiene que ser entero

// El parser ya valido nombres, tipos y obligatorios contra sintaxis.json.
// Aca solo queda ejecutar.
//
// Este archivo tiene SOLO el lenguaje: control de flujo, asignacion, LEER y
// ESCRIBIR. Lo que agrega el host (la escena 3D de VimMon, por ejemplo) entra
// por el registro de procedimientos de mas abajo y no se nombra aca.

// Las variables del programa. A nivel de archivo y no dentro de interp_exec
// porque ESCRIBIR, que vive en exec_instr, tambien necesita leerlas.
static Entorno env;

void paed_runtime_error(const PAEDProgram *prog, const PAEDInstr *in, const char *msg) {
    fprintf(stderr, "%s:%d: error: %s\n", prog->path, in->line, msg);
}

// Nombre corto para el uso interno, que es la mayoria de las llamadas.
#define runtime_error paed_runtime_error

// ── Procedimientos del host ──────────────────────────────────────────────────
//
// Tabla plana y sin malloc, como el resto del proyecto. Doce entradas ya las
// usa la escena de VimMon, asi que 32 deja lugar de sobra sin gastar memoria.
#define PAED_MAX_PROCS 32

typedef struct {
    char     nombre[PAED_NAME_MAX];
    PaedProc fn;
    void    *ud;
} ProcHost;

static ProcHost g_procs[PAED_MAX_PROCS];
static int      g_proc_count = 0;

int paed_register_proc(const char *nombre, PaedProc fn, void *ud) {
    if (!nombre || !*nombre || !fn) return -1;

    // Reemplazar y no acumular: al recargar una escena se registra de nuevo
    // apuntando al estado nuevo, y la entrada vieja quedaria apuntando a
    // memoria que ya no es de nadie.
    for (int i = 0; i < g_proc_count; i++) {
        if (strcasecmp(g_procs[i].nombre, nombre) == 0) {
            g_procs[i].fn = fn;
            g_procs[i].ud = ud;
            return 0;
        }
    }

    if (g_proc_count >= PAED_MAX_PROCS) return -1;

    ProcHost *p = &g_procs[g_proc_count++];
    snprintf(p->nombre, sizeof(p->nombre), "%s", nombre);
    p->fn = fn;
    p->ud = ud;
    return 0;
}

void paed_clear_procs(void) {
    g_proc_count = 0;
}

static const ProcHost *buscar_proc(const char *nombre) {
    for (int i = 0; i < g_proc_count; i++)
        if (strcasecmp(g_procs[i].nombre, nombre) == 0)
            return &g_procs[i];
    return NULL;
}

// ── Entrada de datos (LEER de consola) ───────────────────────────────────────
static PaedEntrada entrada_fn = NULL;
static void       *entrada_ud = NULL;

void interp_set_entrada(PaedEntrada fn, void *ud) {
    entrada_fn = fn;
    entrada_ud = ud;
}

// ── Datos de las secuencias ──────────────────────────────────────────────────
static PaedSecuenciaDatos secdatos_fn = NULL;
static void              *secdatos_ud = NULL;

void interp_set_secuencia(PaedSecuenciaDatos fn, void *ud) {
    secdatos_fn = fn;
    secdatos_ud = ud;
}

// Donde vive el .csv lo decide arch_dir_del_programa, en archivo.c. Estuvo aca
// hasta que el asistente de archivos necesito la misma cuenta: el asistente
// CREA el .csv y el interprete lo BUSCA, asi que si cada uno la hacia por su
// lado, el dia que una cambiara el archivo creado no aparecia.

static const PAEDRegistro *registro_de(const PAEDProgram *prog, const char *tipo);

// ¿Sirve como destino de un LEER? Tiene que ser un nombre: 'x', o 'p.campo'.
// Descarta LEER("hola") y LEER(3), que no tienen donde guardar nada.
static int es_destino(const char *s) {
    if (!s || !*s) return 0;
    if (!isalpha((unsigned char)*s) && *s != '_') return 0;
    for (const char *c = s + 1; *c; c++)
        if (!isalnum((unsigned char)*c) && *c != '_' && *c != '.') return 0;
    return 1;
}

// Parte un destino escrito `A[i]` en nombre e indice. Sin corchetes, el indice
// queda vacio. Devuelve 0 si esta bien escrito, -1 si no.
//
// El parser ya hace esto para el destino de `:=` (parser.c:265) y guarda el
// indice como un argumento aparte. Los argumentos de LEER, en cambio, llegan
// crudos porque LEER es variadico: cualquiera de ellos puede ser 'A[i]', no
// solo el primero.
static int partir_destino(const char *destino, char *nombre, size_t nn,
                          char *indice, size_t ni) {
    const char *corchete = strchr(destino, '[');
    if (!corchete) {
        snprintf(nombre, nn, "%s", destino);
        indice[0] = '\0';
        return es_destino(nombre) ? 0 : -1;
    }

    size_t len = strlen(destino);
    if (len == 0 || destino[len - 1] != ']') return -1;

    size_t largo_nombre = (size_t)(corchete - destino);
    if (largo_nombre == 0 || largo_nombre >= nn) return -1;
    snprintf(nombre, nn, "%.*s", (int)largo_nombre, destino);

    size_t largo_indice = len - largo_nombre - 2;   // sin '[' ni ']'
    if (largo_indice == 0 || largo_indice >= ni) return -1;
    snprintf(indice, ni, "%.*s", (int)largo_indice, corchete + 1);

    return es_destino(nombre) ? 0 : -1;
}

// Guarda `v` en un destino ya partido: `nombre` y, si es un elemento de
// arreglo, `idx_txt` con el indice SIN evaluar. Lo comparten la asignacion y
// LEER, que guardan en los mismos tres lugares (escalar, A[i], p.campo) y solo
// se diferencian en de donde sale el valor.
static int guardar_valor(const PAEDProgram *prog, const PAEDInstr *in,
                         const char *nombre, const char *idx_txt, Valor v) {
    if (!idx_txt || !*idx_txt) {
        // Un campo de registro NO nace en su primera asignacion, al reves que
        // un escalar: los campos se crearon todos al declarar la variable. Si
        // el nombre tiene punto y no existe, es un campo que el registro no
        // tiene — y aceptarlo callado dejaria el registro sin sentido, porque
        // admitiria cualquier campo inventado.
        const char *punto = strchr(nombre, '.');
        if (punto && !env_existe(&env, nombre)) {
            char msg[PAED_MSG_MAX];
            snprintf(msg, sizeof(msg),
                     "'%.*s' no tiene un campo '%s'",
                     (int)(punto - nombre), nombre, punto + 1);
            runtime_error(prog, in, msg);
            return -1;
        }

        if (env_set(&env, nombre, v) != 0) {
            // env_set deja el motivo cuando el nombre ya existe como arreglo.
            runtime_error(prog, in, env.error[0] ? env.error : "no entran mas variables");
            return -1;
        }
        return 0;
    }

    // El indice se evalua RECIEN AHORA, no al parsear: en A[i] := 0 dentro de
    // un bucle, i vale distinto en cada vuelta.
    Valor idx;
    if (expr_eval(idx_txt, &env, &idx) != 0) {
        runtime_error(prog, in, env.error);
        return -1;
    }
    if (idx.tipo != VAL_NUM || idx.num != floor(idx.num)) {
        char msg[PAED_MSG_MAX];
        snprintf(msg, sizeof(msg), "el indice de %s tiene que ser un numero entero", nombre);
        runtime_error(prog, in, msg);
        return -1;
    }

    Valor *elem = env_elem(&env, nombre, (int)idx.num);
    if (!elem) {
        runtime_error(prog, in, env.error);
        return -1;
    }
    *elem = v;
    return 0;
}

// Le saca los espacios de los dos lados, EN EL LUGAR. El '\r' cuenta como
// espacio a proposito: un archivo de datos guardado en Windows termina cada
// linea con "\r\n", y ese '\r' invisible convertiria el numero 12 en el texto
// "12\r". El bug seria mudo y la culpa se la llevaria el interprete.
static char *trim_linea(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n'))
        s[--n] = '\0';
    return s;
}

// Convierte lo que vino por la entrada en un Valor.
//
// PAED declara los tipos en el AMBIENTE, pero el Entorno todavia no los usa
// (esta anotado en el KANBAN), asi que el tipo lo decide EL DATO: si la linea
// entera es un numero, es numero; si no, es texto. Entera, no "empieza con":
// "12abc" es el texto "12abc" y no el numero 12, porque un numero a medias
// escondería el error del que cargo el dato.
static Valor valor_desde_texto(const char *linea) {
    Valor v;
    memset(&v, 0, sizeof(v));

    char *fin = NULL;
    double d  = strtod(linea, &fin);
    if (fin != linea) {
        while (*fin == ' ' || *fin == '\t') fin++;
        if (*fin == '\0') {
            v.tipo = VAL_NUM;
            v.num  = d;
            return v;
        }
    }

    v.tipo = VAL_TEXTO;
    snprintf(v.texto, sizeof(v.texto), "%s", linea);
    return v;
}

// LEER(A, B, p.campo) — pide un dato por destino y lo guarda.
//
// UNA LINEA POR DESTINO, no un token: asi un texto con espacios entra entero.
// Partir por espacios haria que "Juan Perez" llenara dos destinos con medio
// nombre cada uno.
static int leer_consola(const PAEDProgram *prog, const PAEDInstr *in) {
    if (in->arg_count == 0) {
        runtime_error(prog, in, "LEER necesita al menos un destino: LEER(x)");
        return -1;
    }

    if (!entrada_fn) {
        runtime_error(prog, in,
                      "LEER no tiene de donde sacar los datos: este programa corre sin "
                      "entrada enganchada (probalo con build/paedrun)");
        return -1;
    }

    for (int i = 0; i < in->arg_count; i++) {
        const char *destino = in->args[i].val;

        char nombre[PAED_NAME_MAX];
        char indice[PAED_VAL_MAX];
        if (partir_destino(destino, nombre, sizeof(nombre), indice, sizeof(indice)) != 0) {
            char msg[PAED_MSG_MAX];
            snprintf(msg, sizeof(msg),
                     "'%s' no sirve como destino de LEER: se espera una variable, "
                     "un elemento A[i] o un campo p.campo", destino);
            runtime_error(prog, in, msg);
            return -1;
        }

        char linea[PAED_VAL_MAX];
        if (entrada_fn(linea, sizeof(linea), entrada_ud) != 0) {
            char msg[PAED_MSG_MAX];
            snprintf(msg, sizeof(msg),
                     "la entrada se termino antes de darle un valor a '%s'", destino);
            runtime_error(prog, in, msg);
            return -1;
        }

        char *dato = trim_linea(linea);
        if (!*dato) {
            // Una linea vacia casi siempre es un dato que falta, no un texto
            // vacio a proposito. Se avisa en vez de guardar "" callado y que el
            // error aparezca tres instrucciones despues, sin relacion aparente.
            char msg[PAED_MSG_MAX];
            snprintf(msg, sizeof(msg), "la entrada trajo una linea vacia para '%s'", destino);
            runtime_error(prog, in, msg);
            return -1;
        }

        if (guardar_valor(prog, in, nombre, indice, valor_desde_texto(dato)) != 0)
            return -1;
    }

    return 0;
}

// ── Secuencias ───────────────────────────────────────────────────────────────
//
// El estado vive en secuencia.c, que tambien lo usa el evaluador para FDS y
// NFDS. Aca queda solo el puente: buscar la secuencia que nombra la
// instruccion, llamar a la operacion y traducir su fallo a un error con
// archivo:linea.

// La secuencia que nombra el primer argumento. El parser ya le puso la clave
// "secuencia" al resolver la forma, asi que aca no se adivina nada.
static Secuencia *secuencia_de(const PAEDProgram *prog, const PAEDInstr *in) {
    const char *nombre = paed_get_arg(in, "secuencia");
    if (!nombre) nombre = in->arg_count > 0 ? in->args[0].val : "";

    Secuencia *s = sec_buscar(nombre);
    if (!s) {
        char msg[PAED_MSG_MAX];
        snprintf(msg, sizeof(msg),
                 "'%s' no es una secuencia declarada en el AMBIENTE", nombre);
        runtime_error(prog, in, msg);
    }
    return s;
}

// Traduce el fallo de secuencia.c a un error del programa.
static int sec_falla(const PAEDProgram *prog, const PAEDInstr *in) {
    runtime_error(prog, in, sec_error());
    return -1;
}

// AVZ(sec, destino) — trae el proximo elemento de la secuencia al destino.
//
// Cuando la secuencia se termino, el destino NO se toca: la ventana queda con
// lo ultimo que trajo y FDS pasa a ser verdadero. Es el modelo de AED, y es lo
// que hace que `MIENTRAS NFDS(sec)` corte donde tiene que cortar.
static int avanzar_secuencia(const PAEDProgram *prog, const PAEDInstr *in) {
    if (in->arg_count != 2) {
        runtime_error(prog, in, "AVZ lleva dos argumentos: AVZ(secuencia, destino)");
        return -1;
    }

    Secuencia *s = secuencia_de(prog, in);
    if (!s) return -1;

    char elem[PAED_VAL_MAX];
    int  hay = 0;
    if (sec_avanzar(s, elem, sizeof(elem), &hay) != 0) return sec_falla(prog, in);
    if (!hay) return 0;   // llego al fin: no es error, es el ultimo avance

    const char *destino = in->args[1].val;
    char nombre[PAED_NAME_MAX];
    char indice[PAED_VAL_MAX];
    if (partir_destino(destino, nombre, sizeof(nombre), indice, sizeof(indice)) != 0) {
        char msg[PAED_MSG_MAX];
        snprintf(msg, sizeof(msg),
                 "'%s' no sirve como destino de AVZ: se espera una variable, "
                 "un elemento A[i] o un campo p.campo", destino);
        runtime_error(prog, in, msg);
        return -1;
    }

    // En una secuencia DE CARACTER el elemento es siempre un caracter, aunque
    // se vea como un numero: el '5' de un legajo es el caracter '5' y no el
    // numero 5. Por eso el tipo lo decide la DECLARACION y no el dato, al
    // reves que en LEER — que no tiene ninguna declaracion de donde agarrarse.
    Valor v;
    if (s->de_caracteres) {
        memset(&v, 0, sizeof(v));
        v.tipo = VAL_TEXTO;
        snprintf(v.texto, sizeof(v.texto), "%s", elem);
    } else {
        v = valor_desde_texto(elem);
    }

    return guardar_valor(prog, in, nombre, indice, v);
}

// ESCRIBIR(secSal, dato) — le agrega un elemento a la secuencia de salida.
static int grabar_secuencia(const PAEDProgram *prog, const PAEDInstr *in) {
    Secuencia *s = secuencia_de(prog, in);
    if (!s) return -1;

    for (int i = 1; i < in->arg_count; i++) {
        const char *a = in->args[i].val;
        size_t n = strlen(a);

        char texto[PAED_VAL_MAX];
        if (n >= 2 && (a[0] == '"' || a[0] == '\'') && a[n - 1] == a[0]) {
            snprintf(texto, sizeof(texto), "%.*s", (int)(n - 2), a + 1);
        } else {
            Valor v;
            if (expr_eval(a, &env, &v) != 0) {
                runtime_error(prog, in, env.error);
                return -1;
            }
            valor_a_texto(&v, texto, sizeof(texto));
        }

        if (sec_grabar(s, texto) != 0) return sec_falla(prog, in);
    }
    return 0;
}

// Las cuatro operaciones que abren y cierran. CERRAR de una secuencia de
// salida es la que la IMPRIME: mientras esta abierta, la secuencia es un dato
// que el programa esta armando; cerrarla es decir que ya esta.
static int exec_secuencia(const PAEDProgram *prog, const PAEDInstr *in, const char *p) {
    Secuencia *s = secuencia_de(prog, in);
    if (!s) return -1;

    if (strcasecmp(p, "ARR")   == 0) return sec_arrancar(s) != 0 ? sec_falla(prog, in) : 0;
    if (strcasecmp(p, "CREAR") == 0) return sec_crear(s)    != 0 ? sec_falla(prog, in) : 0;

    if (strcasecmp(p, "CERRAR") == 0) {
        int salida = s->es_salida;
        if (sec_cerrar(s) != 0) return sec_falla(prog, in);
        if (salida) printf("%s\n", s->datos);
        return 0;
    }

    if (strcasecmp(p, "AVZ") == 0) return avanzar_secuencia(prog, in);

    return grabar_secuencia(prog, in);
}

// ── Archivos ─────────────────────────────────────────────────────────────────

static int arch_falla(const PAEDProgram *prog, const PAEDInstr *in) {
    runtime_error(prog, in, arch_error());
    return -1;
}

static Archivo *archivo_de(const PAEDProgram *prog, const PAEDInstr *in) {
    const char *nombre = paed_get_arg(in, "archivo");
    if (!nombre) {
        runtime_error(prog, in, "falta el nombre del archivo");
        return NULL;
    }
    Archivo *a = arch_buscar(nombre);
    if (!a) {
        char msg[PAED_MSG_MAX];
        snprintf(msg, sizeof(msg), "'%s' no es un archivo declarado en el AMBIENTE", nombre);
        runtime_error(prog, in, msg);
        return NULL;
    }
    return a;
}

// ¿El campo guarda un numero? Lo dice el TIPO declarado, no el dato.
//
// Es al reves que en LEER de consola, donde el tipo lo decide el dato porque no
// hay ninguna declaracion de donde agarrarse. Aca el REGISTRO ya lo dijo, y
// respetarlo es lo que hace que un ENTERO con 'abc' adentro sea un error de
// lectura y no un cero silencioso.
static int campo_es_numero(const char *tipo) {
    return strncasecmp(tipo, "ENTERO", 6) == 0 ||
           strncasecmp(tipo, "REAL",   4) == 0 ||
           (toupper((unsigned char)tipo[0]) == 'N' && tipo[1] == '(');
}

// El REGISTRO que se lee desde el archivo, o hacia el. Es el segundo argumento.
static const PAEDRegistro *registro_del_arg(const PAEDProgram *prog, const PAEDInstr *in,
                                            const char **var) {
    if (in->arg_count < 2) {
        runtime_error(prog, in, "falta el registro: se escribe LEER(archivo, registro)");
        return NULL;
    }
    *var = in->args[1].val;

    const PAEDDecl *d = NULL;
    for (int i = 0; i < prog->decl_count; i++)
        if (strcasecmp(prog->decls[i].name, *var) == 0) { d = &prog->decls[i]; break; }

    if (!d) {
        char msg[PAED_MSG_MAX];
        snprintf(msg, sizeof(msg), "'%s' no esta declarado en el AMBIENTE", *var);
        runtime_error(prog, in, msg);
        return NULL;
    }

    const PAEDRegistro *r = registro_de(prog, d->type);
    if (!r) {
        char msg[PAED_MSG_MAX];
        snprintf(msg, sizeof(msg),
                 "'%s' es de tipo '%s', que no es un REGISTRO: un archivo se lee "
                 "hacia un registro", *var, d->type);
        runtime_error(prog, in, msg);
        return NULL;
    }
    return r;
}

static int leer_archivo(const PAEDProgram *prog, const PAEDInstr *in, Archivo *a) {
    const char *var = NULL;
    const PAEDRegistro *r = registro_del_arg(prog, in, &var);
    if (!r) return -1;

    char valores[PAED_MAX_CAMPOS][PAED_VAL_MAX];
    int  hay = 0;
    if (arch_leer(a, valores, &hay) != 0) return arch_falla(prog, in);

    // Sin fila no se toca el registro: conserva lo que tenia. Es lo que espera
    // el bucle de AED, donde el LEER de abajo es el que pone FDA en verdadero.
    if (!hay) return 0;

    for (int c = 0; c < r->campo_count; c++) {
        char completo[PAED_NAME_MAX];
        snprintf(completo, sizeof(completo), "%s.%s", var, r->campos[c].name);

        Valor v = {0};
        if (campo_es_numero(r->campos[c].type)) {
            char *fin = NULL;
            v.tipo = VAL_NUM;
            v.num  = strtod(valores[c], &fin);
            while (fin && isspace((unsigned char)*fin)) fin++;

            if (!valores[c][0] || (fin && *fin)) {
                char msg[PAED_MSG_MAX];
                snprintf(msg, sizeof(msg),
                         "en el archivo '%s' el campo '%s' dice '%s' y esta declarado %s",
                         a->nombre, r->campos[c].name, valores[c], r->campos[c].type);
                runtime_error(prog, in, msg);
                return -1;
            }
        } else {
            v.tipo = VAL_TEXTO;
            snprintf(v.texto, sizeof(v.texto), "%s", valores[c]);
        }

        if (env_set(&env, completo, v) != 0) {
            runtime_error(prog, in, env.error);
            return -1;
        }
    }
    return 0;
}

static int grabar_archivo(const PAEDProgram *prog, const PAEDInstr *in, Archivo *a) {
    const char *var = NULL;
    const PAEDRegistro *r = registro_del_arg(prog, in, &var);
    if (!r) return -1;

    char valores[PAED_MAX_CAMPOS][PAED_VAL_MAX];
    for (int c = 0; c < r->campo_count; c++) {
        char completo[PAED_NAME_MAX];
        snprintf(completo, sizeof(completo), "%s.%s", var, r->campos[c].name);

        const Valor *v = env_buscar(&env, completo);
        if (!v) {
            char msg[PAED_MSG_MAX];
            snprintf(msg, sizeof(msg), "el campo '%s' no tiene valor todavia", completo);
            runtime_error(prog, in, msg);
            return -1;
        }
        valor_a_texto(v, valores[c], PAED_VAL_MAX);
    }

    if (arch_grabar(a, valores) != 0) return arch_falla(prog, in);
    return 0;
}

static int exec_archivo(const PAEDProgram *prog, const PAEDInstr *in, const char *p) {
    Archivo *a = archivo_de(prog, in);
    if (!a) return -1;

    // El modo viene de la instruccion (ABRIR E/), y el parser ya verifico que
    // sea uno valido y que el procedimiento lo admita.
    if (strcasecmp(p, "CREAR")  == 0) return arch_crear(a, in->modo)  != 0 ? arch_falla(prog, in) : 0;
    if (strcasecmp(p, "ABRIR")  == 0) return arch_abrir(a, in->modo)  != 0 ? arch_falla(prog, in) : 0;
    if (strcasecmp(p, "CERRAR") == 0) return arch_cerrar(a)           != 0 ? arch_falla(prog, in) : 0;
    if (strcasecmp(p, "LEER")   == 0) return leer_archivo(prog, in, a);
    return grabar_archivo(prog, in, a);
}

static int exec_instr(const PAEDProgram *prog, const PAEDInstr *in) {
    const char *p = in->proc;

    // ── Secuencias ───────────────────────────────────────────────────────────
    //
    // Va PRIMERO, antes que la rama de archivos: ARR y AVZ solo existen para
    // secuencias, y CREAR/CERRAR valen para las dos — a esas las separa la
    // forma que ya resolvio el parser mirando la declaracion.
    if (in->forma == PAED_FORMA_SECUENCIA) return exec_secuencia(prog, in, p);

    if (strcasecmp(p, "ARR") == 0 || strcasecmp(p, "AVZ") == 0) {
        char msg[PAED_MSG_MAX];
        snprintf(msg, sizeof(msg),
                 "%s trabaja sobre una secuencia y esta no lo es", p);
        runtime_error(prog, in, msg);
        return -1;
    }

    // ── Entrada y salida ─────────────────────────────────────────────────────
    //
    // LEER y ESCRIBIR tienen DOS formas y el parser ya decidio cual es esta
    // (in->forma), mirando si el primer argumento se declaro como archivo.
    // Aca solo se despacha. Que la bifurcacion vaya ANTES del bucle de
    // impresion no es un detalle: ESCRIBIR(arch, reg) entraba al bucle, se
    // evaluaba 'arch' como si fuera una variable y el error que salia era
    // "la variable 'arch' no tiene valor todavia", que manda a mirar al lugar
    // equivocado.
    // ABRIR y CERRAR van PRIMERO: tambien operan sobre un archivo, asi que el
    // parser les pone forma ARCHIVO, y si el chequeo de abajo fuera antes
    // dirian que "no graban" — que no es lo que hacen.
    if (in->forma == PAED_FORMA_ARCHIVO &&
        (strcasecmp(p, "ABRIR")  == 0 || strcasecmp(p, "CERRAR") == 0 ||
         strcasecmp(p, "CREAR")  == 0 || strcasecmp(p, "LEER")   == 0 ||
         strcasecmp(p, "ESCRIBIR") == 0))
        return exec_archivo(prog, in, p);

    if (strcasecmp(p, "ABRIR") == 0) {
        char msg[PAED_MSG_MAX];
        snprintf(msg, sizeof(msg),
                 "ABRIR trabaja sobre un archivo declarado en el AMBIENTE");
        runtime_error(prog, in, msg);
        return -1;
    }

    if (strcasecmp(p, "LEER") == 0) return leer_consola(prog, in);

    // ── Salida por consola ───────────────────────────────────────────────────
    if (strcasecmp(p, "ESCRIBIR") == 0) {
        for (int i = 0; i < in->arg_count; i++) {
            const char *v = in->args[i].val;

            // TODO argumento se evalua, incluido el que parece un texto pelado.
            //
            // Aca habia un atajo: si el argumento empezaba y terminaba con
            // comilla, se imprimia el pedazo del medio sin pasar por el
            // evaluador. La condicion era demasiado floja — `"Ana" + "Beto"`
            // tambien empieza y termina con comilla, asi que la expresion
            // entera se tomaba por un literal y se imprimia `Ana" + "Beto`.
            // Lo mismo con `"Ana" = "Ana"` y con cualquier operador entre dos
            // textos literales.
            //
            // Se saca en vez de ajustarle la condicion porque era una SEGUNDA
            // implementacion de lo que primario() ya hace en expr.c, y con las
            // mismas reglas: saca las comillas, respeta PAED_VAL_MAX y acepta
            // las simples igual que las dobles. Dos caminos para lo mismo es
            // exactamente como uno de los dos se queda atras sin que se note.
            Valor val;
            char  buf[PAED_VAL_MAX];
            if (expr_eval(v, &env, &val) == 0) {
                valor_a_texto(&val, buf, sizeof(buf));
                printf("%s", buf);
            } else {
                runtime_error(prog, in, env.error);
                return -1;
            }
        }
        printf("\n");
        return 0;
    }

    // ── Lo que puso el host ──────────────────────────────────────────────────
    //
    // Va DESPUES de los procedimientos del lenguaje, a proposito: registrar un
    // "LEER" propio no puede tapar al LEER de AED. El host extiende el
    // lenguaje, no lo redefine.
    const ProcHost *ph = buscar_proc(p);
    if (ph) return ph->fn(prog, in, ph->ud);

    // El parser lo acepto (esta en sintaxis.json o en una libreria cargada
    // aparte) pero nadie lo implementa. Pasa, por ejemplo, si se corre un .paed
    // con CUBO desde paedrun, que no engancha la escena de VimMon.
    char msg[PAED_MSG_MAX];
    snprintf(msg, sizeof(msg),
             "'%s' lo reconoce el parser pero no lo implementa nadie: "
             "el host no registro ese procedimiento", p);
    runtime_error(prog, in, msg);
    return -1;
}

// Un programa mal escrito puede quedar dando vueltas para siempre, y el
// interprete corre DENTRO del game loop: colgarlo cuelga la ventana entera.
// Se corta y se avisa, que es infinitamente mejor que tener que matar el
// proceso sin saber por que.
#define PAED_MAX_PASOS 2000000

// ¿La condicion de un SI/MIENTRAS es verdadera? Deja el motivo en el entorno
// si no se pudo evaluar.
static int condicion(Entorno *env, const PAEDProgram *prog, const PAEDInstr *in, int *ok) {
    Valor v;
    if (expr_eval(in->cond, env, &v) != 0) {
        runtime_error(prog, in, env->error);
        *ok = 0;
        return 0;
    }
    *ok = 1;
    return valor_verdadero(&v);
}

// Reserva los arreglos declarados en el AMBIENTE. Un escalar no hace falta
// declararlo (nace en su primera asignacion), pero un arreglo si: sin los
// limites no hay donde guardar ni contra que chequear el indice.
// ¿El tipo de esta declaracion es un REGISTRO declarado en el AMBIENTE?
static const PAEDRegistro *registro_de(const PAEDProgram *prog, const char *tipo) {
    for (int i = 0; i < prog->registro_count; i++)
        if (strcasecmp(prog->registros[i].name, tipo) == 0)
            return &prog->registros[i];
    return NULL;
}

static int declarar_ambiente(const PAEDProgram *prog) {
    int fallos = 0;
    for (int i = 0; i < prog->decl_count; i++) {
        const PAEDDecl *d = &prog->decls[i];

        // Una secuencia se anota y, si es de entrada, se le piden los datos AL
        // HOST — una sola vez, aca. Que se pidan al arrancar y no de a un
        // elemento por AVZ no es un detalle de implementacion: la secuencia es
        // un dato fijo del enunciado, y pedirla entera permite que FDS pueda
        // contestar sin tener que adivinar si viene algo mas.
        if (d->es_secuencia) {
            Secuencia *s = sec_declarar(d->name, d->type, d->es_salida);
            if (!s) {
                fprintf(stderr, "%s:%d: error: %s\n", prog->path, d->line, sec_error());
                fallos++;
                continue;
            }
            if (d->es_salida) continue;

            char datos[PAED_SEC_MAX];
            if (secdatos_fn && secdatos_fn(d->name, datos, sizeof(datos), secdatos_ud) == 0) {
                if (sec_cargar(s, datos) != 0) {
                    fprintf(stderr, "%s:%d: error: %s\n", prog->path, d->line, sec_error());
                    fallos++;
                }
            }
            // Sin datos la secuencia queda VACIA, no rota: el primer AVZ la da
            // por terminada. Es lo correcto — un enunciado puede tener una
            // secuencia sin ningun elemento, y eso no es un error del programa.
            continue;
        }

        if (d->es_arreglo) {
            if (env_declarar_arreglo(&env, d->name, d->desde, d->hasta) != 0) {
                fprintf(stderr, "%s:%d: error: %s\n", prog->path, d->line, env.error);
                fallos++;
            }
            continue;
        }

        // Un ARCHIVO se anota en la tabla de archivos y NO se aplana en
        // variables. Va antes de la rama de registros a proposito: sin este
        // `continue`, `arch: ARCHIVO DE venta` creaba las variables
        // "arch.codigo" y "arch.precio" como si el archivo fuera un registro en
        // memoria — y un archivo no tiene campos, los tiene el registro que se
        // lee DESDE el.
        if (d->es_archivo) {
            char dir[PAED_PATH_MAX];
            arch_dir_del_programa(prog->path, dir, sizeof(dir));

            Archivo *a = arch_declarar(d->name, dir);
            if (!a) {
                fprintf(stderr, "%s:%d: error: %s\n", prog->path, d->line, arch_error());
                fallos++;
                continue;
            }

            // Los campos del REGISTRO son el encabezado del CSV y el orden de
            // las columnas. Si el tipo no es un registro, el archivo queda sin
            // campos y ABRIR/CREAR lo dicen — no es error declararlo, es error
            // usarlo.
            const PAEDRegistro *r = registro_de(prog, d->type);
            if (!r) continue;

            char campos[PAED_MAX_CAMPOS][PAED_NAME_MAX];
            for (int c = 0; c < r->campo_count; c++)
                snprintf(campos[c], PAED_NAME_MAX, "%s", r->campos[c].name);

            if (arch_set_campos(a, campos, r->campo_count) != 0) {
                fprintf(stderr, "%s:%d: error: %s\n", prog->path, d->line, arch_error());
                fallos++;
            }
            continue;
        }

        // Variable de tipo REGISTRO: se APLANA en una variable por campo.
        // `pori: vector2` con campos vx,vy pasa a ser "pori.vx" y "pori.vy".
        //
        // Asi el entorno no necesita saber nada de registros: para el son dos
        // variables comunes que da la casualidad que tienen un punto en el
        // nombre. El precio es que no se puede asignar un registro entero
        // (`p1 := p2`), que no aparece en el corpus.
        const PAEDRegistro *reg = registro_de(prog, d->type);
        if (!reg) continue;   // tipo escalar: nace solo en su primera asignacion

        for (int c = 0; c < reg->campo_count; c++) {
            char completo[PAED_NAME_MAX];
            snprintf(completo, sizeof(completo), "%s.%s", d->name, reg->campos[c].name);

            Valor cero = {0};
            cero.tipo = VAL_NUM;
            if (env_set(&env, completo, cero) != 0) {
                fprintf(stderr, "%s:%d: error: no entran mas variables (campo '%s')\n",
                        prog->path, d->line, completo);
                fallos++;
            }
        }
    }
    return fallos;
}

// destino := expr, donde destino puede ser un escalar, un elemento A[i] o un
// campo p.campo. El parser ya partio el destino: el nombre queda en `proc` y el
// indice, si lo hay, como el argumento "indice".
static int asignar(const PAEDProgram *prog, const PAEDInstr *in) {
    Valor v;
    if (expr_eval(in->cond, &env, &v) != 0) {
        runtime_error(prog, in, env.error);
        return -1;
    }
    return guardar_valor(prog, in, in->proc, paed_get_arg(in, "indice"), v);
}

// ¿Alguna de las etiquetas de esta rama del SEGUN es igual al selector?
//
// Las etiquetas van separadas por coma — "'B', 'M':" es la forma del template
// ACT INDEX — y se cortan a NIVEL SUPERIOR: una coma adentro de un parentesis o
// de un texto pertenece a la etiqueta y no separa nada.
//
// Cada etiqueta se EVALUA como expresion en vez de compararse como texto: asi
// una constante o una variable valen de etiqueta igual que un literal, y '1' y
// 1 no terminan siendo dos cosas distintas.
static int caso_matchea(const char *etiquetas, const Valor *sel, Entorno *env,
                        const PAEDProgram *prog, const PAEDInstr *caso, int *fallos) {
    char buf[PAED_COND_MAX];
    snprintf(buf, sizeof(buf), "%s", etiquetas);

    char *ini  = buf;
    int   prof = 0, en_texto = 0;

    for (char *c = buf; ; c++) {
        if (*c == '\'' || *c == '"') en_texto = !en_texto;
        else if (!en_texto) {
            if      (*c == '(' || *c == '[') prof++;
            else if (*c == ')' || *c == ']') prof--;
        }

        if (*c && !(prof == 0 && !en_texto && *c == ',')) continue;

        char fin = *c;
        *c = '\0';

        // trim a mano: el de parser.c no se exporta.
        char *et = ini;
        while (*et == ' ' || *et == '\t') et++;
        for (size_t n = strlen(et); n > 0 && isspace((unsigned char)et[n - 1]); n = strlen(et))
            et[n - 1] = '\0';

        if (*et) {
            Valor v;
            if (expr_eval(et, env, &v) != 0) {
                // Una etiqueta que no se puede evaluar es un error del programa,
                // no una rama que "no matchea": callarlo haria que el SEGUN
                // eligiera otra rama sin que nadie se entere.
                runtime_error(prog, caso, env->error);
                (*fallos)++;
                return 0;
            }
            if (expr_comparar(sel, &v) == 0) return 1;
        }

        if (!fin) break;
        ini = c + 1;
    }
    return 0;
}

int interp_exec(const PAEDProgram *prog) {
    env_init(&env);
    // Dos corridas en el mismo proceso (el editor corre el .paed cada vez que
    // se guarda) no pueden compartir la posicion de lectura de una secuencia:
    // la segunda arrancaria donde quedo la primera.
    sec_reset();
    arch_reset();
    int fallos_ambiente = declarar_ambiente(prog);
    if (fallos_ambiente > 0) return -1;   // sin sus arreglos, el programa no corre

    // Un PARA tiene que inicializar su variable la PRIMERA vez que se entra,
    // pero no cada vuelta. Como el FIN_PARA salta de vuelta al PARA, hace
    // falta recordar cual ya arranco. Es por instruccion y no una sola bandera
    // para que dos PARA anidados no se pisen.
    static char para_activo[PAED_MAX_INSTRS];
    memset(para_activo, 0, sizeof(para_activo));

    int fallos = 0;
    int pasos  = 0;
    int i      = 0;

    // Se sigue el hilo con un indice en vez de recorrer el array de punta a
    // punta: el flujo ya no es lineal. Esto es, literalmente, un contador de
    // programa.
    //
    // El `fallos == 0` CORTA la ejecucion en el primer error de runtime, y esa
    // asimetria con el parser es a proposito:
    //
    //   - Al PARSEAR los errores son independientes. Que falte un ';' en la
    //     linea 10 no tiene nada que ver con que falte un FIN_SI en la 40, asi
    //     que reportarlos todos ahorra una vuelta entera de corregir y volver.
    //
    //   - Al EJECUTAR son dependientes. Despues del primero el estado ya quedo
    //     sucio — variables con valores viejos, un archivo sin abrir, un
    //     registro a medio llenar — y lo que venga despues es probablemente
    //     CONSECUENCIA del primero, no un problema nuevo.
    //
    // Lo que se evitaba dejandolo seguir era peor que un error de mas: un LEER
    // que fallaba dejaba el registro con el valor ANTERIOR, y el programa
    // seguia imprimiendo totales que parecian razonables y estaban mal. Ninguno
    // de esos numeros era un error reportado: eran mentiras silenciosas.
    //
    // La condicion se mira ARRIBA del ciclo, asi que la instruccion que fallo
    // alcanza a reportar su error y recien la vuelta siguiente no arranca.
    while (i >= 0 && i < prog->instr_count && fallos == 0) {
        if (++pasos > PAED_MAX_PASOS) {
            fprintf(stderr, "%s: error: el programa paso los %d pasos sin terminar "
                            "(bucle infinito?)\n", prog->path, PAED_MAX_PASOS);
            fallos++;
            break;
        }

        const PAEDInstr *in = &prog->instrs[i];
        int ok = 1;

        switch (in->kind) {
            case PAED_LLAMADA:
                if (exec_instr(prog, in) != 0) fallos++;
                i++;
                break;

            case PAED_ASIGNA:
                if (asignar(prog, in) != 0) fallos++;
                i++;
                break;

            case PAED_SI:
                // Verdadera: se sigue derecho al cuerpo. Falsa: al SINO, o a
                // la instruccion de despues del FIN_SI.
                i = condicion(&env, prog, in, &ok) ? i + 1 : in->salto;
                if (!ok) { fallos++; i = in->salto; }
                break;

            case PAED_SINO:
                // Llegar aca EJECUTANDO significa que se termino la rama
                // verdadera, asi que hay que saltearse la rama del SINO.
                i = in->salto;
                break;

            case PAED_FIN_SI:
                i++;
                break;

            case PAED_MIENTRAS:
                i = condicion(&env, prog, in, &ok) ? i + 1 : in->salto;
                if (!ok) { fallos++; i = in->salto; }
                break;

            case PAED_PARA: {
                Valor desde, hasta, paso;
                const char *sd = paed_get_arg(in, "desde");
                const char *sh = paed_get_arg(in, "hasta");
                const char *sp = paed_get_arg(in, "paso");

                if (expr_eval(sd ? sd : "0", &env, &desde) != 0 ||
                    expr_eval(sh ? sh : "0", &env, &hasta) != 0 ||
                    expr_eval(sp ? sp : "1", &env, &paso)  != 0) {
                    runtime_error(prog, in, env.error);
                    fallos++;
                    i = in->salto;
                    break;
                }

                if (!para_activo[i]) {
                    para_activo[i] = 1;
                    env_set(&env, in->proc, desde);
                }

                Valor *v = env_buscar(&env, in->proc);
                // Con paso positivo se corta al pasarse del final; con paso
                // negativo, al bajar de el. Sin esto, un PARA en reversa
                // nunca terminaria.
                int sigue = (paso.num >= 0) ? (v->num <= hasta.num)
                                            : (v->num >= hasta.num);
                if (sigue) {
                    i++;
                } else {
                    // Se apaga la marca: si este PARA esta adentro de otro
                    // bucle, la proxima vuelta tiene que volver a arrancar.
                    para_activo[i] = 0;
                    i = in->salto;
                }
                break;
            }

            case PAED_FIN_PARA: {
                const PAEDInstr *cab = &prog->instrs[in->salto];
                Valor *v = env_buscar(&env, cab->proc);
                Valor  paso;
                const char *sp = paed_get_arg(cab, "paso");
                if (v && expr_eval(sp ? sp : "1", &env, &paso) == 0)
                    v->num += paso.num;
                i = in->salto;   // volver al PARA a re-chequear
                break;
            }

            case PAED_FIN_MIENTRAS:
                i = in->salto;   // volver a evaluar la condicion
                break;

            // SEGUN: se evalua la expresion UNA vez y se recorre la cadena de
            // ramas comparando. La cadena la armo el parser, asi que un SEGUN
            // anidado adentro de una rama no se mezcla con el de afuera.
            case PAED_SEGUN: {
                Valor sel;
                if (expr_eval(in->cond, &env, &sel) != 0) {
                    runtime_error(prog, in, env.error);
                    fallos++;
                    i = in->salto;   // al FIN_SEGUN: sin selector no hay rama que elegir
                    break;
                }

                int destino = in->salto;   // el FIN_SEGUN, si no matchea ninguna
                int defecto = -1;

                for (int k = in->salto; k >= 0 && prog->instrs[k].kind == PAED_CASO;
                     k = prog->instrs[k].siguiente) {
                    const PAEDInstr *caso = &prog->instrs[k];

                    // Sin etiquetas es la rama por defecto: se guarda y se
                    // sigue, porque una rama con etiqueta le gana aunque este
                    // escrita mas abajo.
                    if (!caso->cond[0]) { defecto = k; continue; }

                    if (caso_matchea(caso->cond, &sel, &env, prog, caso, &fallos)) {
                        destino = k + 1;   // el cuerpo arranca despues del CASO
                        defecto = -1;
                        break;
                    }
                }

                if (defecto >= 0) destino = defecto + 1;
                i = destino;
                break;
            }

            // Llegar a un CASO EJECUTANDO significa que la rama de arriba
            // termino. Se sale del SEGUN: no hay caida de una rama a la
            // siguiente, que es lo que hacen los dos templates de la catedra.
            case PAED_CASO:
                i = in->salto;
                break;

            case PAED_FIN_SEGUN:
                i++;
                break;

            // REPETIR no hace nada por si mismo: solo marca donde empieza el
            // cuerpo. El trabajo lo hace el HASTA de abajo.
            case PAED_REPETIR:
                i++;
                break;

            // POST-TEST: la condicion dice cuando TERMINAR, no cuando seguir,
            // asi que es al reves que la del MIENTRAS. Y como se evalua DESPUES
            // del cuerpo, el cuerpo siempre corrio al menos una vez.
            case PAED_HASTA:
                i = condicion(&env, prog, in, &ok) ? i + 1 : in->salto;
                if (!ok) { fallos++; i++; }   // sin condicion confiable, se sale
                break;
        }

        if (!ok && fallos > PAED_MAX_ERRORS) break;
    }

    // Cortar sin decir por que es brusco: se ve un error, el programa termina,
    // y no se sabe si termino porque llego al final o porque se corto. Solo se
    // avisa cuando quedaban instrucciones — si el error fue en la ultima, no
    // hay nada que aclarar.
    if (fallos > 0 && i >= 0 && i < prog->instr_count)
        fprintf(stderr, "%s: se corta la ejecucion: despues del primer error "
                        "el estado ya no es confiable\n", prog->path);

    return fallos == 0 ? 0 : -1;
}

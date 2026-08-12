#include "paed/interpreter.h"
#include "paed/expr.h"

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

static int exec_instr(const PAEDProgram *prog, const PAEDInstr *in) {
    const char *p = in->proc;

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
    if (strcasecmp(p, "ABRIR") == 0 || strcasecmp(p, "CERRAR") == 0) {
        char msg[PAED_MSG_MAX];
        snprintf(msg, sizeof(msg),
                 "%s todavia no esta implementado: por ahora solo se valida la declaracion", p);
        runtime_error(prog, in, msg);
        return -1;
    }

    if (in->forma == PAED_FORMA_ARCHIVO) {
        char msg[PAED_MSG_MAX];
        snprintf(msg, sizeof(msg),
                 "%s(archivo, registro) todavia no %s: falta el manejo de archivos en disco",
                 p, strcasecmp(p, "LEER") == 0 ? "avanza el archivo" : "graba");
        runtime_error(prog, in, msg);
        return -1;
    }

    if (strcasecmp(p, "LEER") == 0) return leer_consola(prog, in);

    // ── Salida por consola ───────────────────────────────────────────────────
    if (strcasecmp(p, "ESCRIBIR") == 0) {
        for (int i = 0; i < in->arg_count; i++) {
            const char *v = in->args[i].val;
            size_t n = strlen(v);

            if (n >= 2 && v[0] == '"' && v[n - 1] == '"') {
                printf("%.*s", (int)(n - 2), v + 1);   // texto literal, sin comillas
                continue;
            }

            // Lo que no es literal se EVALUA: antes ESCRIBIR(cont_pal)
            // imprimia "cont_pal", el nombre, en vez del valor.
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

        if (d->es_arreglo) {
            if (env_declarar_arreglo(&env, d->name, d->desde, d->hasta) != 0) {
                fprintf(stderr, "%s:%d: error: %s\n", prog->path, d->line, env.error);
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

int interp_exec(const PAEDProgram *prog) {
    env_init(&env);
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
    while (i >= 0 && i < prog->instr_count) {
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
        }

        if (!ok && fallos > PAED_MAX_ERRORS) break;
    }

    return fallos == 0 ? 0 : -1;
}

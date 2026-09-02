#include "paed/parser.h"
#include "paed/errores.h"
#include "sintaxis.h"
#include "texto.h"
#include "reporte.h"
#include "programa.h"
#include "bloques.h"
#include "sentencias.h"
#include "grafias.h"
#include "ambiente.h"
#include "instruccion.h"
#include "cJSON.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "paed/plataforma.h"   // saber donde esta el binario, sin #ifdef aca


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
            // Nombrar las librerias que SI se cargaron es media respuesta:
            // el que se olvido un USAR ve cual falta comparando.
            char cargadas[256];
            if (syn_libs_nombres(cargadas, sizeof(cargadas)) > 0) {
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

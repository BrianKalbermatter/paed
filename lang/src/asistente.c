#include "paed/asistente.h"

#include <stdarg.h>
#include <stdio.h>
#include <strings.h>   // strcasecmp: los nombres de archivo y tipo no distinguen mayusculas

#include "paed/archivo.h"
#include "paed/colores.h"
#include "paed/parser.h"

// ═══════════════════════════════════════════════════════════════════════════
//  LA LOGICA
//
//  Nada de esta mitad imprime ni lee del teclado: devuelve datos. Es la que
//  usa el menu de consola de mas abajo, y la que puede usar cualquier otro
//  front-end. Una sola logica con dos caras — si fueran dos, se irian
//  separando sin que nadie se de cuenta.
// ═══════════════════════════════════════════════════════════════════════════

// El REGISTRO que se le puso al archivo, o NULL si ese tipo no existe.
//
// Es la pregunta que hace fallar el ABRIR, y el asistente la puede contestar
// ANTES de correr nada: el AMBIENTE ya esta parseado. Ademas hace falta el
// registro entero, no un si/no: sus campos son el encabezado del .csv.
static const PAEDRegistro *registro_de(const PAEDProgram *prog, const char *tipo) {
    for (int i = 0; i < prog->registro_count; i++)
        if (strcasecmp(prog->registros[i].name, tipo) == 0) return &prog->registros[i];
    return NULL;
}

static int existe(const char *ruta) {
    FILE *f = fopen(ruta, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

// Donde iria el .csv de este archivo.
//
// No se arma la ruta a mano: se llama a arch_declarar(), la MISMA funcion que
// usa el interprete para saber donde buscarlo. Si el asistente la armara por su
// cuenta, el dia que esa regla cambiara crearia un archivo que el programa
// despues no encuentra — y ese error no se ve, se sufre.
static void ruta_csv(const PAEDProgram *prog, const char *nombre,
                     char *out, size_t n) {
    char dir[PAED_PATH_MAX];
    arch_dir_del_programa(prog->path, dir, sizeof(dir));

    arch_reset();
    Archivo *a = arch_declarar(nombre, dir);
    snprintf(out, n, "%s", a ? a->ruta : "");
}

// Llena la ficha de una declaracion de archivo.
static void ficha(const PAEDProgram *prog, const PAEDDecl *d, PAEDArchivoInfo *out) {
    snprintf(out->nombre, sizeof(out->nombre), "%s", d->name);
    snprintf(out->tipo,   sizeof(out->tipo),   "%s", d->type);
    // Vacio significa secuencial, y es una organizacion legitima y no un olvido:
    // el corpus escribe 'Archivo SECUENCIAL (no ordenado)'.
    snprintf(out->organizacion, sizeof(out->organizacion), "%s",
             d->org[0] ? d->org : "secuencial");
    out->linea            = d->line;
    out->tipo_es_registro = registro_de(prog, d->type) != NULL;

    ruta_csv(prog, d->name, out->csv, sizeof(out->csv));
    out->csv_existe = out->csv[0] ? existe(out->csv) : 0;
}

int paed_asistente_archivos(const PAEDProgram *prog, PAEDArchivoInfo *out, int max) {
    if (!prog || !out || max <= 0) return 0;

    int n = 0;
    for (int i = 0; i < prog->decl_count && n < max; i++)
        if (prog->decls[i].es_archivo) ficha(prog, &prog->decls[i], &out[n++]);
    return n;
}

int paed_asistente_en_linea(const PAEDProgram *prog, int linea, PAEDArchivoInfo *out) {
    if (!prog) return 0;

    for (int i = 0; i < prog->decl_count; i++) {
        if (!prog->decls[i].es_archivo)      continue;
        if (prog->decls[i].line != linea)    continue;
        if (out) ficha(prog, &prog->decls[i], out);
        return 1;
    }
    return 0;
}

int paed_asistente_opciones(PAEDOpcion *out, int max) {
    if (!out || max <= 0) return -1;

    PAEDOrganizacion orgs[PAED_MAX_ORGANIZACIONES];
    int n_orgs = paed_organizaciones(orgs, PAED_MAX_ORGANIZACIONES);
    if (n_orgs < 0) return -1;

    int n = 0;
    for (int i = 0; i < n_orgs && n < max; i++) {
        if (!orgs[i].en_asistente) continue;

        snprintf(out[n].etiqueta,     sizeof(out[n].etiqueta),     "%s", orgs[i].etiqueta);
        snprintf(out[n].descripcion,  sizeof(out[n].descripcion),  "%s", orgs[i].descripcion);
        snprintf(out[n].organizacion, sizeof(out[n].organizacion), "%s", orgs[i].nombre);
        snprintf(out[n].clausula,     sizeof(out[n].clausula),     "%s",
                 orgs[i].clausula ? orgs[i].clausula : "");
        out[n].implementado = orgs[i].implementado;
        n++;
    }
    return n;
}

// Una sola salida para el mensaje, para no repetir el snprintf con el NULL.
static PAEDAsistResultado con_msg(PAEDAsistResultado r, char *msg, size_t n,
                                  const char *fmt, ...) {
    if (msg && n) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(msg, n, fmt, ap);
        va_end(ap);
    }
    return r;
}

PAEDAsistResultado paed_asistente_crear(const PAEDProgram *prog,
                                        const char *nombre,
                                        const char *organizacion,
                                        char *msg, size_t msg_n) {
    if (!prog || !nombre)
        return con_msg(PAED_ASIST_ERROR, msg, msg_n, "falta el programa o el nombre");

    // La declaracion, para saber de que REGISTRO es.
    const PAEDDecl *d = NULL;
    for (int i = 0; i < prog->decl_count; i++)
        if (prog->decls[i].es_archivo && strcasecmp(prog->decls[i].name, nombre) == 0) {
            d = &prog->decls[i];
            break;
        }
    if (!d)
        return con_msg(PAED_ASIST_ERROR, msg, msg_n,
                       "'%s' no esta declarado como ARCHIVO en el AMBIENTE", nombre);

    const PAEDRegistro *r = registro_de(prog, d->type);
    if (!r)
        return con_msg(PAED_ASIST_SIN_REGISTRO, msg, msg_n,
                       "'%s' no es un REGISTRO declarado, asi que no se sabe que "
                       "columnas lleva el archivo", d->type);

    char dir[PAED_PATH_MAX];
    arch_dir_del_programa(prog->path, dir, sizeof(dir));

    arch_reset();
    Archivo *a = arch_declarar(d->name, dir);
    if (!a) return con_msg(PAED_ASIST_ERROR, msg, msg_n, "%s", arch_error());

    char campos[PAED_MAX_CAMPOS][PAED_NAME_MAX];
    for (int c = 0; c < r->campo_count; c++)
        snprintf(campos[c], PAED_NAME_MAX, "%s", r->campos[c].name);

    if (arch_set_campos(a, campos, r->campo_count) != 0)
        return con_msg(PAED_ASIST_ERROR, msg, msg_n, "%s", arch_error());

    // arch_crear PISA el que exista, y para el interprete esta bien: el maestro
    // nuevo se rehace en cada corrida. Para el asistente NO: el .csv puede
    // tener los datos que cargaste a mano.
    if (existe(a->ruta))
        return con_msg(PAED_ASIST_YA_EXISTIA, msg, msg_n,
                       "%s ya existe: no se toco", a->ruta);

    if (arch_crear(a, "S") != 0)
        return con_msg(PAED_ASIST_ERROR, msg, msg_n, "%s", arch_error());
    arch_cerrar(a);

    // Si la organizacion elegida todavia no se ejecuta, el archivo se creo
    // igual — el .csv es el mismo — pero hay que decirlo, porque el error
    // aparecería lejos, recien en el ABRIR.
    PAEDOrganizacion orgs[PAED_MAX_ORGANIZACIONES];
    int n_orgs = paed_organizaciones(orgs, PAED_MAX_ORGANIZACIONES);
    for (int i = 0; organizacion && i < n_orgs; i++)
        if (strcasecmp(orgs[i].nombre, organizacion) == 0 && !orgs[i].implementado)
            return con_msg(PAED_ASIST_CREADO, msg, msg_n,
                           "creado %s, pero el interprete todavia no ejecuta "
                           "archivos '%s'", a->ruta, organizacion);

    return con_msg(PAED_ASIST_CREADO, msg, msg_n, "creado %s", a->ruta);
}

// ═══════════════════════════════════════════════════════════════════════════
//  EL FRONT-END DE CONSOLA
//
//  `paed asistente <archivo.paed>`. Un menu de texto sobre la MISMA logica de
//  arriba: aca solo se dibuja.
// ═══════════════════════════════════════════════════════════════════════════

// Se usan los mismos roles que el resaltador, y no colores elegidos a mano. Que
// el nombre de un archivo se vea igual en el menu que en el codigo no es
// decoracion: es la misma pregunta contestada por el mismo parser.
static const char *color(const char *rol) { return paed_rol_ansi(rol); }
static const char *reset(void)            { return paed_ansi_reset(); }

// Devuelve la opcion elegida, o -1 si el usuario corto la entrada (Ctrl-D, o
// una tuberia que se acabo). -1 no es un error: es "no hay nadie del otro
// lado", y el asistente tiene que poder salir sin colgarse.
static int leer_opcion(const char *prompt) {
    printf("\n  %s ", prompt);
    fflush(stdout);

    char linea[64];
    if (!fgets(linea, sizeof(linea), stdin)) {
        printf("\n");
        return -1;
    }

    int n = 0;
    if (sscanf(linea, "%d", &n) != 1) return -2;   // no escribio un numero
    return n;
}

static void mostrar_archivo(const PAEDProgram *prog, const PAEDArchivoInfo *a,
                            int numero) {
    printf("    %d) %s%s%s", numero, color("archivos"), a->nombre, reset());
    printf("   linea %d\n", a->linea);
    printf("       ARCHIVO DE %s%s%s\n",
           color(a->tipo_es_registro ? "tipos_usuario" : "variables"),
           a->tipo, reset());
    printf("       tipo de archivo hoy: %s\n", a->organizacion);
    if (a->csv_existe) printf("       %s ya existe\n", a->csv);

    // El diagnostico que importa: si el tipo no es un REGISTRO, ABRIR va a
    // fallar. Se avisa aca, al lado de la declaracion que lo causa, y no en
    // runtime veinte lineas mas abajo.
    if (!a->tipo_es_registro) {
        printf("       ojo: '%s' no es un REGISTRO declarado.", a->tipo);
        if (prog->registro_count == 0) {
            printf(" No hay ninguno en el AMBIENTE.\n");
        } else {
            printf(" Los que hay son:");
            for (int i = 0; i < prog->registro_count; i++)
                printf(" %s%s%s%s", color("tipos_usuario"), prog->registros[i].name,
                       reset(), i + 1 < prog->registro_count ? "," : "");
            printf("\n");
        }
    }
}

static void menu_tipos(const PAEDProgram *prog, const PAEDArchivoInfo *a) {
    printf("\n  Tipo de archivo para %s%s%s:\n\n",
           color("archivos"), a->nombre, reset());

    PAEDOpcion ops[PAED_MAX_ORGANIZACIONES];
    int n = paed_asistente_opciones(ops, PAED_MAX_ORGANIZACIONES);
    if (n <= 0) {
        printf("      (no hay ningun tipo habilitado en sintaxis.json)\n");
        return;
    }

    for (int i = 0; i < n; i++) {
        printf("      %d) %s\n", i + 1, ops[i].etiqueta);
        printf("         %s\n", ops[i].descripcion);
        printf("         se declara %s\n",
               ops[i].clausula[0] ? ops[i].clausula : "SIN clausula");
    }
    printf("\n      0) volver\n");

    int op = leer_opcion("opcion:");
    if (op <= 0 || op > n) return;

    const PAEDOpcion *elegida = &ops[op - 1];

    printf("\n  %s: la declaracion queda\n\n", elegida->etiqueta);
    printf("      %s: ARCHIVO DE %s%s%s;\n",
           a->nombre, a->tipo,
           elegida->clausula[0] ? " " : "", elegida->clausula);

    if (!elegida->clausula[0]) {
        // Es el caso de secuencial, y hay que decirlo con todas las letras:
        // "no lleva clausula" no es que falte algo, es la forma correcta. La
        // catedra escribe 'Archivo SECUENCIAL (no ordenado)' justamente asi.
        printf("\n  Un %s se declara sin clausula: la organizacion es la\n",
               elegida->etiqueta);
        printf("  ausencia de ORDENADO POR / INDEXADO POR, no una palabra que\n");
        printf("  haya que escribir. La linea %d ya esta asi.\n", a->linea);
    }

    char msg[PAED_MSG_MAX];
    paed_asistente_crear(prog, a->nombre, elegida->organizacion, msg, sizeof(msg));
    printf("\n  %s\n", msg);
}

int paed_asistente(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "uso: paed asistente <archivo.paed>\n");
        return 2;
    }
    const char *path = argv[1];

    if (paed_syntax_load() != 0) return 3;

    // Se parsea IGNORANDO si fallo: el AMBIENTE ya esta cargado y el asistente
    // sirve justamente cuando el archivo esta a medio escribir.
    PAEDProgram prog;
    paed_parse_file(path, &prog);

    PAEDArchivoInfo archivos[PAED_MAX_DECLS];
    int count = paed_asistente_archivos(&prog, archivos, PAED_MAX_DECLS);

    printf("\n  ASISTENTE DE ARCHIVOS\n");
    printf("  %s\n", path);

    if (count == 0) {
        printf("\n  No hay ningun ARCHIVO declarado en el AMBIENTE.\n\n");
        printf("  Se declara asi:\n\n");
        printf("      <nombre>: ARCHIVO DE <un REGISTRO del AMBIENTE>;\n\n");
        printf("  Escribi la declaracion y volve a correr el asistente:\n");
        printf("  el menu de tipos de archivo aparece cuando hay uno.\n\n");
        paed_syntax_free();
        return 0;
    }

    printf("\n  Archivos declarados en el AMBIENTE:\n\n");
    for (int i = 0; i < count; i++)
        mostrar_archivo(&prog, &archivos[i], i + 1);

    // Con un solo archivo no se pregunta cual: preguntar algo que tiene una
    // sola respuesta es hacerle perder el tiempo al que lo usa.
    int elegido = 0;
    if (count > 1) {
        int op = leer_opcion("¿sobre cual? (numero, 0 para salir)");
        if (op <= 0 || op > count) { printf("\n"); paed_syntax_free(); return 0; }
        elegido = op - 1;
    }

    menu_tipos(&prog, &archivos[elegido]);
    printf("\n");

    paed_syntax_free();
    return 0;
}

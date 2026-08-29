#include "paed/datos.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   // strcasecmp: los nombres no distinguen mayusculas

#include "paed/archivo.h"
#include "paed/asistente.h"
#include "paed/colores.h"

// El buffer de las filas. Estatico y no de la pila: medio mega no entra en un
// frame, y con un solo cargador a la vez no hace falta que sea reentrante.
char paed_datos_celdas[PAED_MAX_FILAS][PAED_MAX_CAMPOS][PAED_VAL_MAX];

// ═══════════════════════════════════════════════════════════════════════════
//  LA LOGICA
// ═══════════════════════════════════════════════════════════════════════════

// El REGISTRO de un tipo, o NULL. Es la misma busqueda que hacen el interprete
// y el asistente: los tres necesitan los campos del registro y ninguno puede
// pedirselos al otro sin que el modulo de abajo dependa del de arriba.
static const PAEDRegistro *registro_de(const PAEDProgram *prog, const char *tipo) {
    for (int i = 0; i < prog->registro_count; i++)
        if (strcasecmp(prog->registros[i].name, tipo) == 0) return &prog->registros[i];
    return NULL;
}

// La declaracion de un ARCHIVO del AMBIENTE, o NULL.
static const PAEDDecl *archivo_de(const PAEDProgram *prog, const char *nombre) {
    for (int i = 0; i < prog->decl_count; i++)
        if (prog->decls[i].es_archivo &&
            strcasecmp(prog->decls[i].name, nombre) == 0) return &prog->decls[i];
    return NULL;
}

// Una sola salida para el mensaje, para no repetir el snprintf con el NULL.
static PAEDDatosResultado con_msg(PAEDDatosResultado r, char *msg, size_t n,
                                  const char *fmt, ...) {
    if (msg && n) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(msg, n, fmt, ap);
        va_end(ap);
    }
    return r;
}

// ¿El campo guarda un numero? Lo dice el TIPO declarado, no el dato.
//
// Es la misma regla que aplica el interprete al leer una fila (leer_archivo en
// interpreter.c). Tiene que ser la misma: si el cargador aceptara un valor que
// despues el interprete rechaza, el .csv quedaria escrito con un dato que
// revienta recien al correr el programa — lejos de donde se tipeo.
static int campo_es_numero(const char *tipo) {
    return strncasecmp(tipo, "ENTERO", 6) == 0 ||
           strncasecmp(tipo, "REAL",   4) == 0 ||
           (toupper((unsigned char)tipo[0]) == 'N' && tipo[1] == '(');
}

// ¿El tipo admite decimales? Un ENTERO con '3.5' adentro es un error, y la
// catedra lo trata como error: son tipos distintos, no dos formas del mismo.
static int campo_es_entero(const char *tipo) {
    return strncasecmp(tipo, "ENTERO", 6) == 0 ||
           (toupper((unsigned char)tipo[0]) == 'N' && tipo[1] == '(' &&
            !strchr(tipo, ','));
}

PAEDDatosResultado paed_datos_tabla(const PAEDProgram *prog, const char *nombre,
                                    PAEDTabla *out, char *msg, size_t msg_n) {
    if (!prog || !nombre || !out)
        return con_msg(PAED_DATOS_ERROR, msg, msg_n, "falta el programa o el nombre");

    memset(out, 0, sizeof(*out));

    const PAEDDecl *d = archivo_de(prog, nombre);
    if (!d)
        return con_msg(PAED_DATOS_SIN_ARCHIVO, msg, msg_n,
                       "'%s' no esta declarado como ARCHIVO en el AMBIENTE", nombre);

    const PAEDRegistro *r = registro_de(prog, d->type);
    if (!r)
        return con_msg(PAED_DATOS_SIN_REGISTRO, msg, msg_n,
                       "'%s' no es un REGISTRO declarado, asi que no se sabe que "
                       "columnas lleva el archivo", d->type);

    // Las columnas salen del REGISTRO, en su orden de declaracion. Ese orden es
    // tambien el del encabezado del CSV: no hay una segunda lista que mantener.
    out->campo_count = r->campo_count;
    for (int c = 0; c < r->campo_count; c++) {
        snprintf(out->campos[c], PAED_NAME_MAX, "%s", r->campos[c].name);
        snprintf(out->tipos [c], PAED_NAME_MAX, "%s", r->campos[c].type);
    }

    // La clave sale de la clausula, y se traduce de NOMBRES a INDICES una sola
    // vez aca. Ordenar mil veces buscando el campo por nombre seria hacer el
    // mismo strcasecmp en cada comparacion.
    for (int k = 0; k < d->clave_count && k < PAED_MAX_CLAVE; k++) {
        for (int c = 0; c < out->campo_count; c++) {
            if (strcasecmp(out->campos[c], d->clave[k]) == 0) {
                out->clave[out->clave_count++] = c;
                break;
            }
        }
    }

    return con_msg(PAED_DATOS_OK, msg, msg_n, "%s: %d columnas, clave de %d",
                   nombre, out->campo_count, out->clave_count);
}

int paed_datos_cargar_csv(const PAEDProgram *prog, const char *nombre, PAEDTabla *t) {
    if (!prog || !nombre || !t) return -1;

    char dir[PAED_PATH_MAX];
    arch_dir_del_programa(prog->path, dir, sizeof(dir));

    arch_reset();
    Archivo *a = arch_declarar(nombre, dir);
    if (!a) return -1;

    char campos[PAED_MAX_CAMPOS][PAED_NAME_MAX];
    for (int c = 0; c < t->campo_count; c++)
        snprintf(campos[c], PAED_NAME_MAX, "%s", t->campos[c]);
    if (arch_set_campos(a, campos, t->campo_count) != 0) return -1;

    // arch_abrir VALIDA el encabezado contra los campos del REGISTRO. Que falle
    // aca es informacion, no un accidente: significa que el .csv que hay en
    // disco es de otro registro, y agregarle filas lo empeoraria.
    if (arch_abrir(a, "E") != 0) return -1;

    int traidas = 0;
    for (;;) {
        if (t->filas >= PAED_MAX_FILAS) break;

        char valores[PAED_MAX_CAMPOS][PAED_VAL_MAX];
        int  hay = 0;
        if (arch_leer(a, valores, &hay) != 0) { arch_cerrar(a); return -1; }
        if (!hay) break;

        for (int c = 0; c < t->campo_count; c++)
            snprintf(paed_datos_celdas[t->filas][c], PAED_VAL_MAX, "%s", valores[c]);
        t->filas++;
        traidas++;
    }

    arch_cerrar(a);
    return traidas;
}

int paed_datos_valor_valido(const PAEDTabla *t, int campo, const char *valor,
                            char *msg, size_t msg_n) {
    if (!t || campo < 0 || campo >= t->campo_count || !valor) return 0;

    // El separador del CSV adentro de un valor partiria la fila en dos al
    // releerla, y el error aparecería como una columna corrida. Se ataja al
    // tipearlo, que es el unico momento en que se puede explicar.
    if (strchr(valor, PAED_CSV_SEP)) {
        if (msg) snprintf(msg, msg_n,
                          "el valor no puede llevar '%c': es el separador del CSV",
                          PAED_CSV_SEP);
        return 0;
    }

    if (!campo_es_numero(t->tipos[campo])) return 1;

    // Numerico: se acepta lo mismo que acepta el interprete al leer la fila.
    // Un campo vacio tambien vale, y no por descuido: en un archivo de
    // movimientos el campo vacio es lo que significa "esta no se modifica".
    if (valor[0] == '\0') return 1;

    const char *p = valor;
    if (*p == '+' || *p == '-') p++;

    int digitos = 0, puntos = 0;
    for (; *p; p++) {
        if (isdigit((unsigned char)*p)) { digitos++; continue; }
        if (*p == '.')                  { puntos++;  continue; }
        if (msg) snprintf(msg, msg_n,
                          "'%s' es %s y '%c' no es parte de un numero",
                          t->campos[campo], t->tipos[campo], *p);
        return 0;
    }

    if (digitos == 0 || puntos > 1) {
        if (msg) snprintf(msg, msg_n, "'%s' es %s: '%s' no es un numero",
                          t->campos[campo], t->tipos[campo], valor);
        return 0;
    }

    if (puntos && campo_es_entero(t->tipos[campo])) {
        if (msg) snprintf(msg, msg_n,
                          "'%s' es %s: no lleva decimales. Para eso el campo "
                          "tendria que declararse REAL",
                          t->campos[campo], t->tipos[campo]);
        return 0;
    }

    return 1;
}

// Compara una columna de dos filas. Devuelve <0, 0 o >0.
//
// El tipo declarado decide COMO se compara, y no es un refinamiento: en una
// columna numerica '10' va despues de '9', y comparando como texto va antes,
// porque '1' < '9' en ASCII. Un maestro ordenado asi hace fallar la
// actualizacion en el registro numero diez.
static int cmp_columna(const PAEDTabla *t, int col, int fa, int fb) {
    const char *a = paed_datos_celdas[fa][col];
    const char *b = paed_datos_celdas[fb][col];

    if (campo_es_numero(t->tipos[col])) {
        double da = atof(a), db = atof(b);
        if (da < db) return -1;
        if (da > db) return  1;
        return 0;
    }
    return strcmp(a, b);
}

// Compara la clave entera, campo por campo y en el orden de la clausula. El
// primero que difiere decide: es lo mismo que hace el corte de control al
// mirar de mayor a menor jerarquia.
static int cmp_clave(const PAEDTabla *t, int fa, int fb) {
    for (int k = 0; k < t->clave_count; k++) {
        int c = cmp_columna(t, t->clave[k], fa, fb);
        if (c) return c;
    }
    return 0;
}

void paed_datos_ordenar(PAEDTabla *t) {
    if (!t || t->clave_count == 0 || t->filas < 2) return;

    // Insercion, y no algo mas rapido, por dos motivos. Es ESTABLE, que es un
    // requisito y no una preferencia (ver datos.h). Y con decenas de filas la
    // diferencia con un quicksort no se mide: lo que se gana en velocidad se
    // pierde en un codigo que ya no se lee de un vistazo.
    static char fila[PAED_MAX_CAMPOS][PAED_VAL_MAX];

    for (int i = 1; i < t->filas; i++) {
        memcpy(fila, paed_datos_celdas[i], sizeof(fila));

        // La fila que se esta insertando se guardo aparte, asi que para
        // compararla se la deja parada en el hueco j+1 mientras baja.
        int j = i - 1;
        while (j >= 0) {
            memcpy(paed_datos_celdas[j + 1], fila, sizeof(fila));
            if (cmp_clave(t, j, j + 1) <= 0) break;   // <= mantiene la estabilidad
            memcpy(paed_datos_celdas[j + 1], paed_datos_celdas[j], sizeof(fila));
            j--;
        }
        memcpy(paed_datos_celdas[j + 1], fila, sizeof(fila));
    }
}

int paed_datos_claves_repetidas(const PAEDTabla *t) {
    if (!t || t->clave_count == 0) return 0;

    int n = 0;
    for (int i = 1; i < t->filas; i++)
        if (cmp_clave(t, i - 1, i) == 0) n++;
    return n;
}

PAEDDatosResultado paed_datos_escribir(const PAEDProgram *prog, const char *nombre,
                                       const PAEDTabla *t, char *msg, size_t msg_n) {
    if (!prog || !nombre || !t)
        return con_msg(PAED_DATOS_ERROR, msg, msg_n, "falta el programa o el nombre");

    char dir[PAED_PATH_MAX];
    arch_dir_del_programa(prog->path, dir, sizeof(dir));

    arch_reset();
    Archivo *a = arch_declarar(nombre, dir);
    if (!a) return con_msg(PAED_DATOS_ERROR, msg, msg_n, "%s", arch_error());

    char campos[PAED_MAX_CAMPOS][PAED_NAME_MAX];
    for (int c = 0; c < t->campo_count; c++)
        snprintf(campos[c], PAED_NAME_MAX, "%s", t->campos[c]);
    if (arch_set_campos(a, campos, t->campo_count) != 0)
        return con_msg(PAED_DATOS_ERROR, msg, msg_n, "%s", arch_error());

    if (arch_crear(a, "S") != 0)
        return con_msg(PAED_DATOS_ERROR, msg, msg_n, "%s", arch_error());

    for (int f = 0; f < t->filas; f++) {
        char valores[PAED_MAX_CAMPOS][PAED_VAL_MAX];
        for (int c = 0; c < t->campo_count; c++)
            snprintf(valores[c], PAED_VAL_MAX, "%s", paed_datos_celdas[f][c]);

        if (arch_grabar(a, valores) != 0) {
            arch_cerrar(a);
            return con_msg(PAED_DATOS_ERROR, msg, msg_n, "%s", arch_error());
        }
    }

    arch_cerrar(a);
    return con_msg(PAED_DATOS_OK, msg, msg_n, "%d filas en %s", t->filas, a->ruta);
}

// ═══════════════════════════════════════════════════════════════════════════
//  EL FRONT-END DE CONSOLA
//
//  `paed datos <archivo.paed>`. Menu de texto sobre la MISMA logica de arriba.
// ═══════════════════════════════════════════════════════════════════════════

static const char *color(const char *rol) { return paed_rol_ansi(rol); }
static const char *reset(void)            { return paed_ansi_reset(); }

// Lee una linea y le saca el salto. Devuelve 0 si no hay nadie del otro lado
// (Ctrl-D, o una tuberia que se acabo): eso no es un error, es "cortaron".
static int leer_linea(char *out, size_t n) {
    if (!fgets(out, (int)n, stdin)) return 0;
    out[strcspn(out, "\r\n")] = '\0';
    return 1;
}

static int leer_numero(const char *prompt) {
    printf("\n  %s ", prompt);
    fflush(stdout);

    char linea[64];
    if (!leer_linea(linea, sizeof(linea))) { printf("\n"); return -1; }

    int n = 0;
    if (sscanf(linea, "%d", &n) != 1) return -2;
    return n;
}

// ¿Esta columna forma parte de la clave? Sirve para marcarla en pantalla: al
// tipear, saber cual campo decide el orden cambia lo que uno carga.
static int es_clave(const PAEDTabla *t, int campo) {
    for (int k = 0; k < t->clave_count; k++)
        if (t->clave[k] == campo) return 1;
    return 0;
}

static void mostrar_columnas(const PAEDTabla *t, const PAEDArchivoInfo *info) {
    printf("\n  Columnas de %s%s%s, sacadas del REGISTRO %s%s%s:\n\n",
           color("archivos"), info->nombre, reset(),
           color("tipos_usuario"), info->tipo, reset());

    for (int c = 0; c < t->campo_count; c++) {
        printf("      %-20s %-12s", t->campos[c], t->tipos[c]);
        if (es_clave(t, c)) printf("  <- clave");
        printf("\n");
    }

    printf("\n  Tipo de archivo: %s.", info->organizacion);
    if (t->clave_count == 0) {
        // No es un olvido y hay que decirlo: 'Archivo SECUENCIAL (no ordenado)'
        // es una declaracion legitima del corpus, y sus filas se guardan en el
        // orden en que se cargan.
        printf(" Sin clausula, asi que las filas quedan\n");
        printf("  en el orden en que las cargues.\n");
    } else {
        printf(" Las filas se van a ordenar por");
        for (int k = 0; k < t->clave_count; k++)
            printf(" %s%s", t->campos[t->clave[k]],
                   k + 1 < t->clave_count ? "," : "");
        printf(".\n");
    }
}

static void mostrar_tabla(const PAEDTabla *t) {
    if (t->filas == 0) return;

    printf("\n  %d fila%s:\n\n", t->filas, t->filas == 1 ? "" : "s");
    printf("      ");
    for (int c = 0; c < t->campo_count; c++) printf("%-16s", t->campos[c]);
    printf("\n");

    for (int f = 0; f < t->filas; f++) {
        printf("      ");
        for (int c = 0; c < t->campo_count; c++)
            printf("%-16s", paed_datos_celdas[f][c]);
        printf("\n");
    }
}

// Carga filas desde el teclado hasta que el usuario corte. Devuelve cuantas
// agrego.
static int cargar_filas(PAEDTabla *t) {
    printf("\n  Cargá las filas. ENTER vacío en el primer campo termina.\n");

    int agregadas = 0;
    while (t->filas < PAED_MAX_FILAS) {
        printf("\n  ── fila %d ──\n", t->filas + 1);

        int cortado = 0;
        for (int c = 0; c < t->campo_count; c++) {
            for (;;) {
                printf("     %s (%s)%s: ", t->campos[c], t->tipos[c],
                       es_clave(t, c) ? " [clave]" : "");
                fflush(stdout);

                char valor[PAED_VAL_MAX];
                if (!leer_linea(valor, sizeof(valor))) { cortado = 1; break; }

                // El corte va SOLO en el primer campo. A mitad de una fila un
                // ENTER vacio es un campo vacio, que es un dato valido: cortar
                // ahi perderia lo ya tipeado sin avisar.
                if (c == 0 && valor[0] == '\0') { cortado = 1; break; }

                char msg[PAED_MSG_MAX] = {0};
                if (!paed_datos_valor_valido(t, c, valor, msg, sizeof(msg))) {
                    printf("     %s\n", msg);
                    continue;
                }

                snprintf(paed_datos_celdas[t->filas][c], PAED_VAL_MAX, "%s", valor);
                break;
            }
            if (cortado) break;
        }
        if (cortado) break;

        t->filas++;
        agregadas++;
    }

    if (t->filas >= PAED_MAX_FILAS)
        printf("\n  Llegaste a %d filas, que es el maximo.\n", PAED_MAX_FILAS);

    return agregadas;
}

int paed_datos(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "uso: paed datos <archivo.paed>\n");
        return 2;
    }
    const char *path = argv[1];

    if (paed_syntax_load() != 0) return 3;

    // Se parsea IGNORANDO si fallo, igual que el asistente: el AMBIENTE ya esta
    // cargado, y este comando existe justamente para usarse cuando el PROCESO
    // todavia no esta escrito.
    PAEDProgram prog;
    paed_parse_file(path, &prog);

    PAEDArchivoInfo archivos[PAED_MAX_DECLS];
    int count = paed_asistente_archivos(&prog, archivos, PAED_MAX_DECLS);

    printf("\n  CARGADOR DE DATOS\n");
    printf("  %s\n", path);

    if (count == 0) {
        printf("\n  No hay ningun ARCHIVO declarado en el AMBIENTE.\n\n");
        printf("  Este comando lee de ahi las columnas y la clave, asi que\n");
        printf("  primero va la declaracion:\n\n");
        printf("      <nombre>: ARCHIVO DE <un REGISTRO> ORDENADO POR <campo>;\n\n");
        paed_syntax_free();
        return 0;
    }

    printf("\n  Archivos declarados en el AMBIENTE:\n\n");
    for (int i = 0; i < count; i++) {
        printf("    %d) %s%s%s   linea %d   ARCHIVO DE %s%s%s   (%s)%s\n",
               i + 1, color("archivos"), archivos[i].nombre, reset(),
               archivos[i].linea,
               color("tipos_usuario"), archivos[i].tipo, reset(),
               archivos[i].organizacion,
               archivos[i].csv_existe ? "  el .csv ya existe" : "");
    }

    int elegido = 0;
    if (count > 1) {
        int op = leer_numero("¿cual cargas? (numero, 0 para salir)");
        if (op <= 0 || op > count) { printf("\n"); paed_syntax_free(); return 0; }
        elegido = op - 1;
    }
    const PAEDArchivoInfo *info = &archivos[elegido];

    PAEDTabla t;
    char msg[PAED_MSG_MAX];
    PAEDDatosResultado r = paed_datos_tabla(&prog, info->nombre, &t, msg, sizeof(msg));
    if (r != PAED_DATOS_OK) {
        printf("\n  %s\n\n", msg);
        paed_syntax_free();
        return 1;
    }

    mostrar_columnas(&t, info);

    // Lo que ya estaba en disco entra primero. Es la diferencia entre agregarle
    // tres filas a un archivo armado y tener que volver a tipearlo entero.
    if (info->csv_existe) {
        int traidas = paed_datos_cargar_csv(&prog, info->nombre, &t);
        if (traidas < 0) {
            printf("\n  No se pudo leer %s: %s\n", info->csv, arch_error());
            printf("  Cargar encima seria pisarlo, asi que no se toca.\n\n");
            paed_syntax_free();
            return 1;
        }
        printf("\n  Se trajeron %d fila%s de %s.\n",
               traidas, traidas == 1 ? "" : "s", info->csv);
        mostrar_tabla(&t);
    }

    cargar_filas(&t);

    if (t.filas == 0) {
        printf("\n  No hay ninguna fila, asi que no se escribio nada.\n\n");
        paed_syntax_free();
        return 0;
    }

    paed_datos_ordenar(&t);
    mostrar_tabla(&t);

    int rep = paed_datos_claves_repetidas(&t);
    if (rep > 0) {
        printf("\n  %d fila%s repite%s la clave de la anterior.\n",
               rep, rep == 1 ? "" : "s", rep == 1 ? "" : "n");
        printf("  En un MAESTRO eso es un error: la clave lo identifica.\n");
        printf("  En un archivo de MOVIMIENTOS es lo normal — varios\n");
        printf("  movimientos de la misma clave, que es la version POR LOTES.\n");
    }

    r = paed_datos_escribir(&prog, info->nombre, &t, msg, sizeof(msg));
    printf("\n  %s\n\n", msg);

    paed_syntax_free();
    return r == PAED_DATOS_OK ? 0 : 1;
}

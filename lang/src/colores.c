#include "paed/colores.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>   // strcasecmp: los nombres declarados no distinguen mayusculas

// ── De nombre de color a ANSI ────────────────────────────────────────────────
//
// sintaxis.json dice 'naranja', no '\033[38;5;208m', y esta bien que sea asi:
// el nombre del color es del LENGUAJE, el codigo ANSI es de la TERMINAL. Cuando
// editorBim dibuje esto no va a escribir secuencias de escape, va a pintar
// pixeles — y va a leer el mismo nombre.
//
// Son colores de 256, no los 8 basicos: con 8 no alcanza para distinguir un
// tipo del lenguaje de un tipo del programador, que es justo lo que se quiere
// ver. Toda terminal de los ultimos 20 anios los tiene.
static const struct { const char *nombre; const char *ansi; } PALETA[] = {
    { "azul",             "\033[38;5;33m"  },
    { "azul-claro",       "\033[38;5;81m"  },
    { "aqua",             "\033[38;5;51m"  },
    { "verde-teal",       "\033[38;5;43m"  },
    { "verde-lima",       "\033[38;5;154m" },
    { "verde-esmeralda",  "\033[38;5;48m"  },
    { "amarillo",         "\033[38;5;220m" },
    { "dorado",           "\033[38;5;178m" },
    { "naranja",          "\033[38;5;208m" },
    { "naranja-fuerte",   "\033[38;5;202m" },
    { "rosa-lila",        "\033[38;5;176m" },
    { "violeta",          "\033[38;5;141m" },
    { "gris",             "\033[38;5;245m" },
    { "blanco-crema",     "\033[38;5;230m" },
};

const char *paed_ansi_reset(void) { return "\033[0m"; }

const char *paed_rol_ansi(const char *rol) {
    const char *color = paed_categoria_color(rol);
    if (!color) return "";

    for (size_t i = 0; i < sizeof(PALETA) / sizeof(PALETA[0]); i++)
        if (strcmp(PALETA[i].nombre, color) == 0) return PALETA[i].ansi;

    // Un color nuevo en sintaxis.json que todavia no esta en la paleta. No es
    // un error: se ve sin color y se sigue. Que agregar una categoria no pueda
    // romper el resaltado es medio el punto de que la definicion viva alla.
    return "";
}

// ── Que rol cumple un identificador ─────────────────────────────────────────
//
// El ORDEN de las preguntas es la decision de diseno de todo el archivo, asi
// que va explicado:
//
// PRIMERO lo que declaro el programador, DESPUES las palabras del lenguaje.
//
// Al reves seria lo comodo, pero rompe justo donde el resaltador viejo se
// rompia. En sintaxis.json 'N' es un tipo — el N(4) de la catedra — y 'V' es el
// booleano verdadero. Pero un programa puede declarar `N: ENTERO` o `v: LOGICO`,
// y ahi 'N' y 'v' son variables, punto: el AMBIENTE lo dice. Preguntando
// primero por las declaraciones, el caso se resuelve solo y sin ninguna regla
// especial. La gramatica de tree-sitter necesitaba dos parches para esto mismo,
// y aun asi solo acertaba por posicion.
//
// Nunca devuelve NULL: lo que no es nada conocido es una variable.
static const char *rol_de_identificador(const PAEDProgram *prog, const char *palabra) {
    if (prog) {
        // El nombre de la ACCION.
        if (prog->name[0] && strcasecmp(prog->name, palabra) == 0)
            return "acciones";

        // Un REGISTRO declarado. Es lo que pinta el 'fecha' de `fechas: fecha;`
        // y el 'factura' de `ARCHIVO DE factura`.
        for (int i = 0; i < prog->registro_count; i++)
            if (strcasecmp(prog->registros[i].name, palabra) == 0)
                return "tipos_usuario";

        // Una variable del AMBIENTE. Si se declaro ARCHIVO, va de otro color:
        // es el disparador del asistente de archivos.
        for (int i = 0; i < prog->decl_count; i++)
            if (strcasecmp(prog->decls[i].name, palabra) == 0)
                return prog->decls[i].es_archivo ? "archivos" : "variables";

        // Un campo de algun REGISTRO: el 'total' de `f.total`.
        for (int r = 0; r < prog->registro_count; r++)
            for (int c = 0; c < prog->registros[r].campo_count; c++)
                if (strcasecmp(prog->registros[r].campos[c].name, palabra) == 0)
                    return "variables";
    }

    // Recien ahora, el lenguaje.
    const char *cat = paed_categoria_de_palabra(palabra);
    if (cat) return cat;

    return "variables";
}

// ── El escaner ──────────────────────────────────────────────────────────────
//
// Va linea por linea, igual que el parser, por el mismo motivo: en PAED una
// instruccion no se parte en dos lineas, asi que ningun token cruza el salto.
// Eso hace que colorear una sola linea que cambio sea posible sin releer el
// archivo — que es lo que va a necesitar un editor cuando esto se conecte.

static void emitir(PAEDTokenFn fn, void *ud, int linea, const char *base,
                   const char *desde, int largo, const char *rol) {
    if (largo <= 0) return;

    // El token se copia a un buffer propio para poder terminarlo en '\0': el
    // que recibe quiere una cadena, no un puntero al medio de la linea con un
    // largo al lado.
    char texto[PAED_LINEA_MAX];
    if (largo >= (int)sizeof(texto)) largo = (int)sizeof(texto) - 1;
    memcpy(texto, desde, (size_t)largo);
    texto[largo] = '\0';

    PAEDToken tok = {
        .linea = linea,
        .col   = (int)(desde - base) + 1,   // las columnas arrancan en 1
        .largo = largo,
        .rol   = rol,
        .texto = texto,
    };
    fn(&tok, ud);
}

static int es_inicio_de_nombre(char c) { return isalpha((unsigned char)c) || c == '_'; }
static int es_resto_de_nombre(char c)  { return isalnum((unsigned char)c) || c == '_'; }

// ── Nombres con punto ───────────────────────────────────────────────────────
//
// `ACCION EjercicioArchivos2.1.2 ES` es un nombre solo, no tres cosas. El
// parser ya lo lee asi — su sscanf("%63s") se lleva la palabra entera hasta el
// espacio — pero el escaner de colores cortaba en el punto y salia partido:
// 'EjercicioArchivos2' como variable, el '.' como puntuacion y '1.2' como
// numero. Tres colores para un nombre.
//
// No alcanza con dejar que el punto forme parte de cualquier nombre, y por eso
// esto no esta en es_resto_de_nombre(): en `f.total` el punto es el acceso al
// campo de un REGISTRO, y ahi son DOS cosas de verdad. La diferencia no se
// puede sacar mirando el texto — hay que preguntar quien esta DECLARADO.
//
// Entonces la regla es: se estira sobre los puntos solo mientras lo que se
// forma sea un nombre que el programa declaro. `f.total` no es el nombre de
// ninguna accion, asi que no se estira y sigue siendo un acceso a campo.
//
// Un texto que es un NUMERO y nada mas: '2', '2.5'. Se usa para no robarle el
// color a los literales. Si alguien llama '2' a su ACCION, el nombre y el
// numero dos son el mismo texto y no hay forma de distinguirlos mirando: ahi
// gana el numero, que es lo que aparece mil veces en un programa.
static int es_numero_puro(const char *txt, int largo) {
    int digitos = 0, puntos = 0;
    for (int i = 0; i < largo; i++) {
        if (isdigit((unsigned char)txt[i])) { digitos++; continue; }
        if (txt[i] == '.' && puntos == 0 && i > 0 && i < largo - 1) { puntos++; continue; }
        return 0;
    }
    return digitos > 0;
}

// Devuelve el largo del nombre declarado mas LARGO que arranca en `p`, y deja
// su rol en *rol. Cero si no hay ninguno.
static int largo_nombre_declarado(const PAEDProgram *prog, const char *p,
                                  const char **rol) {
    if (!prog) return 0;

    int mejor = 0;

    // Se prueba tramo por tramo: "Ejercicio", "Ejercicio.1", "Ejercicio.1.2".
    // Gana el mas largo que matchee, no el primero: "Ejercicio.1" puede no ser
    // nada y "Ejercicio.1.2" si serlo.
    const char *fin = p + 1;
    while (es_resto_de_nombre(*fin)) fin++;

    for (;;) {
        char nombre[PAED_NAME_MAX];
        size_t n = (size_t)(fin - p);
        if (n >= sizeof(nombre)) break;
        memcpy(nombre, p, n);
        nombre[n] = '\0';

        if (prog->name[0] && strcasecmp(prog->name, nombre) == 0) {
            mejor = (int)n;
            *rol  = "acciones";
        } else {
            // Las subacciones van del MISMO rol que la ACCION, y no de uno
            // propio: el reparto de colores no separa "la accion" de "una
            // funcion tuya", separa lo que definis VOS de lo que trae el
            // lenguaje — es el function.paed de helix/tema.toml.ejemplo. Un
            // rol aparte pediria un color aparte para decir lo mismo.
            for (int i = 0; i < prog->subaccion_count; i++)
                if (strcasecmp(prog->subacciones[i].name, nombre) == 0) {
                    mejor = (int)n;
                    *rol  = "acciones";
                    break;
                }
        }

        // Al siguiente punto, si es que hay uno con algo detras.
        if (*fin != '.' || !es_resto_de_nombre(fin[1])) break;
        fin++;
        while (es_resto_de_nombre(*fin)) fin++;
    }

    if (mejor > 0 && es_numero_puro(p, mejor)) return 0;
    return mejor;
}

int paed_colorear_archivo(const char *path, const PAEDProgram *prog,
                          PAEDTokenFn fn, void *ud) {
    if (!path || !fn) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char linea[PAED_LINEA_MAX];
    int  nro = 0;

    while (fgets(linea, sizeof(linea), f)) {
        nro++;
        linea[strcspn(linea, "\r\n")] = '\0';

        const char *p = linea;
        while (*p) {
            // Espacios: no son un token, se saltan.
            if (isspace((unsigned char)*p)) { p++; continue; }

            // Comentario: se come hasta el fin de linea. Va PRIMERO porque
            // adentro de un comentario no hay codigo, aunque diga MIENTRAS.
            if (p[0] == '/' && p[1] == '/') {
                emitir(fn, ud, nro, linea, p, (int)strlen(p), "comentario");
                break;
            }

            // Texto entre comillas. La catedra escribe con comilla simple
            // (AED_2021_UnI.pdf:10); PAED acepta las dos. Mismo motivo que el
            // comentario: lo de adentro es texto, no codigo.
            if (*p == '"' || *p == '\'') {
                char comilla = *p;
                const char *fin = p + 1;
                while (*fin && *fin != comilla) fin++;
                if (*fin == comilla) fin++;   // sin cerrar: se colorea igual
                emitir(fn, ud, nro, linea, p, (int)(fin - p),
                       comilla == '"' ? "strings" : "strings_simples");
                p = fin;
                continue;
            }

            // Un nombre DECLARADO, antes que nada.
            //
            // Va antes de los numeros y antes de los nombres comunes porque un
            // nombre declarado puede empezar con un digito: `ACCION 2_1_2 ES`.
            // Si preguntaba primero por el numero, la rama de los numeros se
            // quedaba con el '2' y el resto salia aparte — el nombre partido en
            // dos colores. Y va DESPUES del comentario y de las comillas, que
            // esos se comen la linea entera pase lo que pase.
            //
            // Que esto no se lleve puesto a los literales lo cuida
            // es_numero_puro: ver largo_nombre_declarado.
            if (isalnum((unsigned char)*p) || *p == '_') {
                const char *rol_decl = NULL;
                int largo_decl = largo_nombre_declarado(prog, p, &rol_decl);
                if (largo_decl > 0) {
                    emitir(fn, ud, nro, linea, p, largo_decl, rol_decl);
                    p += largo_decl;
                    continue;
                }
            }

            // Numero. El punto se come solo si le sigue un digito: en
            // `ARREGLO[1..10]` el '..' es el rango, no la parte decimal del 1.
            if (isdigit((unsigned char)*p)) {
                const char *fin = p;
                while (isdigit((unsigned char)*fin)) fin++;
                if (*fin == '.' && isdigit((unsigned char)fin[1])) {
                    fin++;
                    while (isdigit((unsigned char)*fin)) fin++;
                }
                emitir(fn, ud, nro, linea, p, (int)(fin - p), "numeros");
                p = fin;
                continue;
            }

            // Nombre: palabra clave, tipo, archivo o variable. Se lee ENTERO
            // antes de preguntar que es. Sin esto, la 'Y' del operador matchea
            // adentro de 'MAYOR' y medio programa queda de color operador.
            if (es_inicio_de_nombre(*p)) {
                const char *fin = p + 1;
                while (es_resto_de_nombre(*fin)) fin++;

                char palabra[PAED_NAME_MAX];
                size_t n = (size_t)(fin - p);
                if (n >= sizeof(palabra)) n = sizeof(palabra) - 1;
                memcpy(palabra, p, n);
                palabra[n] = '\0';

                emitir(fn, ud, nro, linea, p, (int)(fin - p),
                       rol_de_identificador(prog, palabra));
                p = fin;
                continue;
            }

            // Simbolo del lenguaje: ':=', '<=', '+'... El largo lo devuelve
            // sintaxis.json, que es quien sabe cuales existen.
            int simbolo_largo = 0;
            const char *cat = paed_categoria_de_simbolo(p, &simbolo_largo);
            if (cat && simbolo_largo > 0) {
                emitir(fn, ud, nro, linea, p, simbolo_largo, cat);
                p += simbolo_largo;
                continue;
            }

            // Puntuacion: ';' ',' '(' ')' '[' ']' '.'. No esta en ninguna
            // categoria de sintaxis.json y no lleva color a proposito: pintar
            // los parentesis compite con lo que si importa mirar.
            emitir(fn, ud, nro, linea, p, 1, "puntuacion");
            p++;
        }
    }

    fclose(f);
    return 0;
}

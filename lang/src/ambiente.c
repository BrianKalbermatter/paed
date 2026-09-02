// El bloque AMBIENTE: que se declara y como.
//
//     AMBIENTE
//         alumnos = REGISTRO ... FIN_REGISTRO
//         arch: ARCHIVO DE alumnos ORDENADO POR nroSocio;
//         i, j: ENTERO;
//
// Todo lo que decide QUE existe en un programa y de que tipo. El PROCESO, que
// decide que PASA, se parsea en otro lado.
//
// Se lleva tres cosas que estaban separadas por un banner cada una pero son un
// solo tema: la organizacion de un archivo (ORDENADO POR / INDEXADO POR), el
// parseo de una declaracion suelta, y que una declaracion pueda nombrar varias
// variables a la vez.

#include "ambiente.h"
#include "sintaxis.h"
#include "reporte.h"
#include "texto.h"
#include "sentencias.h"

#include "cJSON.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// ── La organizacion de un archivo ─────────────────────────────────────────────
//
// Las organizaciones salen de sintaxis.json y NO de una lista en C, igual que
// todo lo demas: el asistente del editor tiene que ofrecer las mismas que el
// parser acepta, y con dos listas un dia dicen cosas distintas.
static cJSON *organizaciones(void) {
    cJSON *a = syn_seccion("archivos");
    if (!cJSON_IsObject(a)) return NULL;
    cJSON *o = cJSON_GetObjectItem(a, "organizaciones");
    return cJSON_IsArray(o) ? o : NULL;
}

static const char *org_str(cJSON *org, const char *campo) {
    cJSON *v = cJSON_GetObjectItem(org, campo);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}

// ¿`resto` arranca con esta clausula? Devuelve lo que viene DESPUES, o NULL.
//
// La clausula son dos palabras ("ORDENADO POR") y entre ellas puede haber
// cualquier cantidad de espacios, asi que no alcanza con comparar la cadena
// entera: se van consumiendo palabra por palabra.
//
// Cada palabra tiene que terminar donde termina: sin ese recaudo 'ORDENADOS'
// entraria por 'ORDENADO', que es el mismo agujero que tapa el isspace de
// tipo_tras_DE con el 'DE' y 'DEUDA'.
static char *tras_clausula(char *resto, const char *clausula) {
    const char *c = clausula;

    while (*c) {
        while (isspace((unsigned char)*c)) c++;
        size_t n = 0;
        while (c[n] && !isspace((unsigned char)c[n])) n++;
        if (n == 0) break;

        while (isspace((unsigned char)*resto)) resto++;
        if (strncasecmp(resto, c, n) != 0) return NULL;
        if (resto[n] && !isspace((unsigned char)resto[n])) return NULL;

        resto += n;
        c     += n;
    }
    return trim(resto);
}

// Parte la clave en campos. El corpus separa con coma y ademas mete una 'y'
// antes del ultimo: "clave, tipo_novedad y f_novedad".
//
// La 'y' se toma como separador SOLO cuando es palabra suelta. Sin ese
// recaudo, un campo llamado 'hoy' o 'ley' se partiria al medio.
static int partir_clave(PAEDProgram *p, int lineno, char *lista,
                        PAEDDecl *d, const char *nombre) {
    char *c = lista;

    while (*c) {
        while (isspace((unsigned char)*c) || *c == ',') c++;
        if (!*c) break;

        // La 'y' suelta es separador, no un campo.
        if ((*c == 'y' || *c == 'Y') &&
            (isspace((unsigned char)c[1]) || c[1] == ',')) {
            c++;
            continue;
        }

        size_t n = 0;
        while (c[n] && c[n] != ',' && !isspace((unsigned char)c[n])) n++;

        if (d->clave_count >= PAED_MAX_CLAVE) {
            add_error(p, lineno,
                      "la clave de '%s' tiene demasiados campos (maximo %d)",
                      nombre, PAED_MAX_CLAVE);
            return -1;
        }

        char campo[PAED_NAME_MAX];
        snprintf(campo, sizeof(campo), "%.*s", (int)n, c);
        if (!es_identificador(campo)) {
            add_error(p, lineno, "'%s' no es un nombre de campo valido", campo);
            return -1;
        }
        snprintf(d->clave[d->clave_count++], PAED_NAME_MAX, "%s", campo);
        c += n;
    }

    if (d->clave_count == 0) {
        add_error(p, lineno, "a '%s' le faltan los campos de la clave", nombre);
        return -1;
    }
    return 0;
}

// Lee la clausula de organizacion que sigue al tipo, si la hay.
//
// `resto` es lo que quedo despues de 'ARCHIVO DE': el tipo y, opcionalmente,
// la clausula. Recorta `resto` dejando solo el tipo.
//
// Sin este corte el tipo se comeria el renglon entero y `d->type` terminaria
// valiendo "remedio ORDENADO POR farmacia" — un archivo de un tipo que no
// existe, y el error saldria mucho despues hablando de otra cosa.
static int separar_organizacion(PAEDProgram *p, int lineno, char *resto,
                                PAEDDecl *d, const char *nombre) {
    // El tipo es UNA palabra: termina en el primer espacio.
    char *esp = resto;
    while (*esp && !isspace((unsigned char)*esp)) esp++;
    if (!*esp) return 0;              // sin clausula: secuencial

    *esp = '\0';
    char *cola = trim(esp + 1);
    if (!*cola) return 0;

    cJSON *orgs = organizaciones(), *o = NULL;
    cJSON_ArrayForEach(o, orgs) {
        const char *clausula = org_str(o, "clausula");
        if (!clausula) continue;      // la secuencial no tiene clausula

        char *lista = tras_clausula(cola, clausula);
        if (!lista) continue;

        const char *org = org_str(o, "nombre");
        snprintf(d->org, sizeof(d->org), "%s", org ? org : "");

        if (!*lista) {
            add_error(p, lineno, "a '%s' le faltan los campos despues de '%s'",
                      nombre, clausula);
            return -1;
        }
        if (partir_clave(p, lineno, lista, d, nombre) != 0) return -1;

        // 'INDEXADO POR' lleva UN campo: el indice es una sola clave de
        // acceso, no una clave compuesta como la del ordenamiento.
        const char *campos = org_str(o, "campos");
        if (campos && strcmp(campos, "uno") == 0 && d->clave_count != 1) {
            add_error(p, lineno,
                      "'%s' lleva un solo campo y '%s' tiene %d",
                      clausula, nombre, d->clave_count);
            return -1;
        }
        return 0;
    }

    add_error(p, lineno,
              "no se entiende '%s' en la declaracion de '%s': despues del tipo "
              "solo va ORDENADO POR o INDEXADO POR", cola, nombre);
    return -1;
}

// ── Parseo de una declaracion del AMBIENTE: nombre : TIPO; ────────────────────

// Lee el `DE <tipo>` que llevan tanto ARREGLO como ARCHIVO y devuelve el tipo
// de adentro, o NULL si no hay un 'DE' suelto.
//
// Lo usan las dos ramas: son la misma sintaxis, y tenerla escrita dos veces es
// garantizar que un dia se arregle una y la otra no.
// Devuelve NULL si no hay 'DE', y una cadena VACIA si hay 'DE' pero nada
// despues. Son dos errores distintos y cada uno tiene su mensaje: decir "falta
// DE" cuando el DE esta escrito manda a mirar la parte que ya esta bien.
static char *tipo_tras_DE(char *resto) {
    if (strncasecmp(resto, "DE", 2) != 0) return NULL;

    // 'DE' con nada atras: el tipo es lo que falta, no el DE.
    if (resto[2] == '\0') return resto + 2;

    // 'DE' tiene que ser palabra suelta: sin esto, un tipo llamado 'DEUDA'
    // pasaria por 'DE' seguido de 'UDA'.
    if (!isspace((unsigned char)resto[2])) return NULL;
    return trim(resto + 2);
}

// Declara UNA variable, con el nombre y el tipo ya separados.
//
// `tipo` se recibe como buffer propio y modificable a proposito: las ramas de
// abajo lo trocean en el lugar (trim, tipo_tras_DE, separar_organizacion).
static void declarar_una(PAEDProgram *p, char *nombre, char *tipo, int lineno) {
    if (!es_identificador(nombre)) {
        add_error(p, lineno, "nombre de variable invalido: '%s'", nombre);
        return;
    }
    if (p->decl_count >= PAED_MAX_DECLS) {
        add_error(p, lineno, "demasiadas declaraciones (maximo %d)", PAED_MAX_DECLS);
        return;
    }

    PAEDDecl *d = &p->decls[p->decl_count++];
    memset(d, 0, sizeof(*d));
    strncpy(d->name, nombre, PAED_NAME_MAX - 1);
    d->line = lineno;

    // ARREGLO[desde..hasta] DE TIPO
    // Los limites se exigen constantes: en AED el tamaño de un arreglo se
    // conoce al declararlo, no depende de una variable que todavia no existe.
    if (strncasecmp(tipo, "ARREGLO", 7) == 0) {
        int   desde = 0, hasta = 0, leidos = 0;
        char  base[PAED_NAME_MAX] = {0};

        // Ya se sabe que empieza con ARREGLO: se saltan esas 7 letras y se lee
        // el rango. Los espacios del formato admiten "[1..10]" y "[ 1 .. 10 ]".
        if (sscanf(tipo + 7, " [ %d .. %d ]%n", &desde, &hasta, &leidos) != 2 || leidos == 0) {
            add_error(p, lineno,
                      "arreglo mal declarado en '%s': se esperaba "
                      "ARREGLO[desde..hasta] DE TIPO", nombre);
            p->decl_count--;
            return;
        }
        if (hasta < desde) {
            add_error(p, lineno, "el arreglo '%s' tiene los limites al reves: [%d..%d]",
                      nombre, desde, hasta);
            p->decl_count--;
            return;
        }

        char *base_p = tipo_tras_DE(trim(tipo + 7 + leidos));
        if (!base_p) {
            add_error(p, lineno, "al arreglo '%s' le falta 'DE <tipo>'", nombre);
            p->decl_count--;
            return;
        }
        snprintf(base, sizeof(base), "%s", base_p);
        if (!*base) {
            add_error(p, lineno, "al arreglo '%s' le falta el tipo despues de 'DE'", nombre);
            p->decl_count--;
            return;
        }

        d->es_arreglo = 1;
        d->desde      = desde;
        d->hasta      = hasta;
        strncpy(d->type, base, PAED_NAME_MAX - 1);
        return;
    }

    // ARCHIVO DE TIPO
    //
    // El isspace del final NO es decorativo: sin el, un tipo llamado
    // 'ARCHIVOS' entraria por esta rama. La rama de ARREGLO de arriba tiene
    // ese mismo agujero y quedo anotado en el KANBAN.
    if (strncasecmp(tipo, "ARCHIVO", 7) == 0 &&
        (tipo[7] == '\0' || isspace((unsigned char)tipo[7]))) {

        char *base_p = tipo_tras_DE(trim(tipo + 7));
        if (!base_p) {
            add_error(p, lineno, "al archivo '%s' le falta 'DE <tipo>'", nombre);
            p->decl_count--;
            return;
        }
        if (!*base_p) {
            add_error(p, lineno, "al archivo '%s' le falta el tipo despues de 'DE'", nombre);
            p->decl_count--;
            return;
        }

        // Corta `base_p` dejando solo el tipo y se queda con la organizacion.
        // Va ANTES de copiar el tipo, no despues: si no, el tipo ya se llevo
        // la clausula adentro.
        if (separar_organizacion(p, lineno, base_p, d, nombre) != 0) {
            p->decl_count--;
            return;
        }

        d->es_archivo = 1;
        strncpy(d->type, base_p, PAED_NAME_MAX - 1);
        return;
    }

    // SECUENCIA DE TIPO   /   SECUENCIA DE SALIDA
    //
    // El isspace del final es el mismo recaudo que en ARCHIVO: sin el, un tipo
    // llamado 'SECUENCIAL' entraria por esta rama.
    if (strncasecmp(tipo, "SECUENCIA", 9) == 0 &&
        (tipo[9] == '\0' || isspace((unsigned char)tipo[9]))) {

        char *base_p = tipo_tras_DE(trim(tipo + 9));
        if (!base_p) {
            add_error(p, lineno, "a la secuencia '%s' le falta 'DE <tipo>'", nombre);
            p->decl_count--;
            return;
        }
        if (!*base_p) {
            add_error(p, lineno, "a la secuencia '%s' le falta el tipo despues de 'DE'", nombre);
            p->decl_count--;
            return;
        }

        d->es_secuencia = 1;
        // SALIDA no es un tipo de dato: es la direccion en la que va la
        // secuencia. Por eso se guarda como bandera y no como `type`.
        d->es_salida = (strcasecmp(base_p, "SALIDA") == 0);
        strncpy(d->type, base_p, PAED_NAME_MAX - 1);
        return;
    }

    // VENTANA DE TIPO
    //
    // La ventana es la variable donde AVZ deja el elemento actual. En el
    // corpus se declara aparte (`vent: VENTANA DE CARACTER;`) pero se usa
    // como una variable comun — `avz(sec, vent)` la llena y `vent <> '#'` la
    // compara. Asi que se guarda como escalar del tipo de adentro, y no hace
    // falta ningun tratamiento especial en el interprete.
    if (strncasecmp(tipo, "VENTANA", 7) == 0 &&
        (tipo[7] == '\0' || isspace((unsigned char)tipo[7]))) {

        char *base_p = tipo_tras_DE(trim(tipo + 7));
        if (!base_p || !*base_p) {
            add_error(p, lineno, "a la ventana '%s' le falta 'DE <tipo>'", nombre);
            p->decl_count--;
            return;
        }
        strncpy(d->type, base_p, PAED_NAME_MAX - 1);
        return;
    }

    strncpy(d->type, tipo, PAED_NAME_MAX - 1);
}

// ── Una declaracion puede nombrar VARIAS variables ────────────────────────────
//
// `a, doble: entero;` declara las dos. Sale de la catedra, que lo escribe asi
// en el ejemplo canonico (TEORIA_COMPLETA.txt:440).
//
// wiki_paed.txt:149-150 lo tenia como pregunta abierta, contestada con un
// "Frankly dice NO". Frankly no decide: docs/PAED.md:1319 deja escrito que es
// permisivo y que correr ahi no hace correcto a un archivo — o sea que tampoco
// hace incorrecto a lo que rechaza. Manda la catedra, y la catedra lo usa.
//
// El corte es por coma y NO colapsa los vacios: `a,,b` y `a,b,` caen en
// es_identificador("") y dan error con el nombre vacio a la vista. Tragarse una
// coma de mas seria justo lo que este parser promete no hacer.
static void parse_decl(PAEDProgram *p, char *linea, int lineno) {
    // El ';' final es opcional, por el mismo motivo que en las instrucciones:
    // la catedra lo escribe de las dos formas. El AMBIENTE de SECUENCIA.txt no
    // lo lleva y el de REGISTRO.txt si.
    size_t len = strlen(linea);
    if (len > 0 && linea[len - 1] == ';') linea[len - 1] = '\0';
    if (!*trim(linea)) return;

    // 'HV = 99999999;' es como los templates de mezcla y de actualizacion
    // declaran el centinela. Se ACEPTA y se IGNORA, y eso ultimo es lo
    // importante: en PAED, HV es un valor de alto propio del lenguaje y tiene
    // que seguir siendolo. Las claves de los parciales son TEXTO, y
    // strcmp("99999999", "F1-Ibuprofeno") daria que HV es MENOR — justo al
    // reves de lo que HV significa (ver PAED.md 2.8).
    //
    // Asi el programa del parcial se escribe TAL CUAL y ademas compara bien.
    {
        char copia[PAED_LINEA_MAX];
        snprintf(copia, sizeof(copia), "%s", linea);
        char *ig = strchr(copia, '=');
        if (ig) {
            *ig = '\0';
            if (kw_es(trim(copia), "HV")) return;
        }
    }

    char *dosp = strchr(linea, ':');
    if (!dosp) {
        add_error(p, lineno, "declaracion invalida: se esperaba nombre: TIPO;");
        return;
    }
    *dosp = '\0';
    char *nombres = trim(linea);
    char *tipo    = trim(dosp + 1);

    if (!*tipo) {
        add_error(p, lineno, "falta el tipo de '%s'", nombres);
        return;
    }

    // El tipo se COPIA para cada nombre: declarar_una lo trocea en el lugar, y
    // sin copia la segunda variable se encontraria el tipo ya comido.
    for (char *n = nombres;;) {
        char *coma = strchr(n, ',');
        if (coma) *coma = '\0';

        char copia[PAED_LINEA_MAX];
        snprintf(copia, sizeof(copia), "%s", tipo);
        declarar_una(p, trim(n), copia, lineno);

        if (!coma) break;
        n = coma + 1;
    }
}

// ── El bloque AMBIENTE, que puede tener REGISTROS adentro ─────────────────────
//
//     AMBIENTE
//         vector2 = REGISTRO      <- abre un tipo
//             vx: REAL;           <- campo
//             vy: REAL;
//         FIN_REGISTRO            <- lo cierra
//         pori: vector2;          <- variable de ese tipo
//
// `reg` apunta al registro que se esta llenando, o es NULL si no hay ninguno
// abierto. El que llama lo mantiene entre lineas: es el sub-estado del bloque.
//
// Esta funcion ve UNA declaracion. Varias en el mismo renglon las parte
// por_cada_sentencia, que es el que llama — misma regla que en el PROCESO.
// ¿Esta linea abre un REGISTRO? `<nombre> = REGISTRO`, SIN tocar la linea.
//
// Hay una version que si la toca (parte el '=' para quedarse con el nombre) mas
// abajo, en la rama que lo abre de verdad. Esta existe para poder PREGUNTAR
// antes de decidir: mientras hay un registro abierto, la respuesta cambia el
// error que corresponde.
static int abre_registro(const char *linea) {
    const char *ig = strchr(linea, '=');
    if (!ig) return 0;

    const char *r = ig + 1;
    while (*r && isspace((unsigned char)*r)) r++;

    size_t n = strlen(r);
    while (n > 0 && isspace((unsigned char)r[n - 1])) n--;

    return n == 8 && strncasecmp(r, "REGISTRO", 8) == 0;
}

// ¿Esta linea declara un CONJUNTO? `<nombre> = { ... }`, SIN tocar la linea.
//
// Se mira que despues del '=' venga una llave. Con eso alcanza para separarlo
// de `x = REGISTRO` y de una declaracion normal, que lleva ':' y no '='.
static int abre_conjunto(const char *linea) {
    const char *ig = strchr(linea, '=');
    if (!ig) return 0;

    const char *r = ig + 1;
    while (*r && isspace((unsigned char)*r)) r++;
    return *r == '{';
}

// Carga los elementos de `{a, b, c}` en el conjunto.
//
// Se guardan como TEXTO y sin las comillas: el que compara es el evaluador, con
// las reglas del '=' del lenguaje. Aca no se decide de que tipo es nada.
//
// El corte es a mano y no con strtok por dos motivos, y el segundo es el que
// importa: strtok COLAPSA los separadores seguidos, asi que `{1,,2}` le pasaba
// como si fueran dos elementos y `{1, , 2}` daba error — el mismo tipeo con dos
// resultados distintos, que es peor que cualquiera de los dos. Ademas strtok
// guarda estado global, y un parser es el ultimo lugar donde uno quiere eso.
static void parse_elementos(PAEDProgram *p, PAEDConjunto *cj, char *dentro,
                            int lineno) {
    // `{}` no es un elemento vacio: es un conjunto vacio, y eso lo reporta el
    // que llama con su propio mensaje. Sin este corte, la coma que no esta se
    // leeria como un elemento en blanco y el error hablaria de otra cosa.
    if (*trim(dentro) == '\0') return;

    char *item = dentro;

    for (;;) {
        char *coma = strchr(item, ',');
        if (coma) *coma = '\0';

        char *e = trim(item);

        // Las comillas son del literal, no del dato: "A" y 'A' y A son el
        // mismo elemento. Sacarlas aca deja una sola forma para comparar.
        size_t n = strlen(e);
        if (n >= 2 && (e[0] == '"' || e[0] == '\'') && e[n - 1] == e[0]) {
            e[n - 1] = '\0';
            e++;
        }

        if (*e == '\0') {
            add_error(p, lineno, "el conjunto '%s' tiene un elemento vacio",
                      cj->name);
            return;
        }
        if (cj->elem_count >= PAED_MAX_ELEMENTOS) {
            add_error(p, lineno, "el conjunto '%s' no entra: maximo %d elementos",
                      cj->name, PAED_MAX_ELEMENTOS);
            return;
        }

        strncpy(cj->elems[cj->elem_count], e, PAED_NAME_MAX - 1);
        cj->elems[cj->elem_count][PAED_NAME_MAX - 1] = '\0';
        cj->elem_count++;

        if (!coma) return;
        item = coma + 1;
    }
}

static void parse_ambiente_una(PAEDProgram *p, char *linea, int lineno, PAEDRegistro **reg) {
    // ── Dentro de un REGISTRO ──
    if (*reg) {
        if (kw_es(linea, "FIN_REGISTRO") || kw_es(linea, "FINREGISTRO")) {
            if ((*reg)->campo_count == 0)
                add_error(p, lineno, "el registro '%s' no tiene ningun campo", (*reg)->name);
            *reg = NULL;
            return;
        }

        // Otro REGISTRO que arranca con uno todavia abierto. Lo que falta es el
        // FIN_REGISTRO del anterior, y hay que decir ESO.
        //
        // Sin este caso, `alumno = REGISTRO` entraba como un CAMPO del registro
        // de arriba, y el error era "falta ';' al final de la declaracion" en
        // ESTA linea — mandando a mirar la unica linea que estaba bien, mientras
        // el problema real quedaba varios renglones mas arriba.
        if (abre_registro(linea)) {
            add_error(p, lineno,
                      "falta FIN_REGISTRO: el registro '%s' de la linea %d sigue "
                      "abierto y aca ya empieza otro", (*reg)->name, (*reg)->line);
            // Se lo cierra igual y la linea sigue viaje a la rama que abre el
            // nuevo, mas abajo. Dejarlo abierto haria que todos los campos del
            // segundo registro cayeran en el primero, y cada uno sumaria su
            // propio error por algo que ya se reporto una vez.
            *reg = NULL;
        }
    }

    // ── Un campo del REGISTRO que sigue abierto ──
    if (*reg) {
        if ((*reg)->campo_count >= PAED_MAX_CAMPOS) {
            add_error(p, lineno, "el registro '%s' tiene demasiados campos (maximo %d)",
                      (*reg)->name, PAED_MAX_CAMPOS);
            return;
        }

        // Un campo se declara igual que una variable, asi que se reusa el mismo
        // parseo en vez de tener dos copias que se desincronicen.
        PAEDDecl *destino = &(*reg)->campos[(*reg)->campo_count];
        int antes = p->decl_count;
        parse_decl(p, linea, lineno);

        if (p->decl_count > antes) {
            // parse_decl lo dejo en la tabla general: se mueve al registro,
            // porque un campo no es una variable del programa.
            *destino = p->decls[--p->decl_count];

            // Un registro vive en MEMORIA: es el molde de lo que se lee o se
            // graba. Un archivo adentro no tiene sentido, y aceptarlo callado
            // dejaria pasar una declaracion que despues explota sin motivo
            // visible.
            if (destino->es_archivo) {
                add_error(p, lineno,
                          "un campo de registro no puede ser un archivo: '%s' "
                          "(un registro vive en memoria)", destino->name);
                return;
            }
            (*reg)->campo_count++;
        }
        return;
    }

    // ── ¿Declara un CONJUNTO? `<nombre> = { ... }` ──
    // Va antes del REGISTRO porque los dos empiezan igual, con '=', y este se
    // reconoce por la llave.
    if (abre_conjunto(linea)) {
        char *llave = strchr(linea, '{');
        char *cierra = strrchr(linea, '}');
        if (!cierra) {
            add_error(p, lineno,
                      "falta '}' para cerrar el conjunto: se escribe "
                      "nombre = {a, b, c};");
            return;
        }

        *strchr(linea, '=') = '\0';
        char *nombre = trim(linea);
        *cierra = '\0';

        if (!es_identificador(nombre)) {
            add_error(p, lineno, "nombre de conjunto invalido: '%s'", nombre);
            return;
        }
        if (p->conjunto_count >= PAED_MAX_CONJUNTOS) {
            add_error(p, lineno, "demasiados conjuntos (maximo %d)",
                      PAED_MAX_CONJUNTOS);
            return;
        }
        for (int i = 0; i < p->conjunto_count; i++)
            if (kw_es(p->conjuntos[i].name, nombre)) {
                add_error(p, lineno,
                          "el conjunto '%s' ya estaba declarado en la linea %d",
                          nombre, p->conjuntos[i].line);
                return;
            }

        PAEDConjunto *cj = &p->conjuntos[p->conjunto_count++];
        memset(cj, 0, sizeof(*cj));
        strncpy(cj->name, nombre, PAED_NAME_MAX - 1);
        cj->line = lineno;
        parse_elementos(p, cj, llave + 1, lineno);

        if (cj->elem_count == 0)
            add_error(p, lineno,
                      "conjunto sin elementos: '%s' no tiene ninguno", nombre);
        return;
    }

    // ── ¿Abre un REGISTRO? `<nombre> = REGISTRO` ──
    // Se mira el '=' antes que nada: una declaracion normal lleva ':' y esta no.
    if (abre_registro(linea)) {
        *strchr(linea, '=') = '\0';   // abre_registro ya garantizo que esta
        char *nombre = trim(linea);

        if (!es_identificador(nombre)) {
            add_error(p, lineno, "nombre de registro invalido: '%s'", nombre);
            return;
        }
        if (p->registro_count >= PAED_MAX_REGISTROS) {
            add_error(p, lineno, "demasiados registros (maximo %d)", PAED_MAX_REGISTROS);
            return;
        }
        for (int i = 0; i < p->registro_count; i++)
            if (kw_es(p->registros[i].name, nombre)) {
                add_error(p, lineno, "el registro '%s' ya se declaro en la linea %d",
                          nombre, p->registros[i].line);
                return;
            }

        PAEDRegistro *nuevo = &p->registros[p->registro_count++];
        memset(nuevo, 0, sizeof(*nuevo));
        strncpy(nuevo->name, nombre, PAED_NAME_MAX - 1);
        nuevo->line = lineno;
        *reg = nuevo;
        return;
    }

    // ── Declaracion normal de variable ──
    parse_decl(p, linea, lineno);
}

static void una_declaracion(PAEDProgram *p, char *texto, int lineno, void *ctx) {
    parse_ambiente_una(p, texto, lineno, (PAEDRegistro **)ctx);
}

// El AMBIENTE, renglon completo. Puede traer mas de una declaracion:
//
//     a: ENTERO; b: ENTERO; c: ENTERO;
//
// El REGISTRO que este abierto viaja como contexto, porque el corte no cambia
// quien es el destino de cada campo: sigue siendo el mismo registro para todas
// las declaraciones del renglon.
void parse_ambiente(PAEDProgram *p, char *linea, int lineno, PAEDRegistro **reg) {
    por_cada_sentencia(p, linea, lineno, una_declaracion, reg);
}

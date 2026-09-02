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
#include "chequeos.h"
#include "subacciones.h"
#include "cJSON.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "paed/plataforma.h"   // saber donde esta el binario, sin #ifdef aca


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

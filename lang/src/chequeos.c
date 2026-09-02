// Los chequeos que necesitan el programa ENTERO leido.
//
// Van aparte del parseo linea por linea porque no se pueden hacer mientras se
// lee: para saber si un LEER es valido hay que saber que dejo abierto el ABRIR
// de mas arriba, y para saber si una subaccion existe hay que haber leido todo
// el archivo, porque una puede llamar a otra declarada mas abajo.

#include "chequeos.h"
#include "sintaxis.h"
#include "subacciones.h"
#include "reporte.h"
#include "texto.h"

#include "cJSON.h"

#include <stdio.h>
#include <string.h>

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

void chequear_modos(PAEDProgram *p) {
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
void chequear_claves(PAEDProgram *p) {
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


// Verifica TODAS las llamadas a subacciones de una sola vez, con el programa
// entero leido. Va aparte del parseo linea por linea porque recien aca se sabe
// que subacciones existen: una puede llamar a otra declarada mas abajo.
void chequear_subacciones(PAEDProgram *p) {
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


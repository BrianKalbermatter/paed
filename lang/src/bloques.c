// La pila de bloques: quien abre, quien cierra y a donde salta cada uno.
//
// SI/MIENTRAS/PARA/SEGUN abren un bloque y su FIN_ lo cierra. Mientras se lee
// el archivo de arriba a abajo, los bloques abiertos se apilan: el ultimo que
// se abrio es el primero que se tiene que cerrar. Cuando uno cierra, recien ahi
// se sabe a donde salta la instruccion que lo abrio, y se completa hacia atras.
//
// Esa es toda la razon de la pila. Sin ella habria que leer el archivo dos
// veces, o adivinar el salto antes de saber donde termina el bloque.

#include "bloques.h"
#include "programa.h"
#include "reporte.h"
#include "sentencias.h"
#include "texto.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// ── La pila de bloques ────────────────────────────────────────────────────────
//
// Una variable sola no sirve para el anidamiento. En
//
//     MIENTRAS (a) HACER          MIENTRAS (a) HACER
//         MIENTRAS (b) HACER          x := 1;
//         FIN_MIENTRAS            FIN_MIENTRAS
//     FIN_MIENTRAS                MIENTRAS (b) HACER
//                                 FIN_MIENTRAS
//
// los dos programas tienen 2 MIENTRAS y 2 FIN_MIENTRAS: contar no los
// distingue. Hace falta saber QUIEN abrio cada uno.
//
// Y es una PILA y no otra cosa porque los bloques cierran en el orden inverso
// al que se abrieron: el ultimo que se abrio es el primero que se cierra. Eso
// es LIFO, y eso es exactamente una pila. Es lo mismo que hace un compilador
// de C con las llaves.

const char *nombre_kind(PAEDKind k) {
    switch (k) {
        case PAED_SI:       return "SI";
        case PAED_MIENTRAS: return "MIENTRAS";
        case PAED_PARA:     return "PARA";
        case PAED_REPETIR:  return "REPETIR";
        case PAED_SEGUN:    return "SEGUN";
        default:            return "?";
    }
}

// Abre un bloque: crea la instruccion, guarda la condicion y apila.
static void abrir(PAEDProgram *p, Pila *pila, PAEDKind kind,
                  const char *cond, int lineno) {
    PAEDInstr *in = nueva_instr(p, kind, lineno);
    if (!in) return;
    strncpy(in->cond, cond, PAED_COND_MAX - 1);

    if (pila->tope >= PAED_MAX_BLOQUES) {
        add_error(p, lineno, "demasiados bloques anidados (maximo %d)", PAED_MAX_BLOQUES);
        return;
    }
    pila->items[pila->tope++] = (Abierto){ kind, lineno, p->instr_count - 1, -1, -1 };
}

// Devuelve 1 si la linea era de bloque (y ya la trato), 0 si no lo era.
int parse_bloque(PAEDProgram *p, char *linea, int lineno, Pila *pila) {

    // ── SI (condicion) ENTONCES ──
    if (empieza_con(linea, "SI")) {
        char *cond = cuerpo_cabecera(linea, "SI", "ENTONCES");
        if (!cond)  { add_error(p, lineno, "se esperaba: SI <condicion> ENTONCES"); return 1; }
        if (!*cond) { add_error(p, lineno, "el SI no tiene condicion"); return 1; }
        abrir(p, pila, PAED_SI, cond, lineno);
        return 1;
    }

    // ── MIENTRAS (condicion) HACER ──
    if (empieza_con(linea, "MIENTRAS")) {
        char *cond = cuerpo_cabecera(linea, "MIENTRAS", "HACER");
        if (!cond)  { add_error(p, lineno, "se esperaba: MIENTRAS <condicion> HACER"); return 1; }
        if (!*cond) { add_error(p, lineno, "el MIENTRAS no tiene condicion"); return 1; }
        abrir(p, pila, PAED_MIENTRAS, cond, lineno);
        return 1;
    }

    // ── PARA <var> := <desde> HASTA <hasta>[; <paso>] HACER ──
    //
    // La forma sale de TEORIA_COMPLETA.txt:565-571, que dice textual:
    //     "Si el incremento es distinto de 1, debe indicarse."
    //     PARA contador := inicialización hasta fin; incremento HACER
    // O sea que el paso es OPCIONAL y por defecto vale 1. Aca se separa del
    // for de Pascal, que no tiene clausula de incremento.
    //
    // Todo se guarda en args (desde/hasta/paso) para que el interprete lo lea
    // con paed_get_arg, igual que cualquier otro argumento.
    if (empieza_con(linea, "PARA")) {
        char *cuerpo = cuerpo_cabecera(linea, "PARA", "HACER");
        if (!cuerpo) {
            add_error(p, lineno,
                      "se esperaba: PARA <var> := <desde> HASTA <hasta> HACER"
                      " (con '; <paso>' antes de HACER si el incremento no es 1)");
            return 1;
        }

        char *asig = strstr(cuerpo, ":=");
        if (!asig) {
            add_error(p, lineno, "al PARA le falta ':=' con el valor inicial");
            return 1;
        }
        *asig = '\0';
        char *var   = trim(cuerpo);
        char *resto = trim(asig + 2);

        char *h = palabra_en(resto, "HASTA");
        if (!h) {
            add_error(p, lineno, "al PARA le falta HASTA con el valor final");
            return 1;
        }
        *h = '\0';
        char *desde = trim(resto);
        char *hasta = trim(h + 5);

        // El paso es OPCIONAL: sin el, vale 1. Lo separa una coma o un ';'.
        //
        // La catedra usa la COMA — 'Para c := 1 hasta 10, 1 hacer' aparece en
        // Para.txt, SUBSECUENCIA.txt y ARREGLOS_Conceptos.txt, y ni una sola
        // vez con ';'. PAED habia elegido ';'; desde el 2026-08-17 acepta las
        // dos, con la coma como forma de catedra.
        //
        // Se busca el separador de NIVEL SUPERIOR, no el primero que aparezca:
        // en 'hasta f(a, b), 1' la coma de adentro del parentesis es del
        // argumento y no del paso. Sin esta cuenta, ese PARA se partiria mal y
        // el error saldria en un lugar que no tiene nada que ver.
        const char *paso = "1";
        char *sep = NULL;
        int   prof = 0;
        for (char *c = hasta; *c; c++) {
            if      (*c == '(' || *c == '[') prof++;
            else if (*c == ')' || *c == ']') prof--;
            else if (prof == 0 && (*c == ',' || *c == ';')) { sep = c; break; }
        }
        if (sep) {
            char corte = *sep;
            *sep = '\0';
            char *pval = trim(sep + 1);
            if (!*pval) {
                add_error(p, lineno, "el PARA tiene '%c' pero no dice el incremento", corte);
                return 1;
            }
            paso  = pval;
            hasta = trim(hasta);
        }

        if (!es_identificador(var)) {
            add_error(p, lineno, "variable de PARA invalida: '%s'", var);
            return 1;
        }
        if (!*desde) { add_error(p, lineno, "al PARA le falta el valor inicial"); return 1; }
        if (!*hasta) { add_error(p, lineno, "al PARA le falta el valor final");   return 1; }

        PAEDInstr *in = nueva_instr(p, PAED_PARA, lineno);
        if (!in) return 1;
        strncpy(in->proc, var, PAED_NAME_MAX - 1);
        strncpy(in->args[0].key, "desde", PAED_KEY_MAX - 1);
        strncpy(in->args[0].val, desde,   PAED_VAL_MAX - 1);
        strncpy(in->args[1].key, "hasta", PAED_KEY_MAX - 1);
        strncpy(in->args[1].val, hasta,   PAED_VAL_MAX - 1);
        strncpy(in->args[2].key, "paso",  PAED_KEY_MAX - 1);
        strncpy(in->args[2].val, paso,    PAED_VAL_MAX - 1);
        in->arg_count = 3;

        if (pila->tope >= PAED_MAX_BLOQUES) {
            add_error(p, lineno, "demasiados bloques anidados (maximo %d)", PAED_MAX_BLOQUES);
            return 1;
        }
        pila->items[pila->tope++] = (Abierto){ PAED_PARA, lineno, p->instr_count - 1, -1, -1 };
        return 1;
    }

    // ── SEGUN <expresion> HACER ──
    //
    // La seleccion multiple de la catedra. Confirmada por dos templates
    // oficiales: Segun.txt y ACT INDEX [TEMPLATE].txt.
    if (empieza_con(linea, "SEGUN")) {
        char *cuerpo = cuerpo_cabecera(linea, "SEGUN", "HACER");
        if (!cuerpo) {
            add_error(p, lineno, "se esperaba: SEGUN <expresion> HACER");
            return 1;
        }
        if (!*cuerpo) {
            add_error(p, lineno, "al SEGUN le falta la expresion a comparar");
            return 1;
        }
        abrir(p, pila, PAED_SEGUN, cuerpo, lineno);
        return 1;
    }

    // ── FIN_SEGUN ──
    if (kw_es(linea, "FIN_SEGUN")) {
        if (pila->tope == 0 || pila->items[pila->tope - 1].kind != PAED_SEGUN) {
            add_error(p, lineno, "FIN_SEGUN sin un SEGUN abierto");
            return 1;
        }
        Abierto *a = &pila->items[pila->tope - 1];

        PAEDInstr *in = nueva_instr(p, PAED_FIN_SEGUN, lineno);
        if (!in) return 1;
        int fin = p->instr_count - 1;

        // Sin ninguna rama, el SEGUN salta derecho al final.
        if (a->ultimo_caso < 0) p->instrs[a->instr].salto = fin;

        // Cada rama sale por el final, y la ultima ademas cierra la cadena.
        for (int k = a->instr + 1; k <= fin; k++)
            if (p->instrs[k].kind == PAED_CASO) p->instrs[k].salto = fin;
        if (a->ultimo_caso >= 0) p->instrs[a->ultimo_caso].siguiente = -1;

        // La rama por defecto (SINO / CONTRARIO) se guardo en `sino`.
        if (a->sino >= 0) p->instrs[a->sino].salto = fin;

        pila->tope--;
        return 1;
    }

    // ── Una rama del SEGUN: <etiqueta>[, <etiqueta>]: <sentencias> ──
    //
    // Se reconoce SOLO adentro de un SEGUN. Afuera, una linea con ':' es una
    // declaracion o un error, y robarsela aca daria un mensaje incomprensible.
    if (pila->tope > 0 && pila->items[pila->tope - 1].kind == PAED_SEGUN) {
        Abierto *a = &pila->items[pila->tope - 1];

        // La rama por defecto. 'CONTRARIO' ya llego aca convertido en 'SINO'
        // por normalizar_catedra; 'CONTRARIO:' con dos puntos es la forma del
        // template de ACT INDEX, asi que se acepta con y sin ellos.
        if (kw_es(linea, "SINO") || kw_es(linea, "SINO:") ||
            kw_es(linea, "CONTRARIO") || kw_es(linea, "CONTRARIO:")) {
            if (a->sino >= 0) {
                add_error(p, lineno, "el SEGUN de la linea %d ya tiene una rama por defecto",
                          a->line);
                return 1;
            }
            PAEDInstr *in = nueva_instr(p, PAED_CASO, lineno);
            if (!in) return 1;
            int idx = p->instr_count - 1;
            in->cond[0] = '\0';                 // sin etiquetas = rama por defecto
            in->siguiente = -1;
            if (a->ultimo_caso >= 0) p->instrs[a->ultimo_caso].siguiente = idx;
            else                     p->instrs[a->instr].salto = idx;
            a->ultimo_caso = idx;
            a->sino = idx;
            return 1;
        }

        // Una rama con etiquetas. Los dos puntos que la abren son los primeros
        // de NIVEL SUPERIOR: en "'a': ESCRIBIR('x: y')" el segundo esta adentro
        // de un texto y no separa nada.
        int prof = 0;
        char comilla_et = 0;
        char *dosp = NULL;
        for (char *c = linea; *c; c++) {
            if (!comilla_et && (*c == '\'' || *c == '"')) { comilla_et = *c; continue; }
            if (comilla_et) { if (*c == comilla_et) comilla_et = 0; continue; }
            if      (*c == '(' || *c == '[') prof++;
            else if (*c == ')' || *c == ']') prof--;
            else if (*c == ':' && prof == 0) {
                if (c[1] == '=') break;          // ':=' es una asignacion, no una rama
                dosp = c;
                break;
            }
        }

        if (dosp) {
            *dosp = '\0';
            char *etiquetas = trim(linea);
            char *resto     = trim(dosp + 1);

            if (!*etiquetas) {
                add_error(p, lineno, "a la rama del SEGUN le faltan las etiquetas antes de ':'");
                return 1;
            }

            // 'CONTRARIO: <sentencia>' — la rama por defecto con los dos puntos
            // pegados, como en ACT INDEX [TEMPLATE].txt. Sin este caso,
            // 'CONTRARIO' se leeria como el nombre de una variable y el error
            // saldria recien al ejecutar.
            if (kw_es(etiquetas, "SINO") || kw_es(etiquetas, "CONTRARIO")) {
                if (a->sino >= 0) {
                    add_error(p, lineno, "el SEGUN de la linea %d ya tiene una rama por defecto",
                              a->line);
                    return 1;
                }
                PAEDInstr *d = nueva_instr(p, PAED_CASO, lineno);
                if (!d) return 1;
                int didx = p->instr_count - 1;
                d->cond[0]  = '\0';
                d->siguiente = -1;
                if (a->ultimo_caso >= 0) p->instrs[a->ultimo_caso].siguiente = didx;
                else                     p->instrs[a->instr].salto = didx;
                a->ultimo_caso = didx;
                a->sino = didx;
                if (*resto) parse_sentencias(p, resto, lineno);
                return 1;
            }

            PAEDInstr *in = nueva_instr(p, PAED_CASO, lineno);
            if (!in) return 1;
            int idx = p->instr_count - 1;
            strncpy(in->cond, etiquetas, PAED_COND_MAX - 1);
            in->siguiente = -1;
            if (a->ultimo_caso >= 0) p->instrs[a->ultimo_caso].siguiente = idx;
            else                     p->instrs[a->instr].salto = idx;
            a->ultimo_caso = idx;

            // El cuerpo puede venir en la misma linea (que es como lo escriben
            // los dos templates) o en las de abajo. Si viene, se parsea igual
            // que cualquier otra sentencia.
            if (*resto) parse_sentencias(p, resto, lineno);
            return 1;
        }
    }

    // ── REPETIR ──
    //
    // El ciclo POST-TEST de la catedra (template Repetir.txt, ver PAED.md 0 para
    // donde viven los templates). Es una cabecera
    // pelada: no lleva condicion, porque la condicion vive en el HASTA de abajo.
    if (kw_es(linea, "REPETIR")) {
        abrir(p, pila, PAED_REPETIR, "", lineno);
        return 1;
    }

    // ── HASTA [QUE] <condicion> ──
    //
    // Cierra el REPETIR. Se aceptan las dos formas del material:
    //
    //     Hasta que c > 10;     template Repetir.txt
    //     HASTA (condicion)     la wiki
    //
    // No se pisa con el HASTA del PARA: ese vive DENTRO de la cabecera del
    // PARA, que ya se trato mas arriba y nunca llega aca.
    if (empieza_con(linea, "HASTA")) {
        char *cond = trim(linea + 5);

        // 'que' es opcional y no aporta nada mas que leerse bien.
        if (empieza_con(cond, "QUE")) cond = trim(cond + 3);

        // 'Hasta que c > 10;' trae el ';' de la catedra. Es un cierre de bloque,
        // asi que se lo saca como a cualquier otro.
        for (size_t n = strlen(cond); n > 0 &&
             (cond[n - 1] == ';' || isspace((unsigned char)cond[n - 1])); n = strlen(cond))
            cond[n - 1] = '\0';

        if (!*cond) {
            add_error(p, lineno, "al HASTA le falta la condicion: HASTA QUE <condicion>");
            return 1;
        }
        if (pila->tope == 0 || pila->items[pila->tope - 1].kind != PAED_REPETIR) {
            add_error(p, lineno, "HASTA sin un REPETIR abierto");
            return 1;
        }

        Abierto *a = &pila->items[pila->tope - 1];
        PAEDInstr *in = nueva_instr(p, PAED_HASTA, lineno);
        if (!in) return 1;
        strncpy(in->cond, cond, PAED_COND_MAX - 1);

        // Condicion FALSA -> volver al cuerpo, que arranca justo despues del
        // REPETIR. Es al reves que el MIENTRAS a proposito: aca la condicion
        // dice cuando TERMINAR, no cuando seguir.
        in->salto = a->instr + 1;
        pila->tope--;
        return 1;
    }

    // ── SINO ──
    if (kw_es(linea, "SINO")) {
        if (pila->tope == 0 || pila->items[pila->tope - 1].kind != PAED_SI) {
            add_error(p, lineno, "SINO sin un SI abierto");
            return 1;
        }
        Abierto *a = &pila->items[pila->tope - 1];
        if (a->sino >= 0) {
            add_error(p, lineno, "SINO repetido: el SI de la linea %d ya tiene uno", a->line);
            return 1;
        }
        PAEDInstr *in = nueva_instr(p, PAED_SINO, lineno);
        if (!in) return 1;
        int idx = p->instr_count - 1;

        // Condicion falsa -> arrancar justo despues del SINO.
        p->instrs[a->instr].salto = idx + 1;
        a->sino = idx;
        return 1;
    }

    // ── FIN_SI ──
    if (kw_es(linea, "FIN_SI")) {
        if (pila->tope == 0) { add_error(p, lineno, "FIN_SI sin un SI abierto"); return 1; }
        Abierto *a = &pila->items[pila->tope - 1];
        if (a->kind != PAED_SI) {
            // Este es el mensaje que la pila hace posible: se puede decir QUE
            // bloque quedo abierto y EN QUE LINEA.
            add_error(p, lineno, "FIN_SI cierra un %s abierto en la linea %d",
                      nombre_kind(a->kind), a->line);
            return 1;
        }
        PAEDInstr *in = nueva_instr(p, PAED_FIN_SI, lineno);
        if (!in) return 1;
        int idx = p->instr_count - 1;

        // Con SINO, el que salta al final es el SINO (el SI ya apunta a el).
        // Sin SINO, es el propio SI el que se saltea el cuerpo.
        if (a->sino >= 0) p->instrs[a->sino].salto  = idx + 1;
        else              p->instrs[a->instr].salto = idx + 1;

        pila->tope--;
        return 1;
    }

    // ── FIN_MIENTRAS y FIN_PARA ──
    // Los dos cierran un bucle y hacen exactamente lo mismo con los saltos, asi
    // que comparten el cierre en vez de tener dos copias que se desincronicen.
    if (kw_es(linea, "FIN_MIENTRAS") || kw_es(linea, "FIN_PARA")) {
        // toupper: con 'fin_para' en minuscula, linea[4] es 'p' y sin esto
        // el bucle se cerraria como si fuera un FIN_MIENTRAS.
        int      es_para = toupper((unsigned char)linea[4]) == 'P';
        PAEDKind abre    = es_para ? PAED_PARA     : PAED_MIENTRAS;
        PAEDKind cierra  = es_para ? PAED_FIN_PARA : PAED_FIN_MIENTRAS;

        if (pila->tope == 0) {
            add_error(p, lineno, "%s sin un %s abierto", linea, nombre_kind(abre));
            return 1;
        }
        Abierto *a = &pila->items[pila->tope - 1];
        if (a->kind != abre) {
            add_error(p, lineno, "%s cierra un %s abierto en la linea %d",
                      linea, nombre_kind(a->kind), a->line);
            return 1;
        }
        PAEDInstr *in = nueva_instr(p, cierra, lineno);
        if (!in) return 1;
        int idx = p->instr_count - 1;

        in->salto                 = a->instr;  // volver al principio del bucle
        p->instrs[a->instr].salto = idx + 1;   // terminado -> salir del bucle

        pila->tope--;
        return 1;
    }

    return 0;   // no era una linea de bloque
}

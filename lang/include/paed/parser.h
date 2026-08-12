#ifndef VIMMON_PARSER_H
#define VIMMON_PARSER_H

// Parser de PAED (pseudocodigo). Un solo lenguaje, una sola gramatica.
//
// La definicion formal del lenguaje NO vive en este header: vive en
// paed/Frankly/data/sintaxis.json y se carga en runtime con cJSON.
// Si agregas un procedimiento o un parametro, se agrega ahi y nada mas.

#define PAED_NAME_MAX      64
#define PAED_KEY_MAX       32
#define PAED_VAL_MAX      128
#define PAED_MAX_ARGS      16
#define PAED_MAX_DECLS     64
#define PAED_MAX_INSTRS   256
#define PAED_MAX_ERRORS    32
#define PAED_MSG_MAX      192
#define PAED_PATH_MAX     256
#define PAED_COND_MAX     192
// Cuantos bloques se pueden anidar. 32 niveles de SI dentro de MIENTRAS dentro
// de SI es mucho mas de lo que aguanta leer un humano.
#define PAED_MAX_BLOQUES   32

// El archivo con la definicion formal del lenguaje. Vive en el DIRECTORIO DE
// DATOS, que se resuelve en runtime — ver paed_datadir().
#define PAED_SYNTAX_FILE "sintaxis.json"

// Argumento con nombre: clave = valor.
// En procedimientos variadicos (ESCRIBIR) key queda vacio y solo vale val.
typedef struct {
    char key[PAED_KEY_MAX];
    char val[PAED_VAL_MAX];
} PAEDArg;

// Declaracion del bloque AMBIENTE: nombre: TIPO;
// Tambien   nombre: ARREGLO[desde..hasta] DE TIPO;
//           nombre: ARCHIVO DE TIPO;
//
// ARREGLO y ARCHIVO son dos ENVOLTORIOS sobre un tipo base, y por eso se
// parsean igual: `type` guarda el tipo de adentro en los dos casos. Lo que
// cambia es donde vive el dato — el arreglo en memoria, el archivo en disco.
typedef struct {
    char name[PAED_NAME_MAX];
    char type[PAED_NAME_MAX];   // el tipo BASE: ENTERO, REAL, CARACTER, LOGICO
    int  es_arreglo;
    // Limites del arreglo, los dos inclusive. En AED los elige el programador
    // y no arrancan en 0: ARREGLO[1..10] va del 1 al 10.
    int  desde, hasta;

    // Archivo. Saber que una variable es un archivo es lo que permite
    // distinguir `LEER(arch, reg)` (avanzar un registro) de `LEER(A, B)`
    // (pedir datos por consola): las dos formas se escriben igual y tienen
    // dos argumentos, asi que lo unico que las separa es esta declaracion.
    int  es_archivo;

    int  line;
} PAEDDecl;

// Que clase de instruccion es. Antes toda linea del PROCESO era una llamada;
// con los bloques hay lineas que no llaman a nada (SINO, FIN_SI) y otras que
// llevan una condicion en vez de argumentos.
typedef enum {
    PAED_LLAMADA = 0,     // PROCEDIMIENTO(clave = valor, ...);
    PAED_ASIGNA,          // destino := expresion;
    PAED_SI,              // SI (condicion) ENTONCES
    PAED_SINO,            // SINO
    PAED_FIN_SI,          // FIN_SI
    PAED_MIENTRAS,        // MIENTRAS (condicion) HACER
    PAED_FIN_MIENTRAS,    // FIN_MIENTRAS
    PAED_PARA,            // PARA <var> := <desde> HASTA <hasta> HACER
    PAED_FIN_PARA,        // FIN_PARA
} PAEDKind;

// Algunos procedimientos de AED se escriben IGUAL pero hacen cosas distintas
// segun sobre que operen:
//
//     LEER(salario)       pide un dato por consola
//     LEER(arch, reg)     lee el proximo registro del archivo y AVANZA
//
// No se distinguen contando argumentos: `LEER(clave, cod_mov)` es consola y
// tiene dos, igual que la de archivo (wiki.txt:2761 y :2765, en el MISMO
// algoritmo). Lo unico que las separa es si el primer argumento se declaro
// como `archivo de X` en el AMBIENTE, y eso lo resuelve el parser.
//
// La teoria pone `Leer(Arch, Reg)` en la misma fila que `Avanzar(Sec, v)`
// (TEORIA_COMPLETA.txt:1107): son operaciones hermanas, no la misma.
typedef enum {
    PAED_FORMA_UNICA = 0,   // el procedimiento tiene una sola forma
    PAED_FORMA_CONSOLA,     // LEER(x)       ESCRIBIR("hola")
    PAED_FORMA_ARCHIVO,     // LEER(arch,r)  ESCRIBIR(arch,r)
} PAEDForma;

// Instruccion del bloque PROCESO.
typedef struct {
    PAEDKind kind;
    PAEDForma forma;      // cual de las dos formas es, si el proc tiene dos
    // LLAMADA: nombre del proc. ASIGNA: destino. PARA: la variable del bucle,
    // con los limites en args como desde/hasta.
    char    proc[PAED_NAME_MAX];
    PAEDArg args[PAED_MAX_ARGS];
    int     arg_count;
    int     line;

    // Condicion de SI/MIENTRAS, o la expresion de la derecha de un ':='.
    // Se guarda CRUDA: todavia no hay evaluador de expresiones.
    char    cond[PAED_COND_MAX];

    // A donde salta el flujo. Lo completa el parser cuando CIERRA el bloque,
    // porque al abrirlo todavia no sabe donde termina (esto se llama
    // "backpatching"). Vale -1 en las instrucciones que no saltan.
    //   SI            -> primera instruccion del SINO, o la de despues del FIN_SI
    //   SINO          -> la de despues del FIN_SI
    //   MIENTRAS      -> la de despues del FIN_MIENTRAS (cuando la condicion es falsa)
    //   FIN_MIENTRAS  -> el MIENTRAS de arriba (para volver a evaluar)
    int     salto;
} PAEDInstr;

typedef struct {
    int  line;
    char msg[PAED_MSG_MAX];
} PAEDError;

// Tipo REGISTRO declarado en el AMBIENTE:
//
//     vector2 = REGISTRO
//         vx: REAL;
//         vy: REAL;
//     FIN_REGISTRO
//
// Es el `struct` de C con otro nombre. Guarda QUE campos tiene el tipo; las
// variables de ese tipo se declaran aparte (`pori: vector2;`).
#define PAED_MAX_REGISTROS 16
#define PAED_MAX_CAMPOS    16

typedef struct {
    char     name[PAED_NAME_MAX];      // vector2
    PAEDDecl campos[PAED_MAX_CAMPOS];  // vx: REAL, vy: REAL
    int      campo_count;
    int      line;
} PAEDRegistro;

typedef struct {
    char      path[PAED_PATH_MAX];
    char      name[PAED_NAME_MAX];          // el <nombre> de ACCION <nombre> ES
    PAEDDecl  decls  [PAED_MAX_DECLS];
    int       decl_count;
    PAEDRegistro registros[PAED_MAX_REGISTROS];
    int          registro_count;
    PAEDInstr instrs [PAED_MAX_INSTRS];
    int       instr_count;
    PAEDError errors [PAED_MAX_ERRORS];
    int       error_count;
} PAEDProgram;

// ── Donde vive la definicion del lenguaje ────────────────────────────────────
//
// Un lenguaje que se instala en cualquier maquina no puede tener la ruta de su
// propia definicion clavada al repo donde nacio. Se busca, en este orden, el
// primer directorio que contenga sintaxis.json:
//
//   1. $PAED_HOME                 — lo pisa todo, para desarrollo y para probar
//                                   una definicion sin instalarla (como PYTHONHOME)
//   2. PAED_DATADIR               — donde lo dejo `make install`, fijado al compilar
//   3. Frankly/data               — corriendo parado en el repo de PAED
//   4. paed/Frankly/data          — corriendo parado en VimMon, que lo tiene adentro
//
// Devuelve el directorio elegido, o NULL si en ninguno estaba sintaxis.json.
const char *paed_datadir(void);

// Carga sintaxis.json. Idempotente: la segunda llamada no hace nada.
// Devuelve 0 si esta cargada, -1 si no pudo leerla.
int  paed_syntax_load(void);

// Carga una libreria de procedimientos que NO son del lenguaje, buscandola en
// el mismo directorio de datos: paed_syntax_load_lib("escena") lee escena.json.
//
// Se pide por NOMBRE y no por ruta a proposito: la ruta depende de desde donde
// se corra, y entonces el mismo programa daria mensajes distintos en una
// maquina y en otra.
//
// Es lo que hace VimMon para sumar su escena 3D. Sin esto, PAED es AED puro.
// Devuelve 0 si la cargo, -1 si no.
int  paed_syntax_load_lib(const char *nombre);

void paed_syntax_free(void);

// Analiza el archivo completo. Devuelve 0 si no hubo NINGUN error.
// Si devuelve != 0, out->errors tiene el detalle: el programa no debe ejecutarse.
int  paed_parse_file(const char *path, PAEDProgram *out);

// Devuelve el valor de un argumento con nombre, o NULL si no esta.
const char *paed_get_arg(const PAEDInstr *instr, const char *key);

// Imprime todos los errores en stderr con formato archivo:linea: error: msg
void paed_print_errors(const PAEDProgram *prog);

#endif // VIMMON_PARSER_H

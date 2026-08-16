#ifndef VIMMON_ARCHIVO_H
#define VIMMON_ARCHIVO_H

#include <stddef.h>
#include <stdio.h>
#include "parser.h"

// Archivos de AED, guardados en disco como CSV.
//
//     ABRIR E/(mae);            abre para leer
//     LEER(mae, reg);           trae el proximo registro y avanza
//     MIENTRAS NFDA(mae)        mientras el ultimo LEER haya traido algo
//     CERRAR(mae);
//
// En disco cada archivo es un .csv con FILA DE ENCABEZADO: los nombres de los
// campos del REGISTRO. El motivo es que se pueda MIRAR — el registro ya es una
// fila con columnas, y un formato binario guarda lo mismo sin que se pueda
// abrir en una planilla. Ver docs/PAED.md §2.6.
//
// Este modulo tiene SOLO el estado, el CSV y el acceso al disco. No imprime ni
// reporta con archivo:linea — eso lo hace el interprete, que tiene el programa
// y la instruccion a mano. Y la CONVERSION de tipos tampoco es de aca: aca todo
// es texto, porque el que sabe que campo es ENTERO y cual AN(20) es el que
// tiene el REGISTRO.
//
// Vive aparte de interpreter.c por el mismo motivo que secuencia.c: el
// evaluador de expresiones necesita el estado para FDA y NFDA, y meterlo en el
// interprete haria que expr.c dependiera del interprete que ya depende de el.

// Ocho archivos abiertos a la vez. Una actualizacion secuencial usa cinco
// (maestro, movimientos, maestro nuevo, bajas, errores); ocho deja aire.
#define PAED_MAX_ARCHIVOS 8

// Cuanto entra en una fila del CSV. Un registro de un parcial son cuatro o
// cinco campos cortos; 1024 es holgado y sigue siendo memoria estatica.
#define PAED_CSV_LINEA 1024

// El separador de campos. Es ';' y no ',' a proposito, decidido el 2026-08-14:
// el motivo de usar CSV era poder abrirlo y ver la planilla, y con coma Excel
// configurado en español lo abre con todo apelmazado en la columna A.
//
// No choca con los numeros: el decimal de PAED es el punto, asi que la coma no
// estaba ocupada — el ';' se elige por la herramienta, no por el dato.
//
// Esta en UN solo lugar: si algun dia el archivo tiene que viajar a algo que
// exige coma, se cambia aca y nada mas.
#define PAED_CSV_SEP ';'

typedef struct {
    char nombre[PAED_NAME_MAX];   // la variable del AMBIENTE: 'mae'
    char ruta  [PAED_PATH_MAX];   // 'mae.csv', al lado del .paed

    // Los campos del REGISTRO, en orden de declaracion. Son el encabezado del
    // CSV y el orden de las columnas.
    char campos[PAED_MAX_CAMPOS][PAED_NAME_MAX];
    int  campo_count;

    FILE *f;
    char  modo[PAED_MODO_MAX];    // "E", "S" o "ES"; vacio si se abrio sin modo
    int   abierto;
    int   cerrado;

    // FDA: el ultimo LEER no encontro ninguna fila.
    //
    // No es "el archivo esta vacio" ni una mirada adelantada: es el resultado
    // del ultimo intento. Eso es lo que hace andar el bucle de AED, donde el
    // LEER de abajo del ciclo es el que termina poniendolo en 1:
    //
    //     LEER(mae, reg);
    //     MIENTRAS NFDA(mae) HACER
    //         ...
    //         LEER(mae, reg);
    //     FIN_MIENTRAS
    int   fin;
} Archivo;

// Vacia la tabla y cierra lo que haya quedado abierto. La llama el interprete
// antes de cada programa: dos corridas en el mismo proceso no pueden compartir
// la posicion de lectura ni los descriptores.
void arch_reset(void);

// La carpeta donde vive el .paed. El .csv se crea al lado: un parcial es una
// carpeta con el programa y sus datos.
//
// Vive aca y no en el interprete porque no la usa solo el: el asistente de
// archivos tiene que crear el .csv en el MISMO lugar donde el interprete lo va
// a buscar despues. Dos copias de esta cuenta serian dos lugares distintos, y
// el asistente crearia un archivo que el programa despues no encuentra.
//
// Se miran las dos barras porque el mismo binario se cross-compila para
// Windows, donde el separador es '\\'.
void arch_dir_del_programa(const char *path_paed, char *out, size_t n);

// Anota un archivo declarado en el AMBIENTE. `dir` es la carpeta del .paed: el
// .csv se crea al lado del programa, con el nombre de la variable.
//
// El nombre sale de la variable y no de un argumento porque la catedra NO
// escribe rutas en ningun lado del corpus. Inventar un argumento de ruta seria
// agregar sintaxis que las fuentes no tienen.
Archivo *arch_declarar(const char *nombre, const char *dir);

Archivo *arch_buscar(const char *nombre);

// Le dice que campos tiene su REGISTRO. Sin esto no se puede ni escribir el
// encabezado ni validarlo.
int arch_set_campos(Archivo *a, const char campos[][PAED_NAME_MAX], int n);

// ── Operaciones ──────────────────────────────────────────────────────────────
//
// Todas devuelven 0 si salieron bien y -1 si no, dejando el motivo en
// arch_error(). Ninguna imprime.

// Crea el archivo y le escribe el encabezado. PISA el que ya exista: en la
// actualizacion secuencial el maestro nuevo se crea en CADA corrida, y si la
// segunda diera error habria que borrarlo a mano entre corrida y corrida.
int arch_crear(Archivo *a, const char *modo);

// Abre uno que ya existe y VALIDA su encabezado contra los campos del
// REGISTRO. Un archivo de movimientos abierto como maestro se leeria sin
// protestar y devolveria basura con forma de dato valido: ese es el error mas
// caro de los archivos, y el encabezado lo ataja gratis.
int arch_abrir(Archivo *a, const char *modo);

int arch_cerrar(Archivo *a);

// Trae la proxima fila, ya partida en campos y sin comillas. Deja `*hay` en 1
// si trajo una y en 0 si el archivo se termino — que NO es un error: es el
// avance que pone FDA en 1, y el bucle de AED cuenta con el.
int arch_leer(Archivo *a, char valores[][PAED_VAL_MAX], int *hay);

// Agrega una fila al final, con los valores en el orden de los campos.
int arch_grabar(Archivo *a, const char valores[][PAED_VAL_MAX]);

const char *arch_error(void);

#endif // VIMMON_ARCHIVO_H

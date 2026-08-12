#ifndef VIMMON_SECUENCIA_H
#define VIMMON_SECUENCIA_H

#include <stddef.h>   // size_t
#include "parser.h"

// Secuencias de AED — la estructura que toman los parciales.
//
// Una secuencia es una tira de elementos que se recorre HACIA ADELANTE y de a
// uno. No se indexa, no se vuelve atras: eso es justamente lo que la distingue
// de un arreglo, y por que su recorrido es siempre el mismo bucle.
//
//     arr(sec);              posiciona ANTES del primer elemento
//     avz(sec, v);           trae el proximo elemento a 'v'
//     MIENTRAS NFDS(sec)     mientras la ventana no se paso del ultimo
//
// La forma sale del corpus de la catedra (apuntes/AED/Simulacros): `arr` no lee
// nada, y el primer `avz` es el que trae el primer elemento. Por eso el modismo
// arranca siempre con los dos juntos:
//
//     arr(secAlu); avz(secAlu, v);
//
// Este modulo tiene SOLO el estado y el avance. No imprime ni reporta errores
// con archivo:linea — eso lo hace el interprete, que es el que tiene el
// programa y la instruccion a mano. Vive aparte de interpreter.c porque el
// evaluador de expresiones tambien lo necesita, para FDS y NFDS, y hacerlo al
// reves crearia una dependencia circular entre los dos.

// Ocho secuencias abiertas a la vez. Los parciales usan tres o cuatro (una de
// caracteres, una de enteros, una de salida); ocho deja aire sin gastar nada.
#define PAED_MAX_SECUENCIAS 8

// Cuanto texto entra en una secuencia. Los datos de un parcial son un par de
// renglones; 4096 es holgado y sigue siendo memoria estatica, sin malloc, como
// el resto del proyecto.
#define PAED_SEC_MAX 4096

typedef struct {
    char nombre[PAED_NAME_MAX];

    // Un elemento es UN CARACTER, o un token separado por espacios. Lo decide
    // el tipo declarado, no el dato: en `secuencia de caracter` el elemento
    // '5' es el caracter '5' y NO el numero 5, y esa diferencia importa —
    // ConvertiraNumero('5') existe justamente porque no son lo mismo.
    int de_caracteres;

    // Secuencia de salida: no se lee, se escribe. `CREAR` la abre, `ESCRIBIR`
    // le agrega y `CERRAR` la termina.
    int es_salida;

    char datos[PAED_SEC_MAX];
    int  largo;      // cuanto ocupa `datos`
    int  pos;        // por donde va la lectura dentro de `datos`

    int  abierta;    // ARR o CREAR ya la abrieron
    int  cerrada;    // CERRAR ya la termino
    int  fin;        // la ventana se paso del ultimo elemento (FDS)
} Secuencia;

// Vacia la tabla. La llama el interprete antes de cada programa: dos corridas
// seguidas en el mismo proceso no pueden compartir la posicion de lectura.
void sec_reset(void);

// Anota una secuencia declarada en el AMBIENTE. `tipo_base` es lo que vino
// despues del DE (CARACTER, CARACTERES, ENTERO, SALIDA...).
// Devuelve la secuencia, o NULL si no entran mas.
Secuencia *sec_declarar(const char *nombre, const char *tipo_base, int es_salida);

// La busca por nombre. NULL si no esta declarada.
Secuencia *sec_buscar(const char *nombre);

// Le carga los datos de entrada, tal como los trajo el host.
// Devuelve 0 si entraron, -1 si no: el motivo queda en sec_error().
int sec_cargar(Secuencia *s, const char *datos);

// ── Operaciones ──────────────────────────────────────────────────────────────
//
// Todas devuelven 0 si salieron bien y -1 si no, dejando el motivo en
// sec_error(). Ninguna imprime: el que reporta es el interprete.

int sec_arrancar(Secuencia *s);
int sec_crear   (Secuencia *s);
int sec_cerrar  (Secuencia *s);

// Trae el proximo elemento a `buf`. Deja `*hay` en 1 si trajo uno, y en 0 si la
// secuencia se termino — que NO es un error: es el avance que pone la ventana
// en FDS, y el bucle `MIENTRAS NFDS(sec)` cuenta con el.
//
// Avanzar cuando la ventana YA estaba en FDS si es un error: significa que el
// programa siguio leyendo despues del final, y dejarlo pasar en silencio
// convierte un bug en un bucle raro tres instrucciones mas abajo.
int sec_avanzar(Secuencia *s, char *buf, size_t n, int *hay);

// Le agrega texto a una secuencia de salida.
int sec_grabar(Secuencia *s, const char *texto);

// El motivo del ultimo fallo. Cadena vacia si no hubo ninguno.
const char *sec_error(void);

#endif // VIMMON_SECUENCIA_H

#ifndef PAED_APRENDER_H
#define PAED_APRENDER_H

// `paed aprender` — el tutorial de PAED, al estilo de rustlings.
//
// La idea: una serie de programas ROTOS, ordenados de menos a mas. El alumno
// arregla uno, el tutor lo corre, y si la salida coincide con la que el propio
// ejercicio declara, pasa al siguiente.
//
// ── Tres decisiones que explican todo lo demas ─────────────────────────────
//
// 1. NO HAY ARCHIVO DE PROGRESO.
//
//    El ejercicio actual es "el primero que todavia no pasa", y eso se
//    CALCULA corriendolos en orden. Un archivo de estado seria una segunda
//    fuente de verdad que se puede desincronizar del disco: borras un
//    ejercicio, editas otro a mano, copias la carpeta a otra maquina, y el
//    archivo miente. Correr doce programas de veinte lineas cuesta
//    milisegundos, y a cambio el estado NO PUEDE estar mal.
//
// 2. EL JUEZ ES LA SALIDA, Y VIVE EN EL MISMO ARCHIVO.
//
//    Cada .paed lleva al final su bloque `// ── SALIDA ESPERADA ──`, igual
//    que los tests de tests/correr.sh. Que el alumno la vea NO es hacer
//    trampa: es el enunciado — le dice que tiene que lograr. Borrarla para
//    "aprobar" solo se engaña a si mismo, y ningun diseño evita eso.
//
// 3. EL EJERCICIO SE CORRE COMO SUBPROCESO, no adentro de este mismo binario.
//
//    Cuesta un proceso por ejercicio (microsegundos) y compra tres cosas:
//    un ejercicio que se cuelga o que revienta NO se lleva puesto al tutor,
//    la salida se captura sin manosear el stdout del proceso propio, y sobre
//    todo el tutor juzga EXACTAMENTE el mismo comando que el alumno tipearia
//    a mano. Sin eso podria pasar que un ejercicio "apruebe" en el tutorial y
//    falle al correrlo de verdad.
//
// ── Los ejercicios viajan adentro del binario ──────────────────────────────
//
// Igual que sintaxis.json: el Makefile los mete en el ejecutable. Por eso el
// que baja `paed` suelto de un release tiene el tutorial completo sin clonar
// nada, y por eso `aprender init` puede DESEMPACARLOS en una carpeta de
// trabajo. El alumno edita la copia; el original queda intacto adentro del
// binario y es lo que permite que `aprender reset` exista.

// Corre el subcomando. `argv` es el de main SIN el nombre del programa: o sea
// argv[0] == "aprender". Devuelve el codigo de salida del proceso.
int paed_aprender(int argc, char **argv);

// Un ejercicio embebido, tal como lo genera el Makefile desde aprender/*.paed.
typedef struct {
    const char *nombre;      // "0-01_escribir.paed"
    const char *contenido;   // el archivo entero
} PaedEjercicio;

extern const PaedEjercicio PAED_EJERCICIOS[];
extern const int           PAED_EJERCICIOS_N;

// ── Los modulos ────────────────────────────────────────────────────────────
//
// El tutorial esta partido en temas, y el tema de un ejercicio se lee en su
// propio nombre: "1-03_cadenas.paed" es del modulo 1. No hay tabla que ligue
// ejercicios con modulos — el nombre del archivo ES la tabla, igual que el
// numero del archivo ES el orden.
//
// Lo unico que hace falta guardar es COMO SE LLAMA cada modulo, y eso vive en
// aprender/modulos.txt y viaja horneado igual que los ejercicios.
//
// Un ejercicio cuyo prefijo no figure en modulos.txt sigue siendo un
// ejercicio: se muestra sin nombre de modulo y nada mas.
typedef struct {
    int         numero;      // 1
    const char *nombre;      // "Secuencias de datos elementales y subacciones"
} PaedModulo;

extern const PaedModulo PAED_MODULOS[];
extern const int        PAED_MODULOS_N;

// ── Los moldes ─────────────────────────────────────────────────────────────
//
// Un ejercicio que todavia NO declara su bloque `// -- SALIDA ESPERADA` es un
// MOLDE: esta en el repo con su nombre y su tema, pero no esta escrito. El
// tutorial lo saltea entero — no lo desempaca, no lo cuenta, no lo muestra —
// y `make test` no le pide solucion.
//
// Es lo que permite dejar la estructura completa del curso puesta desde el
// principio y llenarla de a un ejercicio por vez, sin que el tutorial mienta
// sobre cuantos ejercicios hay ni que el test se ponga rojo por algo que
// todavia no existe.

#endif // PAED_APRENDER_H

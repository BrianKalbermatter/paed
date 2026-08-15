#ifndef PAED_PLATAFORMA_H
#define PAED_PLATAFORMA_H

#include <stddef.h>

// Lo poco que PAED necesita del sistema operativo y que Linux y Windows no
// escriben igual. Se junta todo aca para que el resto del codigo no tenga
// #ifdef desparramados: el dia que haya un tercer sistema se toca UN archivo.
//
// Es exactamente el mismo criterio que `renderer.h` en VimMon — la interfaz
// dice QUE se necesita, cada implementacion dice COMO.

// La ruta completa del ejecutable que se esta corriendo.
//
//   Linux    /proc/self/exe
//   Windows  GetModuleFileNameA
//
// Devuelve 0 si pudo, -1 si no.
int paed_ruta_ejecutable(char *out, size_t n);

// La carpeta que contiene a `ruta`, EN EL LUGAR (le corta el ultimo tramo).
// Entiende las dos barras: Windows acepta '/' pero devuelve '\'.
// Devuelve 0 si pudo, -1 si la ruta no tenia ninguna barra.
int paed_dirname(char *ruta);

// mkdir de un solo nivel. En Windows no lleva permisos.
// Devuelve 0 si la creo, -1 si no (incluido "ya existe").
int paed_mkdir(const char *ruta);

// Busca `aguja` adentro de `pajar` SIN distinguir mayusculas, y devuelve donde
// empieza (NULL si no esta). Es `strcasestr`, que existe en glibc pero NO es C
// estandar: MinGW no la tiene y el .exe no compilaba por eso.
char *paed_strcasestr(const char *pajar, const char *aguja);

// Cuando se modifico `ruta` por ultima vez, en segundos. Lo usa el modo que
// mira un archivo y vuelve a correrlo cuando lo guardas: la unica forma
// portable de preguntar "¿esto cambio?" sin quedarse escuchando al sistema de
// archivos, que en Linux es inotify y en Windows es otra cosa completamente.
//
// Un segundo de resolucion alcanza de sobra: entre que guardas y que mirás la
// terminal pasa mas que eso.
//
// Devuelve 0 si pudo, -1 si el archivo no esta.
int paed_mtime(const char *ruta, long long *out);

// Frena el proceso `ms` milisegundos. Sin esto, un ciclo que pregunta "¿cambio
// el archivo?" se come un nucleo entero preguntando millones de veces por
// segundo.
void paed_dormir_ms(int ms);

// El separador de carpetas del sistema, para armar rutas y para los mensajes.
// Y la extension del ejecutable: en Windows un archivo sin .exe no se ejecuta.
#ifdef _WIN32
#define PAED_SEP "\\"
#define PAED_EXE ".exe"
#else
#define PAED_SEP "/"
#define PAED_EXE ""
#endif

#endif // PAED_PLATAFORMA_H

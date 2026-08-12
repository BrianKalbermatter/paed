#include "paed/plataforma.h"

#include <ctype.h>
#include <string.h>

#ifdef _WIN32
  #include <windows.h>
  #include <direct.h>    // _mkdir
#else
  #include <sys/stat.h>
  #include <unistd.h>    // readlink
#endif

int paed_ruta_ejecutable(char *out, size_t n) {
#ifdef _WIN32
    // GetModuleFileNameA con NULL pide el ejecutable del proceso. Devuelve
    // cuantos bytes escribio; si llena el buffer entero, la ruta no entro y no
    // hay forma de saber cuanto faltaba, asi que se trata como error.
    DWORD largo = GetModuleFileNameA(NULL, out, (DWORD)n);
    if (largo == 0 || largo >= n) return -1;
    out[largo] = '\0';
    return 0;
#else
    // /proc/self/exe apunta al ejecutable REAL, ya resueltos los symlinks, asi
    // que un `paed` que en verdad es un enlace igual encuentra sus datos.
    ssize_t largo = readlink("/proc/self/exe", out, n - 1);
    if (largo <= 0) return -1;
    out[largo] = '\0';
    return 0;
#endif
}

int paed_dirname(char *ruta) {
    // Se buscan LAS DOS barras y gana la que este mas a la derecha. Windows
    // acepta '/' en las rutas que uno escribe pero devuelve '\' en las que el
    // arma, asi que una ruta puede tener las dos mezcladas:
    //     C:\Users\brian/paed.exe
    char *barra  = strrchr(ruta, '/');
    char *contra = strrchr(ruta, '\\');
    char *corte  = barra > contra ? barra : contra;

    if (!corte) return -1;
    *corte = '\0';
    return 0;
}

int paed_mkdir(const char *ruta) {
#ifdef _WIN32
    return _mkdir(ruta);
#else
    return mkdir(ruta, 0755);
#endif
}

char *paed_strcasestr(const char *pajar, const char *aguja) {
    size_t n = strlen(aguja);
    if (n == 0) return (char *)pajar;

    // Busqueda directa, sin trucos: se prueba en cada posicion. Es O(n*m) y
    // esta bien — los pajares aca son LINEAS de codigo, no archivos enteros, y
    // el parser ya recorre el archivo linea por linea.
    for (const char *c = pajar; *c; c++) {
        size_t i = 0;
        while (i < n && tolower((unsigned char)c[i]) == tolower((unsigned char)aguja[i])) i++;
        if (i == n) return (char *)c;
    }
    return NULL;
}

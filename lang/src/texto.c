// Cortar y reconocer texto. Nada mas que eso.
//
// Son las funciones mas usadas del parser —trim aparece 49 veces, y
// es_identificador 14— y no dependen de ninguna otra parte: no leen
// sintaxis.json, no tocan el programa, no reportan errores. Entra un char* y
// sale un char* o un si/no.
//
// Salieron de parser.c antes que los bloques grandes a proposito: los cinco
// modulos que faltan las necesitan a todas, asi que mientras vivieran adentro
// del parser ningun otro corte era posible sin arrastrarlo entero.
//
// Que no tengan estado ni dependencias es lo que las hace faciles de testear
// sueltas, si algun dia hace falta.

#include "texto.h"

#include <ctype.h>
#include <string.h>

// ── Utilidades de texto ───────────────────────────────────────────────────────

char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char *fin = s + strlen(s) - 1;
    while (fin > s && isspace((unsigned char)*fin)) *fin-- = '\0';
    return s;
}

// Corta el comentario y el salto de linea. La catedra escribe comentarios de
// TRES formas, y las tres estan en material oficial:
//
//     // hasta fin de linea      la wiki, y todos los .paed del repo
//     { entre llaves }           templates de UTN-FRRe/isi-aed/Pseudocodigo
//     * entre asteriscos *       diapositivas de los Temas 12 y 13
//
// Las dos ultimas solo se reconocen cuando ABREN LA LINEA, y no es una
// limitacion caprichosa: los dos caracteres ya significan otra cosa en el medio
// de una linea.
//
//     d: {1..31};        <- '{' despues de ':' es un tipo RANGO, no un comentario
//     a := b * c;        <- '*' entre operandos es multiplicacion
//     x := 2 ** 3;       <- y '**' es potencia
//
// Todos los comentarios con llave del material oficial ocupan su propio renglon
// ({Asigno el valor a retornar}, {Avanzo el guion}), asi que la regla no pierde
// ningun caso real y no puede comerse un tipo ni una cuenta.
void strip_comment(char *s) {
    // El '{' o el '* ' de apertura pueden venir con sangria adelante.
    char *ini = s;
    while (*ini == ' ' || *ini == '\t') ini++;

    if (*ini == '{') {
        char *cierre = strchr(ini, '}');
        // Sin '}' el comentario se come el resto de la linea, que es lo que el
        // que lo escribio quiso decir.
        if (cierre) memmove(ini, cierre + 1, strlen(cierre + 1) + 1);
        else        *ini = '\0';
        // Puede quedar codigo despues del comentario: se vuelve a mirar.
        strip_comment(s);
        return;
    }

    // '* ' con espacio: '*p' es desreferencia y '**' es potencia, ninguno abre
    // comentario. Se pide el espacio para no confundirlos.
    if (ini[0] == '*' && (ini[1] == ' ' || ini[1] == '\t')) { *ini = '\0'; return; }

    char comilla = 0;
    for (char *c = s; *c; c++) {
        if (!comilla && (*c == '"' || *c == '\'')) comilla = *c;
        else if (comilla && *c == comilla)          comilla = 0;
        if (!comilla && c[0] == '/' && c[1] == '/') { *c = '\0'; return; }
    }
}

// ¿Este byte puede ser parte de un nombre?
//
// Las letras ASCII, los digitos y el '_' de siempre, MAS cualquier byte de
// UTF-8 multibyte (>= 0x80). La catedra escribe campos con ñ y con tildes —
// 'año' aparece en REGISTRO.txt, ARCHIVO_CREAR.txt y ARCHIVO_LEER.txt, los tres
// templates oficiales — y rechazarlos obligaria a reescribir sus programas para
// correrlos.
//
// Se acepta el byte crudo sin decodificar el punto de codigo: los nombres se
// comparan y se guardan como bytes, asi que alcanza con dejarlos pasar enteros.
// Las palabras clave siguen siendo ASCII puro, asi que ningun nombre con ñ
// puede chocar con una.
static int byte_de_nombre(unsigned char c) {
    return isalnum(c) || c == '_' || c >= 0x80;
}

// Igual, pero para el PRIMER byte: un nombre no puede empezar con digito.
static int byte_inicial_de_nombre(unsigned char c) {
    return isalpha(c) || c == '_' || c >= 0x80;
}

int es_identificador(const char *s) {
    if (!*s || !byte_inicial_de_nombre((unsigned char)*s)) return 0;
    for (const char *c = s; *c; c++)
        if (!byte_de_nombre((unsigned char)*c)) return 0;
    return 1;
}

// Igual que es_identificador, pero acepta el punto de acceso a campo:
// 'pori.vx' es un nombre valido y 'pori' tambien.
//
// El punto tiene que estar ENTRE dos partes validas. Asi '.x', 'pori.' y
// 'pori..vx' se siguen rechazando, y un numero como 1.5 nunca entra aca porque
// no empieza con letra.
int es_campo(const char *s) {
    if (!*s || !byte_inicial_de_nombre((unsigned char)*s)) return 0;

    for (const char *c = s; *c; c++) {
        if (*c == '.') {
            // ni al principio, ni al final, ni dos seguidos
            if (c == s || !byte_inicial_de_nombre((unsigned char)c[1])) return 0;
            continue;
        }
        if (!byte_de_nombre((unsigned char)*c)) return 0;
    }
    return 1;
}

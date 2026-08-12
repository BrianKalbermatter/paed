// paedrun — corre un programa PAED desde la terminal, sin abrir la ventana.
//
// El interprete vive dentro del game loop, asi que hasta ahora la unica forma
// de probar un .paed era abrir SDL y mirar. Eso no se puede automatizar ni
// meter en un script: un test que hay que mirar no es un test.
//
// Este arnes linkea SOLO el parser, el evaluador y el interprete. No toca SDL
// ni el bus. ESCRIBIR ya imprime por stdout, asi que la salida se puede
// comparar contra un archivo esperado.
//
//   build/paedrun paed/Frankly/tests/busqueda_lineal.paed
//
// Codigos de salida, para poder encadenarlo con && en la shell:
//   0  el programa corrio entero
//   1  error de parseo o de ejecucion
//   2  mal uso
//   3  no se pudo cargar sintaxis.json

#include <stdio.h>
#include <string.h>

#include <paed/parser.h>
#include <paed/interpreter.h>

// De aca saca LEER sus datos. El interprete no abre stdin solo (ver el
// comentario de interp_set_entrada): en la ventana SDL eso congelaria el game
// loop. Aca, en cambio, stdin es exactamente lo que corresponde — sea el que
// tipea o una tuberia con los datos del test.
//
// Una linea mas larga que el buffer se parte: lo que sobra queda como la
// proxima linea, y el proximo LEER se lo come. Es el comportamiento de fgets y
// se prefiere a truncar callado.
static int leer_de_stdin(char *buf, size_t n, void *ud) {
    (void)ud;
    if (!fgets(buf, (int)n, stdin)) return -1;   // fin de la entrada
    buf[strcspn(buf, "\n")] = '\0';
    return 0;
}

int main(int argc, char **argv) {
    const char *path = NULL;

    // ESCRIBIR va a stdout y los errores a stderr. Cuando la salida es una
    // tuberia y no la terminal, stdout pasa a tener buffer y stderr no: los
    // errores se adelantan y aparecen ANTES de lineas que en realidad se
    // imprimieron primero. Sin esto, comparar la salida contra un archivo
    // esperado da diferencias que dependen de si hay tuberia o no.
    setvbuf(stdout, NULL, _IONBF, 0);

    // Se cayo `--escena`: mostraba la escena 3D, que ya no es parte del
    // lenguaje. Los cuerpos los pone VimMon registrando sus procedimientos, y
    // este runner corre PAED pelado.
    //
    // `--lib <nombre>` carga una libreria de procedimientos que no son del
    // lenguaje (--lib escena lee escena.json del directorio de datos). Sirve
    // para VALIDAR un programa que la usa; ejecutarla es otra cosa, y para eso
    // hace falta el host que implemente esos procedimientos.
    const char *lib = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lib") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "paed: --lib necesita un nombre (ej: --lib escena)\n");
                return 2;
            }
            lib = argv[++i];
        }
        else if (!path) path = argv[i];
        else {
            fprintf(stderr, "paed: argumento de mas: %s\n", argv[i]);
            return 2;
        }
    }

    if (!path) {
        fprintf(stderr, "uso: paed [--lib <nombre>] <archivo.paed>\n");
        return 2;
    }

    // sintaxis.json NO se busca al lado del .paed: se busca en el directorio de
    // datos de PAED, que sale de $PAED_HOME, de la instalacion o del repo. Por
    // eso este binario anda desde cualquier carpeta una vez instalado.
    if (paed_syntax_load() != 0) {
        // paed_syntax_load ya explico donde busco. Aca solo se traduce a un
        // codigo de salida, para poder encadenarlo en la shell.
        return 3;
    }

    if (lib && paed_syntax_load_lib(lib) != 0) {
        fprintf(stderr, "paed: no se pudo cargar la libreria '%s'\n", lib);
        return 3;
    }

    PAEDProgram prog;
    if (paed_parse_file(path, &prog) != 0) {
        paed_print_errors(&prog);
        paed_syntax_free();
        return 1;
    }

    interp_set_entrada(leer_de_stdin, NULL);
    int rc = interp_exec(&prog);

    paed_syntax_free();
    return rc == 0 ? 0 : 1;
}

// paed — el interprete de PAED en la terminal.
//
//   paed programa.paed          corre un programa
//   paed install [destino]      se instala solo, copiandose a si mismo
//   paed uninstall [destino]    se borra
//   paed --version              que version es
//
// El binario se basta solo: lleva la definicion del lenguaje adentro, asi que
// se puede bajar suelto y funciona sin nada al lado. Por eso `install` no
// necesita un paquete: se copia el ejecutable y escribe el sintaxis.json que ya
// tiene puesto.
//
// Codigos de salida, para poder encadenarlo con && en la shell:
//   0  el programa corrio entero
//   1  error de parseo o de ejecucion
//   2  mal uso
//   3  no se pudo cargar la definicion del lenguaje
//   4  fallo la instalacion

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

// ── paed install ────────────────────────────────────────────────────────────

static int copiar(const char *desde, const char *hasta, int modo) {
    FILE *o = fopen(desde, "rb");
    if (!o) { fprintf(stderr, "paed: no puedo leer %s: %s\n", desde, strerror(errno)); return -1; }

    // Se borra primero: si el destino es un binario EN USO, escribirle encima da
    // "Text file busy". Borrarlo y crearlo de nuevo no molesta a quien lo este
    // ejecutando, porque el proceso viejo sigue con el archivo que ya abrio.
    unlink(hasta);

    FILE *d = fopen(hasta, "wb");
    if (!d) {
        fprintf(stderr, "paed: no puedo escribir %s: %s\n", hasta, strerror(errno));
        fclose(o);
        return -1;
    }

    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), o)) > 0) {
        if (fwrite(buf, 1, n, d) != n) {
            fprintf(stderr, "paed: se corto la copia de %s\n", hasta);
            fclose(o); fclose(d);
            return -1;
        }
    }
    fclose(o);
    fclose(d);
    chmod(hasta, (mode_t)modo);
    return 0;
}

static int se_puede_escribir(const char *dir) {
    return access(dir, W_OK) == 0;
}

// Se copia a si mismo y escribe la definicion del lenguaje que lleva adentro.
// No necesita ningun paquete ni ningun archivo al lado: por eso alcanza con
// bajar el binario suelto y correr `paed install`.
static int instalar(const char *destino) {
    char prefix[512];

    if (destino) {
        snprintf(prefix, sizeof(prefix), "%s", destino);
    } else if (se_puede_escribir("/usr/local/bin")) {
        // Se mira si SE PUEDE ESCRIBIR, no si el usuario "es" root: en un
        // contenedor sos root sin sudo, y con `sudo -E` sos root sin serlo.
        snprintf(prefix, sizeof(prefix), "/usr/local");
    } else {
        const char *home = getenv("HOME");
        if (!home) { fprintf(stderr, "paed: no se donde instalar (no hay $HOME)\n"); return 4; }
        snprintf(prefix, sizeof(prefix), "%s/.local", home);
    }

    char bindir[600], datadir[600], bin[700], json[700];
    snprintf(bindir,  sizeof(bindir),  "%s/bin", prefix);
    snprintf(datadir, sizeof(datadir), "%s/share/paed", prefix);
    snprintf(bin,     sizeof(bin),     "%s/paed", bindir);
    snprintf(json,    sizeof(json),    "%s/sintaxis.json", datadir);

    // mkdir -p a mano, un nivel por vez. Los errores distintos de "ya existe"
    // se ignoran aca y los caza el fopen de abajo, con un mensaje que dice cual
    // fue el archivo.
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s/share", prefix);
    mkdir(prefix, 0755); mkdir(bindir, 0755); mkdir(tmp, 0755); mkdir(datadir, 0755);

    char yo[512];
    ssize_t largo = readlink("/proc/self/exe", yo, sizeof(yo) - 1);
    if (largo <= 0) { fprintf(stderr, "paed: no puedo saber donde estoy\n"); return 4; }
    yo[largo] = '\0';

    printf("Instalando PAED %s en %s\n", PAED_VERSION, prefix);

    if (copiar(yo, bin, 0755) != 0) return 4;

    FILE *f = fopen(json, "w");
    if (!f) {
        fprintf(stderr, "paed: no puedo escribir %s: %s\n", json, strerror(errno));
        return 4;
    }
    fputs(paed_sintaxis_embebida(), f);
    fclose(f);

    printf("  %s\n  %s\n\n", bin, json);

    // Instalar y que el comando no exista es la forma mas comun de que esto
    // parezca roto. Se avisa ANTES de que pase, con la linea para arreglarlo.
    const char *ruta = getenv("PATH");
    if (ruta && strstr(ruta, bindir)) {
        printf("Listo. Probalo:\n    paed tu_programa.paed\n");
    } else {
        printf("OJO: %s no esta en tu PATH, asi que el comando 'paed' todavia no existe.\n", bindir);
        printf("Agregalo una sola vez:\n\n");
        printf("    echo 'export PATH=\"%s:$PATH\"' >> ~/.bashrc && source ~/.bashrc\n\n", bindir);
        printf("Mientras tanto anda por ruta completa:  %s tu_programa.paed\n", bin);
    }
    return 0;
}

// Saca el prefijo de instalacion a partir de donde esta ESTE ejecutable:
// de  <prefijo>/bin/paed  devuelve  <prefijo>.
//
// Es lo que permite que `paed uninstall` a secas funcione sin decirle donde:
// el binario que corre ES el instalado, asi que sabe de donde sacarse.
// Devuelve 0 si pudo, -1 si no.
static int prefijo_de_este_binario(char *out, size_t n) {
    char exe[512];
    ssize_t largo = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (largo <= 0) return -1;
    exe[largo] = '\0';

    char *barra = strrchr(exe, '/');      // .../bin/paed -> .../bin
    if (!barra) return -1;
    *barra = '\0';

    char *padre = strrchr(exe, '/');      // .../bin -> ...
    if (!padre) return -1;
    *padre = '\0';

    snprintf(out, n, "%s", exe);
    return 0;
}

// Borra lo que puso `install`, y NADA MAS. Se nombran los dos archivos uno por
// uno en vez de borrar carpetas enteras: un `rm -rf` sobre un prefijo mal
// escrito se lleva puesto lo que haya ahi, y esto corre con permisos de root
// cuando se instalo en /usr/local.
static int desinstalar(const char *destino) {
    char prefix[512];

    if (destino) {
        snprintf(prefix, sizeof(prefix), "%s", destino);
    } else if (prefijo_de_este_binario(prefix, sizeof(prefix)) != 0) {
        fprintf(stderr, "paed: no puedo saber donde estoy instalado, decime el destino:\n");
        fprintf(stderr, "    paed uninstall /usr/local\n");
        return 4;
    }

    char bin[700], json[700], datadir[600];
    snprintf(bin,     sizeof(bin),     "%s/bin/paed", prefix);
    snprintf(datadir, sizeof(datadir), "%s/share/paed", prefix);
    snprintf(json,    sizeof(json),    "%s/sintaxis.json", datadir);

    // Si no esta el binario, ahi no hay una instalacion. Se avisa en vez de
    // borrar a ciegas y decir "listo" sin haber hecho nada.
    if (access(bin, F_OK) != 0) {
        fprintf(stderr, "paed: no encuentro una instalacion en %s\n", prefix);
        fprintf(stderr, "      (buscaba %s)\n", bin);
        return 4;
    }

    printf("Desinstalando PAED de %s\n", prefix);

    // Borrar el binario que se esta ejecutando ES legal en Linux: el proceso
    // sigue con el archivo que ya tiene abierto, y el nombre desaparece.
    if (unlink(bin) != 0) {
        fprintf(stderr, "paed: no puedo borrar %s: %s\n", bin, strerror(errno));
        return 4;
    }
    printf("  borrado %s\n", bin);

    if (unlink(json) == 0) printf("  borrado %s\n", json);

    // rmdir y no rm -rf: solo se va si quedo VACIO. Si el directorio de datos
    // tiene otras librerias (escena.json, por ejemplo), no son de PAED y no se
    // tocan — se avisa que quedaron.
    if (rmdir(datadir) == 0) {
        printf("  borrado %s\n", datadir);
    } else if (access(datadir, F_OK) == 0) {
        printf("\n %s no quedo vacio, asi que no se borro.\n", datadir);
        printf("     Adentro hay librerias que no son del lenguaje. Mirá que son antes de borrarlas.\n");
    }

    printf("\nListo. PAED ya no esta en %s\n", prefix);
    return 0;
}

static void ayuda(void) {
    printf("paed %s — el pseudocodigo AED de la catedra, ejecutable\n\n", PAED_VERSION);
    printf("  paed <archivo.paed>        corre un programa\n");
    printf("  paed install [destino]     se instala (por defecto /usr/local o ~/.local)\n");
    printf("  paed uninstall [destino]   se borra (por defecto de donde se esta corriendo)\n");
    printf("  paed --version             la version\n");
    printf("  paed --help                esto\n\n");
    printf("  --lib <nombre>             carga una libreria que no es del lenguaje\n\n");
}

int main(int argc, char **argv) {
    const char *path = NULL;

    // ── Subcomandos ─────────────────────────────────────────────────────────
    // Van antes que todo lo demas y no pasan por el parseo de argumentos de
    // abajo: `install` no abre ningun .paed, asi que tratarlo como un archivo
    // daria "no se pudo abrir install".
    if (argc >= 2) {
        if (strcmp(argv[1], "install") == 0)
            return instalar(argc >= 3 ? argv[2] : NULL);

        if (strcmp(argv[1], "uninstall") == 0)
            return desinstalar(argc >= 3 ? argv[2] : NULL);

        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0 ||
            strcmp(argv[1], "-version") == 0 || strcmp(argv[1], "version") == 0) {
            printf("paed %s\n", PAED_VERSION);
            return 0;
        }

        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 ||
            strcmp(argv[1], "help") == 0) {
            ayuda();
            return 0;
        }
    }

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

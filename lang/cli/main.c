// paed — el interprete de PAED en la terminal.
//
//   paed programa.paed          corre un programa
//   paed install [destino]      se instala solo: se copia y se pone en el PATH
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

#include <paed/aprender.h>
#include <paed/asistente.h>
#include <paed/datos.h>
#include <paed/errores.h>
#include <paed/colores.h>
#include <paed/parser.h>
#include <paed/interpreter.h>
#include <paed/plataforma.h>
#include <paed/secuencia.h>

// Lo que en Linux vive en unistd.h, en Windows vive en io.h con otro nombre.
#ifdef _WIN32
  #include <io.h>        // _access, _unlink
  #include <direct.h>    // _rmdir
  #define ACCESO(p, m)  _access(p, m)
  #define BORRAR(p)     _unlink(p)
  #define BORRAR_DIR(p) _rmdir(p)
  #define F_OK 0
  #define W_OK 2
#else
  #include <sys/stat.h>
  #include <unistd.h>
  #define ACCESO(p, m)  access(p, m)
  #define BORRAR(p)     unlink(p)
  #define BORRAR_DIR(p) rmdir(p)
#endif

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

// ── De donde salen los datos de una SECUENCIA ───────────────────────────────
//
// Del propio .paed, en un bloque de comentarios al final:
//
//     // ── SECUENCIA secAlu ──
//     // 12345Perez Juan#0503
//     // 67890Gomez Ana#1204
//
// Es la misma decision que la del bloque SALIDA ESPERADA y la del de ENTRADA:
// un programa es UN archivo. Los datos de una secuencia son parte del
// enunciado — sin ellos el algoritmo no significa nada — asi que guardarlos en
// otro lado obliga a mantener dos archivos sincronizados a mano.
//
// El bloque se lee de nuevo por cada secuencia y no una vez para todas: son
// tres o cuatro por programa, el archivo ya esta en el cache del sistema, y la
// alternativa es una tabla que hay que llenar, ordenar y liberar.
//
// Las lineas del bloque se PEGAN sin separador. Asi una secuencia de
// caracteres larga se puede partir en varios renglones para que se lea, sin que
// aparezcan saltos de linea que no estan en los datos.
// De donde salen los datos de una secuencia: de SU ARCHIVO, y de ningun otro
// lado. La ruta la arma la libreria (sec_ruta_datos, en secuencia.h) y el
// editor escribe en esa misma ruta.
//
// Hubo una segunda forma — un bloque de comentarios adentro del .paed — y se
// saco a proposito. Con dos lugares donde puede estar la cinta, el dia que las
// dos digan cosas distintas gana una por un detalle de implementacion y el que
// escribe el programa no tiene forma de saber cual. Un dato, un lugar.
static int datos_de_secuencia(const char *nombre, char *buf, size_t n, void *ud) {
    return sec_leer_datos((const char *)ud, nombre, buf, n);
}

// ── paed install ────────────────────────────────────────────────────────────

static int copiar(const char *desde, const char *hasta, int modo) {
    FILE *o = fopen(desde, "rb");
    if (!o) { fprintf(stderr, "paed: no puedo leer %s: %s\n", desde, strerror(errno)); return -1; }

    // Se borra primero: si el destino es un binario EN USO, escribirle encima da
    // "Text file busy". Borrarlo y crearlo de nuevo no molesta a quien lo este
    // ejecutando, porque el proceso viejo sigue con el archivo que ya abrio.
    BORRAR(hasta);

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

    // En Windows lo que hace ejecutable a un archivo es la extension .exe, no
    // un permiso, asi que no hay nada que marcar.
#ifndef _WIN32
    chmod(hasta, (mode_t)modo);
#else
    (void)modo;
#endif
    return 0;
}

// Solo la usa el camino de Linux, para decidir entre /usr/local y ~/.local.
// En Windows cada usuario tiene su carpeta y no hay nada que decidir.
#ifndef _WIN32
static int se_puede_escribir(const char *dir) {
    return ACCESO(dir, W_OK) == 0;
}
#endif

// ── El PATH ─────────────────────────────────────────────────────────────────
//
// Instalar y que el comando no exista es la forma mas comun de que esto
// parezca roto: el binario esta, pero la shell no lo encuentra. Asi que
// install lo arregla en vez de solo avisar.

#ifndef _WIN32
// Si `dir` es una de las entradas del PATH. Se compara ENTRADA POR ENTRADA y no
// con strstr: "~/.local/bin" aparece como substring dentro de "~/.local/bin2" y
// de cualquier ruta que lo contenga, y entonces install creeria que ya esta
// puesto cuando no lo esta.
static int path_contiene(const char *path, const char *dir) {
    if (!path || !dir || !*dir) return 0;
    size_t largo = strlen(dir);

    for (const char *e = path; *e; ) {
        const char *fin = strchr(e, ':');
        size_t n = fin ? (size_t)(fin - e) : strlen(e);
        if (n == largo && strncmp(e, dir, largo) == 0) return 1;
        if (!fin) break;
        e = fin + 1;
    }
    return 0;
}

// El archivo de configuracion de la shell del usuario, y como se escribe en el
// una linea que agrega al PATH. fish no usa la sintaxis de sh, asi que no
// alcanza con elegir el archivo: cambia tambien lo que se escribe.
//
// Si $SHELL no dice nada conocido se cae a ~/.profile, que lo leen todas las
// shells tipo Bourne al iniciar sesion.
static const char *rc_de_la_shell(const char *home, char *out, size_t n) {
    const char *sh = getenv("SHELL");
    const char *nombre = sh ? sh : "";
    for (const char *c = nombre; *c; c++) if (*c == '/') nombre = c + 1;

    if (strcmp(nombre, "zsh") == 0) {
        snprintf(out, n, "%s/.zshrc", home);
        return "export PATH=\"%s:$PATH\"";
    }
    if (strcmp(nombre, "fish") == 0) {
        snprintf(out, n, "%s/.config/fish/config.fish", home);
        return "set -gx PATH %s $PATH";
    }
    if (strcmp(nombre, "bash") == 0) {
        snprintf(out, n, "%s/.bashrc", home);
        return "export PATH=\"%s:$PATH\"";
    }
    snprintf(out, n, "%s/.profile", home);
    return "export PATH=\"%s:$PATH\"";
}

// Agrega bindir al PATH escribiendo en el rc de la shell.
//
// Devuelve 1 si escribio, 0 si ya estaba, -1 si no pudo. Que "ya estaba" no sea
// un error es lo que hace que install se pueda correr dos veces sin ensuciar el
// archivo: si el usuario reinstala, no le quedan cinco lineas iguales.
static int agregar_al_path(const char *bindir, char *rc, size_t nrc) {
    const char *home = getenv("HOME");
    if (!home) return -1;

    const char *formato = rc_de_la_shell(home, rc, nrc);

    // Ya escrita en una corrida anterior. Se busca el bindir y no la linea
    // entera: si el usuario la edito a mano, igual esta puesto.
    FILE *f = fopen(rc, "r");
    if (f) {
        char linea[1024];
        while (fgets(linea, sizeof(linea), f)) {
            if (linea[0] == '#') continue;
            if (strstr(linea, bindir)) { fclose(f); return 0; }
        }
        fclose(f);
    }

    // Las carpetas intermedias, si faltan. fish guarda su config en
    // ~/.config/fish/config.fish, y en una maquina recien instalada esa carpeta
    // no existe: fopen en modo append NO la crea, y la instalacion terminaba
    // diciendo que no pudo escribir sin que hubiera nada roto.
    for (char *c = rc + 1; *c; c++) {
        if (*c != '/') continue;
        *c = '\0';
        paed_mkdir(rc);
        *c = '/';
    }

    f = fopen(rc, "a");
    if (!f) return -1;
    fprintf(f, "\n# PAED\n");
    fprintf(f, formato, bindir);
    fprintf(f, "\n");
    fclose(f);
    return 1;
}
#endif

// Se copia a si mismo y escribe la definicion del lenguaje que lleva adentro.
// No necesita ningun paquete ni ningun archivo al lado: por eso alcanza con
// bajar el binario suelto y correr `paed install`.
static int instalar(const char *destino) {
    char prefix[512];

    if (destino) {
        snprintf(prefix, sizeof(prefix), "%s", destino);
    } else {
#ifdef _WIN32
        // En Windows no hay /usr/local ni permisos de root: cada usuario tiene
        // su carpeta. LOCALAPPDATA es donde van los programas de un solo
        // usuario, que es lo que corresponde para algo que se instala sin
        // pedirle nada a nadie.
        const char *base = getenv("LOCALAPPDATA");
        if (!base) base = getenv("USERPROFILE");
        if (!base) { fprintf(stderr, "paed: no se donde instalar (no hay LOCALAPPDATA ni USERPROFILE)\n"); return 4; }
        snprintf(prefix, sizeof(prefix), "%s\\paed", base);
#else
        if (se_puede_escribir("/usr/local/bin")) {
            // Se mira si SE PUEDE ESCRIBIR, no si el usuario "es" root: en un
            // contenedor sos root sin sudo, y con `sudo -E` sos root sin serlo.
            snprintf(prefix, sizeof(prefix), "/usr/local");
        } else {
            const char *home = getenv("HOME");
            if (!home) { fprintf(stderr, "paed: no se donde instalar (no hay $HOME)\n"); return 4; }
            snprintf(prefix, sizeof(prefix), "%s/.local", home);
        }
#endif
    }

    char bindir[600], datadir[600], bin[700], json[700];
    snprintf(bindir,  sizeof(bindir),  "%s" PAED_SEP "bin", prefix);
    snprintf(datadir, sizeof(datadir), "%s" PAED_SEP "share" PAED_SEP "paed", prefix);
    snprintf(bin,     sizeof(bin),     "%s" PAED_SEP "paed" PAED_EXE, bindir);
    snprintf(json,    sizeof(json),    "%s" PAED_SEP "sintaxis.json", datadir);

    // mkdir -p a mano, un nivel por vez. Los errores distintos de "ya existe"
    // se ignoran aca y los caza el fopen de abajo, con un mensaje que dice cual
    // fue el archivo.
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s" PAED_SEP "share", prefix);
    paed_mkdir(prefix); paed_mkdir(bindir); paed_mkdir(tmp); paed_mkdir(datadir);

    char yo[512];
    if (paed_ruta_ejecutable(yo, sizeof(yo)) != 0) {
        fprintf(stderr, "paed: no puedo saber donde estoy\n");
        return 4;
    }

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

    const char *ruta = getenv("PATH");
#ifdef _WIN32
    // En Windows el PATH del usuario vive en el registro, no en un archivo de
    // texto: escribirlo desde aca es meterse con la configuracion del sistema.
    // Se deja el comando, que es una linea.
    if (ruta && strstr(ruta, bindir)) {
        printf("Listo. Probalo:\n    paed tu_programa.paed\n");
    } else {
        printf("%s no esta en tu PATH, asi que el comando 'paed' todavia no existe.\n", bindir);
        printf("Agregalo una sola vez:\n\n");
        // setx escribe el PATH del usuario en el registro, y recien lo ve una
        // consola NUEVA: la que corre este comando sigue con el viejo.
        printf("    setx PATH \"%%PATH%%;%s\"\n\n", bindir);
        printf("(y despues abri una consola nueva)\n\n");
        printf("Mientras tanto anda por ruta completa:  %s tu_programa.paed\n", bin);
    }
#else
    if (path_contiene(ruta, bindir)) {
        printf("Listo. Probalo:\n    paed tu_programa.paed\n");
        return 0;
    }

    char rc[700];
    int puesto = agregar_al_path(bindir, rc, sizeof(rc));

    if (puesto == 1) {
        printf("%s no estaba en el PATH: agregado a %s\n\n", bindir, rc);
        printf("Abri una terminal nueva (o corre 'source %s') y probalo:\n", rc);
        printf("    paed --version\n");
    } else if (puesto == 0) {
        // Esta en el rc pero no en el PATH de ESTA shell: el archivo se lee al
        // abrir la terminal, y esta ya estaba abierta cuando se escribio.
        printf("%s ya figura en %s, pero esta terminal todavia no lo tomo.\n\n", bindir, rc);
        printf("Abri una terminal nueva (o corre 'source %s').\n", rc);
    } else {
        printf("%s no esta en tu PATH y no pude escribir tu archivo de shell.\n", bindir);
        printf("Agregalo a mano, una sola vez:\n\n");
        printf("    echo 'export PATH=\"%s:$PATH\"' >> ~/.bashrc && source ~/.bashrc\n\n", bindir);
        printf("Mientras tanto anda por ruta completa:  %s tu_programa.paed\n", bin);
    }
#endif
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
    if (paed_ruta_ejecutable(exe, sizeof(exe)) != 0) return -1;

    if (paed_dirname(exe) != 0) return -1;   // .../bin/paed -> .../bin
    if (paed_dirname(exe) != 0) return -1;   // .../bin      -> ...

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
    snprintf(bin,     sizeof(bin),     "%s" PAED_SEP "bin" PAED_SEP "paed" PAED_EXE, prefix);
    snprintf(datadir, sizeof(datadir), "%s" PAED_SEP "share" PAED_SEP "paed", prefix);
    snprintf(json,    sizeof(json),    "%s" PAED_SEP "sintaxis.json", datadir);

    // Si no esta el binario, ahi no hay una instalacion. Se avisa en vez de
    // borrar a ciegas y decir "listo" sin haber hecho nada.
    if (ACCESO(bin, F_OK) != 0) {
        fprintf(stderr, "paed: no encuentro una instalacion en %s\n", prefix);
        fprintf(stderr, "      (buscaba %s)\n", bin);
        return 4;
    }

    printf("Desinstalando PAED de %s\n", prefix);

    // Borrar el binario que se esta ejecutando ES legal en Linux: el proceso
    // sigue con el archivo que ya tiene abierto, y el nombre desaparece.
    if (BORRAR(bin) != 0) {
        fprintf(stderr, "paed: no puedo borrar %s: %s\n", bin, strerror(errno));
        return 4;
    }
    printf("  borrado %s\n", bin);

    if (BORRAR(json) == 0) printf("  borrado %s\n", json);

    // rmdir y no rm -rf: solo se va si quedo VACIO. Si el directorio de datos
    // tiene otras librerias (escena.json, por ejemplo), no son de PAED y no se
    // tocan — se avisa que quedaron.
    if (BORRAR_DIR(datadir) == 0) {
        printf("  borrado %s\n", datadir);
    } else if (ACCESO(datadir, F_OK) == 0) {
        printf("\n %s no quedo vacio, asi que no se borro.\n", datadir);
        printf("     Adentro hay librerias que no son del lenguaje. Mirá que son antes de borrarlas.\n");
    }

    printf("\nListo. PAED ya no esta en %s\n", prefix);
    return 0;
}

static void ayuda(void) {
    printf("paed %s — el pseudocodigo AED de la catedra, ejecutable\n\n", PAED_VERSION);
    printf("  paed <archivo.paed>        corre un programa\n");
    printf("  paed aprender              el tutorial: ejercicios rotos, de menos a mas\n");
    printf("  paed asistente <archivo>   el menu de archivos: que tipo es cada uno\n");
    printf("  paed datos <archivo>       carga las filas del .csv, ordenadas por su clave\n");
    printf("  paed errores               los codigos de error y que significa cada uno\n");
    printf("  paed install [destino]     se instala y se agrega al PATH (por defecto /usr/local o ~/.local)\n");
    printf("  paed uninstall [destino]   se borra (por defecto de donde se esta corriendo)\n");
    printf("  paed --version             la version\n");
    printf("  paed --help                esto\n\n");
    printf("  --lib <nombre>             carga una libreria que no es del lenguaje\n");
    printf("  --colores <archivo>        muestra el programa coloreado en la terminal\n");
    printf("  --tokens <archivo>         un token por linea: linea col largo rol texto\n");
    printf("  --secuencias <archivo>     las secuencias de entrada: nombre tipo hay|falta\n\n");
}

// ── Que hacer con el archivo ────────────────────────────────────────────────
typedef enum {
    MODO_EJECUTAR = 0,
    MODO_COLORES,     // --colores: para MIRARLO, con ANSI
    MODO_TOKENS,      // --tokens:  para que lo lea otro programa (editorBim)
    MODO_SECUENCIAS,  // --secuencias: que secuencias hay y cuales sin datos
} Modo;

// Donde va escribiendo el pintor. El resaltador entrega TOKENS y no el archivo
// entero, asi que los espacios entre token y token los repone aca, mirando la
// columna del proximo. Es lo mismo que hace un editor: el color va sobre el
// texto, y los huecos no llevan color porque no son nada.
typedef struct { int linea; int col; } Pintor;

static void pintar_ansi(const PAEDToken *tok, void *ud) {
    Pintor *p = (Pintor *)ud;

    while (p->linea < tok->linea) { putchar('\n'); p->linea++; p->col = 1; }
    while (p->col   < tok->col)   { putchar(' ');  p->col++; }

    const char *ansi = paed_rol_ansi(tok->rol);
    // Los roles sin color (la puntuacion) salen tal cual, sin ensuciar la
    // salida con un reset que no apaga nada.
    if (*ansi) printf("%s%s%s", ansi, tok->texto, paed_ansi_reset());
    else       printf("%s", tok->texto);

    p->col += tok->largo;
}

// Una linea por token, separado por tabs. Es la forma en que editorBim va a
// consumir esto: sin ANSI, sin adornos, y con linea/columna/largo para poder
// pintar el buffer sin volver a leer el archivo.
static void imprimir_token(const PAEDToken *tok, void *ud) {
    (void)ud;
    printf("%d\t%d\t%d\t%s\t%s\n", tok->linea, tok->col, tok->largo, tok->rol, tok->texto);
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

        // El tutorial. Se le pasa argv desde 'aprender' en adelante, asi que
        // adentro argv[0] es "aprender" y argv[1] su subcomando.
        if (strcmp(argv[1], "aprender") == 0)
            return paed_aprender(argc - 1, argv + 1);

        // El asistente de archivos. Mismo trato que 'aprender': no abre un
        // .paed para ejecutarlo, asi que no pasa por el parseo de argumentos.
        if (strcmp(argv[1], "asistente") == 0)
            return paed_asistente(argc - 1, argv + 1);

        // El cargador de datos. Lee el AMBIENTE del .paed para saber que
        // columnas y que clave tiene cada archivo, y escribe el .csv ordenado
        // por esa clave. Va antes de escribir el PROCESO, no despues.
        if (strcmp(argv[1], "datos") == 0)
            return paed_datos(argc - 1, argv + 1);

        // El catalogo de errores. Lo mismo que esta en errores.md, para poder
        // mirarlo sin salir de la terminal.
        if (strcmp(argv[1], "errores") == 0) {
            printf("\n  Codigos de error de PAED\n\n");
            for (int i = 0; i < paed_codigos_count(); i++)
                printf("    %-8s %s\n", paed_codigo_nombre(i), paed_codigo_titulo(i));
            printf("\n  Cada uno esta explicado en xasolError/XLerror-NN.md,\n");
            printf("  con el indice en errores.md.\n\n");
            return 0;
        }

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
    const char *lib  = NULL;
    Modo        modo = MODO_EJECUTAR;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lib") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "paed: --lib necesita un nombre (ej: --lib escena)\n");
                return 2;
            }
            lib = argv[++i];
        }
        else if (strcmp(argv[i], "--colores") == 0) modo = MODO_COLORES;
        else if (strcmp(argv[i], "--tokens")  == 0) modo = MODO_TOKENS;
        else if (strcmp(argv[i], "--secuencias") == 0) modo = MODO_SECUENCIAS;
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
    int parse_rc = paed_parse_file(path, &prog);

    // ── Resaltado ───────────────────────────────────────────────────────────
    // Va ANTES de mirar parse_rc, y a proposito: un archivo con errores tiene
    // que colorearse igual — es cuando mas se lo necesita. El parser junta los
    // errores y sigue leyendo, asi que el AMBIENTE ya esta cargado y los
    // archivos y los tipos del programador se reconocen lo mismo.
    // ── Que secuencias hay, y cuales todavia no tienen cinta ────────────────
    //
    // Lo pregunta el editor ANTES de correr, para poder pedirte los datos que
    // faltan. Va antes de mirar parse_rc por lo mismo que el resaltado: un
    // programa a medio escribir ya declara sus secuencias, y es justo cuando
    // hace falta saberlo.
    //
    // Una linea por secuencia de ENTRADA: nombre, tipo base, y si ya tiene
    // datos o no. Las de SALIDA no salen: esas las escribe el programa.
    if (modo == MODO_SECUENCIAS) {
        for (int i = 0; i < prog.decl_count; i++) {
            const PAEDDecl *d = &prog.decls[i];
            if (!d->es_secuencia || d->es_salida) continue;

            char datos[PAED_SEC_MAX];
            int hay = datos_de_secuencia(d->name, datos, sizeof(datos),
                                         (void *)path) == 0;

            char ruta[600];
            sec_ruta_datos(path, d->name, ruta, sizeof(ruta));
            printf("%s\t%s\t%s\t%s\n", d->name, d->type,
                   hay ? "hay" : "falta", ruta);
        }
        paed_syntax_free();
        return 0;
    }

    if (modo == MODO_COLORES || modo == MODO_TOKENS) {
        Pintor pintor = { .linea = 1, .col = 1 };
        paed_colorear_archivo(path, &prog,
                              modo == MODO_COLORES ? pintar_ansi : imprimir_token,
                              &pintor);
        if (modo == MODO_COLORES) printf("%s\n", paed_ansi_reset());
        paed_syntax_free();
        return 0;
    }

    if (parse_rc != 0) {
        paed_print_errors(&prog);
        paed_syntax_free();
        return 1;
    }

    interp_set_entrada(leer_de_stdin, NULL);
    // Los datos de las secuencias salen del mismo .paed, asi que el host le
    // pasa la ruta y la funcion los busca ahi.
    interp_set_secuencia(datos_de_secuencia, (void *)path);
    int rc = interp_exec(&prog);

    paed_syntax_free();
    return rc == 0 ? 0 : 1;
}

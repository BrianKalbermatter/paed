#include "paed/aprender.h"
#include "paed/plataforma.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// popen se llama distinto en Windows, y ahi el shell es cmd.exe en vez de sh.
// Las dos cosas que necesitamos del shell — redirigir stdin con '<' y juntar
// stderr con stdout con '2>&1' — las entienden los dos igual.
#ifdef _WIN32
  #include <io.h>
  #define ABRIR_CANIO(c)  _popen((c), "r")
  #define CERRAR_CANIO(f) _pclose(f)
  #define ACCESO(p)       (_access((p), 0) == 0)
#else
  #include <unistd.h>
  #define ABRIR_CANIO(c)  popen((c), "r")
  #define CERRAR_CANIO(f) pclose(f)
  #define ACCESO(p)       (access((p), F_OK) == 0)
#endif

#define DIR_POR_DEFECTO "paed-aprender"
#define MAX_SALIDA      65536
#define MAX_LINEA       1024
#define MAX_RUTA        1024

// El prefijo con el que arranca TODA marca de bloque: '// ── '. Las rayas son
// U+2500 en UTF-8 (e2 94 80), y van escritas por bytes a proposito: escribirlas
// como caracteres literales en el .c depende de con que codificacion se guardo
// el archivo fuente, y eso es justo lo que no queremos que decida si el
// tutorial encuentra sus bloques o no.
#define MARCA_PRE "// \xe2\x94\x80\xe2\x94\x80 "

// ── Leer un bloque de comentarios del .paed ─────────────────────────────────
//
// Los ejercicios llevan sus datos y su respuesta adentro, en bloques marcados:
//
//     // ── PISTA ──
//     // ── ENTRADA ──
//     // ── SALIDA ESPERADA ──
//
// Es la misma convencion que tests/correr.sh, y no es casualidad: un ejercicio
// ES un test, con la unica diferencia de que viene roto a proposito. Si el
// tutorial usara otro formato habria dos maneras de declarar lo mismo, y la
// segunda envejeceria sin que nadie se entere.
//
// El bloque va desde su marca hasta la proxima marca (o hasta el final). Deja
// el contenido en `buf`, ya sin el '// ' de cada linea.
// Devuelve 0 si encontro el bloque, -1 si no esta.

static int es_marca(const char *linea) {
    return strncmp(linea, MARCA_PRE, strlen(MARCA_PRE)) == 0;
}

static int bloque_de(const char *ruta, const char *marca, char *buf, size_t n,
                     int saltar_vacias) {
    FILE *f = fopen(ruta, "r");
    if (!f) return -1;

    char linea[MAX_LINEA];
    int    dentro = 0;
    size_t usado  = 0;
    buf[0] = '\0';

    while (fgets(linea, sizeof(linea), f)) {
        if (es_marca(linea)) {
            if (dentro) break;   // empezo otro bloque: el nuestro termino
            // Se compara el nombre despues del prefijo, no en cualquier lado
            // de la linea: asi 'SALIDA ESPERADA' no matchea con un comentario
            // cualquiera que mencione esas palabras.
            if (strncmp(linea + strlen(MARCA_PRE), marca, strlen(marca)) == 0)
                dentro = 1;
            continue;
        }
        if (!dentro) continue;

        linea[strcspn(linea, "\r\n")] = '\0';

        // Una linea sin '//' no es parte del bloque. En la practica es la linea
        // en blanco que separa un bloque del siguiente.
        if (strncmp(linea, "//", 2) != 0) {
            if (linea[0] == '\0') continue;
            break;
        }

        const char *cuerpo = linea + 2;
        if (*cuerpo == ' ') cuerpo++;   // el espacio de cortesia despues del //

        // La ENTRADA descarta renglones vacios: un LEER trata la linea vacia
        // como un dato que falta, y un renglon de aire no deberia romper nada.
        // La SALIDA ESPERADA NO los descarta, porque ahi una linea vacia puede
        // ser parte legitima de lo que el programa imprime.
        if (saltar_vacias && cuerpo[0] == '\0') continue;

        size_t largo = strlen(cuerpo);
        if (usado + largo + 2 >= n) break;
        memcpy(buf + usado, cuerpo, largo);
        usado += largo;
        buf[usado++] = '\n';
        buf[usado]   = '\0';
    }

    fclose(f);
    return dentro ? 0 : -1;
}

// Saca los '\n' (y los '\r') que quedan colgando al final. Es lo que hace la
// shell con $(...), y hay que hacer lo mismo para que un salto de linea de mas
// al final del archivo no cuente como una diferencia.
static void recortar_final(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = '\0';
}

// Los '\r' del medio se van tambien: en Windows el programa hijo puede
// terminar las lineas con CRLF y el bloque del .paed nunca los tiene.
static void sacar_cr(char *s) {
    char *w = s;
    for (char *r = s; *r; r++)
        if (*r != '\r') *w++ = *r;
    *w = '\0';
}

// ── Correr un ejercicio ─────────────────────────────────────────────────────
//
// Se lanza como SUBPROCESO, con el mismo binario que esta corriendo esto. Ver
// aprender.h para el porque; lo importante en la practica es que el comando que
// se arma aca es, letra por letra, el que el alumno tipearia a mano.

#ifndef _WIN32
// `timeout` es de coreutils y en Linux esta siempre, pero no es del sistema
// base en todos lados. Se pregunta UNA vez y se guarda: preguntarlo por cada
// ejercicio serian doce procesos extra por corrida.
//
// Sin el, un ejercicio con un ciclo infinito — que el alumno VA a escribir —
// cuelga el tutorial entero y solo se sale con Ctrl-C.
static int hay_timeout(void) {
    static int cache = -1;
    if (cache < 0) cache = (system("timeout --version > /dev/null 2>&1") == 0);
    return cache;
}
#endif

static int correr(const char *ruta, char *salida, size_t n) {
    char yo[MAX_RUTA];
    if (paed_ruta_ejecutable(yo, sizeof(yo)) != 0) {
        fprintf(stderr, "paed: no puedo saber donde estoy para correr el ejercicio\n");
        return -1;
    }

    // La ENTRADA va a un archivo al lado del ejercicio y se le redirige al
    // hijo. El archivo se escribe SIEMPRE, aunque el bloque no exista: asi un
    // programa que pide datos que no estan falla con "la entrada se termino" en
    // vez de quedarse esperando a que alguien tipee.
    char entrada[MAX_SALIDA];
    if (bloque_de(ruta, "ENTRADA", entrada, sizeof(entrada), 1) != 0)
        entrada[0] = '\0';

    char tmp[MAX_RUTA];
    snprintf(tmp, sizeof(tmp), "%s.entrada", ruta);

    FILE *e = fopen(tmp, "w");
    if (!e) {
        fprintf(stderr, "paed: no puedo escribir %s\n", tmp);
        return -1;
    }
    fputs(entrada, e);
    fclose(e);

    char cmd[MAX_RUTA * 3];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" < \"%s\" 2>&1", yo, ruta, tmp);
#else
    snprintf(cmd, sizeof(cmd), "%s\"%s\" \"%s\" < \"%s\" 2>&1",
             hay_timeout() ? "timeout 10 " : "", yo, ruta, tmp);
#endif

    FILE *p = ABRIR_CANIO(cmd);
    if (!p) {
        fprintf(stderr, "paed: no pude lanzar el ejercicio\n");
        remove(tmp);
        return -1;
    }

    size_t usado = 0;
    size_t leido;
    while ((leido = fread(salida + usado, 1, n - usado - 1, p)) > 0) {
        usado += leido;
        if (usado >= n - 1) break;
    }
    salida[usado] = '\0';

    int rc = CERRAR_CANIO(p);
    remove(tmp);

    sacar_cr(salida);
    recortar_final(salida);
    return rc;
}

// ── Comparar ────────────────────────────────────────────────────────────────
//
// Devuelve 0 si la salida es la esperada. Si no, devuelve el numero de la
// PRIMERA linea que difiere (1 en adelante).
//
// Se informa la primera diferencia y no un diff entero a proposito: para el que
// esta aprendiendo, "la linea 3 tenia que decir esto y dice esto otro" es
// accionable, y veinte lineas de diff unificado son ruido.
static int primera_diferencia(const char *real, const char *esperado) {
    int linea = 1;
    const char *a = real, *b = esperado;

    for (;;) {
        const char *fa = strchr(a, '\n');
        const char *fb = strchr(b, '\n');
        size_t la = fa ? (size_t)(fa - a) : strlen(a);
        size_t lb = fb ? (size_t)(fb - b) : strlen(b);

        if (la != lb || memcmp(a, b, la) != 0) return linea;
        if (!fa && !fb) return 0;          // se terminaron las dos, iguales
        if (!fa || !fb) return linea + 1;  // a una le sobran lineas

        a = fa + 1;
        b = fb + 1;
        linea++;
    }
}

static void imprimir_con_numeros(const char *texto, const char *sangria) {
    if (!*texto) {
        printf("%s(no imprimio nada)\n", sangria);
        return;
    }
    int n = 1;
    const char *p = texto;
    while (*p) {
        const char *fin = strchr(p, '\n');
        size_t largo = fin ? (size_t)(fin - p) : strlen(p);
        printf("%s%2d | %.*s\n", sangria, n, (int)largo, p);
        if (!fin) break;
        p = fin + 1;
        n++;
    }
}

// ── La carpeta de trabajo ───────────────────────────────────────────────────

// Donde estan los .paed del alumno. En orden:
//   1. el argumento, si lo dio
//   2. $PAED_APRENDER
//   3. el directorio actual, si el primer ejercicio esta ahi (estas parado
//      adentro de la carpeta)
//   4. ./paed-aprender
static void elegir_dir(const char *arg, char *out, size_t n) {
    if (arg)                       { snprintf(out, n, "%s", arg); return; }

    const char *env = getenv("PAED_APRENDER");
    if (env && *env)               { snprintf(out, n, "%s", env); return; }

    if (PAED_EJERCICIOS_N > 0 && ACCESO(PAED_EJERCICIOS[0].nombre)) {
        snprintf(out, n, ".");
        return;
    }
    snprintf(out, n, "%s", DIR_POR_DEFECTO);
}

static void ruta_de(const char *dir, const char *nombre, char *out, size_t n) {
    snprintf(out, n, "%s" PAED_SEP "%s", dir, nombre);
}

// Cual es el ejercicio actual: el PRIMERO que todavia no pasa.
//
// No hay archivo de progreso — se calcula. Ver el comentario grande de
// aprender.h: un estado que se deriva del disco no puede contradecir al disco.
//
// Devuelve el indice, o PAED_EJERCICIOS_N si ya pasaron todos, o -1 si falta
// algun archivo.
static int actual(const char *dir) {
    char ruta[MAX_RUTA], salida[MAX_SALIDA], esperado[MAX_SALIDA];

    for (int i = 0; i < PAED_EJERCICIOS_N; i++) {
        ruta_de(dir, PAED_EJERCICIOS[i].nombre, ruta, sizeof(ruta));

        if (!ACCESO(ruta)) {
            fprintf(stderr, "paed: falta %s\n", ruta);
            fprintf(stderr, "      empeza con:  paed aprender init\n");
            return -1;
        }

        if (bloque_de(ruta, "SALIDA ESPERADA", esperado, sizeof(esperado), 0) != 0) {
            fprintf(stderr, "paed: %s no declara su salida esperada\n", ruta);
            return -1;
        }
        recortar_final(esperado);

        if (correr(ruta, salida, sizeof(salida)) < 0) return -1;
        if (primera_diferencia(salida, esperado) != 0) return i;
    }
    return PAED_EJERCICIOS_N;
}

// ── Los subcomandos ─────────────────────────────────────────────────────────

static int cmd_init(const char *dir) {
    paed_mkdir(dir);

    int escritos = 0, salteados = 0;

    for (int i = 0; i < PAED_EJERCICIOS_N; i++) {
        char ruta[MAX_RUTA];
        ruta_de(dir, PAED_EJERCICIOS[i].nombre, ruta, sizeof(ruta));

        // Si ya esta, NO se pisa. `init` se corre de nuevo despues de bajar una
        // version nueva del binario, y pisar ahi seria borrar sin avisar todo
        // lo que el alumno resolvio. Para volver un ejercicio a cero esta
        // `reset`, que pregunta antes.
        if (ACCESO(ruta)) { salteados++; continue; }

        FILE *f = fopen(ruta, "w");
        if (!f) {
            fprintf(stderr, "paed: no puedo escribir %s\n", ruta);
            return 4;
        }
        fputs(PAED_EJERCICIOS[i].contenido, f);
        fclose(f);
        escritos++;
    }

    printf("Ejercicios en %s" PAED_SEP "\n", dir);
    printf("  %d nuevos", escritos);
    if (salteados) printf(", %d que ya estaban (no se tocaron)", salteados);
    printf("\n\n");
    printf("Arranca con:\n");
    printf("    paed aprender\n");
    return 0;
}

static void mostrar_ejercicio(const char *dir, int i) {
    char ruta[MAX_RUTA], salida[MAX_SALIDA], esperado[MAX_SALIDA];
    ruta_de(dir, PAED_EJERCICIOS[i].nombre, ruta, sizeof(ruta));

    bloque_de(ruta, "SALIDA ESPERADA", esperado, sizeof(esperado), 0);
    recortar_final(esperado);
    correr(ruta, salida, sizeof(salida));

    printf("\nEjercicio %d de %d — %s\n", i + 1, PAED_EJERCICIOS_N,
           PAED_EJERCICIOS[i].nombre);
    printf("Editalo:  %s\n\n", ruta);

    int d = primera_diferencia(salida, esperado);
    if (d == 0) { printf("  pasa\n"); return; }

    printf("  La salida no es la que el ejercicio pide (primera diferencia en la linea %d).\n\n", d);
    printf("  esperaba:\n");
    imprimir_con_numeros(esperado, "    ");
    printf("\n  obtuvo:\n");
    imprimir_con_numeros(salida, "    ");
    printf("\n  Si te trabas:  paed aprender pista\n");
}

static int cmd_estado(const char *dir) {
    int i = actual(dir);
    if (i < 0) return 4;

    if (i >= PAED_EJERCICIOS_N) {
        printf("\nTerminaste los %d ejercicios.\n\n", PAED_EJERCICIOS_N);
        printf("El paso siguiente es escribir uno tuyo desde cero: un .paed que\n");
        printf("resuelva algo que te haga falta, no un ejercicio.\n");
        return 0;
    }
    mostrar_ejercicio(dir, i);
    return 1;
}

static int cmd_lista(const char *dir) {
    char ruta[MAX_RUTA], salida[MAX_SALIDA], esperado[MAX_SALIDA];
    int hechos = 0, faltan = 0;

    printf("\n");
    for (int i = 0; i < PAED_EJERCICIOS_N; i++) {
        ruta_de(dir, PAED_EJERCICIOS[i].nombre, ruta, sizeof(ruta));

        if (!ACCESO(ruta)) {
            printf("  ?  %s  (falta)\n", PAED_EJERCICIOS[i].nombre);
            faltan++;
            continue;
        }
        bloque_de(ruta, "SALIDA ESPERADA", esperado, sizeof(esperado), 0);
        recortar_final(esperado);
        correr(ruta, salida, sizeof(salida));

        if (primera_diferencia(salida, esperado) == 0) {
            printf("  ok %s\n", PAED_EJERCICIOS[i].nombre);
            hechos++;
        } else {
            printf("  .  %s\n", PAED_EJERCICIOS[i].nombre);
        }
    }
    // Si faltan archivos, el resumen NO puede ser "0 de 12": eso se lee como
    // progreso, y el problema es otro — la carpeta esta vacia o es la
    // equivocada. Un numero que miente sobre la causa manda a buscar el
    // problema al lado que no es.
    if (faltan == PAED_EJERCICIOS_N) {
        printf("\nNo hay ejercicios en %s" PAED_SEP "\n", dir);
        printf("Desempacalos con:  paed aprender init %s\n",
               strcmp(dir, DIR_POR_DEFECTO) == 0 ? "" : dir);
        return 4;
    }
    if (faltan) {
        printf("\n%d de %d (y %d sin desempacar: paed aprender init %s)\n",
               hechos, PAED_EJERCICIOS_N - faltan, faltan,
               strcmp(dir, DIR_POR_DEFECTO) == 0 ? "" : dir);
        return 4;
    }
    printf("\n%d de %d\n", hechos, PAED_EJERCICIOS_N);
    return 0;
}

static int cmd_pista(const char *dir) {
    int i = actual(dir);
    if (i < 0) return 4;
    if (i >= PAED_EJERCICIOS_N) { printf("No queda ningun ejercicio pendiente.\n"); return 0; }

    char ruta[MAX_RUTA], pista[MAX_SALIDA];
    ruta_de(dir, PAED_EJERCICIOS[i].nombre, ruta, sizeof(ruta));

    if (bloque_de(ruta, "PISTA", pista, sizeof(pista), 0) != 0) {
        printf("%s no tiene pista.\n", PAED_EJERCICIOS[i].nombre);
        return 0;
    }
    printf("\nPista — %s\n\n%s\n", PAED_EJERCICIOS[i].nombre, pista);
    return 0;
}

static int cmd_reset(const char *dir) {
    int i = actual(dir);
    if (i < 0) return 4;
    if (i >= PAED_EJERCICIOS_N) { printf("No queda ningun ejercicio pendiente.\n"); return 0; }

    char ruta[MAX_RUTA];
    ruta_de(dir, PAED_EJERCICIOS[i].nombre, ruta, sizeof(ruta));

    // Se pregunta antes, y no es burocracia: esto BORRA lo que el alumno
    // escribio, y lo escrito es justamente el trabajo del tutorial.
    printf("Esto descarta todo lo que escribiste en:\n    %s\n\n", ruta);
    printf("Escribi 'si' para confirmar: ");
    fflush(stdout);

    char resp[32];
    if (!fgets(resp, sizeof(resp), stdin)) { printf("\ncancelado\n"); return 0; }
    resp[strcspn(resp, "\r\n")] = '\0';
    if (strcmp(resp, "si") != 0) { printf("cancelado\n"); return 0; }

    FILE *f = fopen(ruta, "w");
    if (!f) { fprintf(stderr, "paed: no puedo escribir %s\n", ruta); return 4; }
    fputs(PAED_EJERCICIOS[i].contenido, f);
    fclose(f);

    printf("%s volvio a como venia.\n", PAED_EJERCICIOS[i].nombre);
    return 0;
}

// Mira el ejercicio actual y lo vuelve a correr cada vez que lo guardas.
//
// Pregunta por la fecha del archivo en vez de escuchar al sistema de archivos:
// inotify en Linux y ReadDirectoryChangesW en Windows no se parecen en nada, y
// no vale la pena esa complejidad para mirar UN archivo. Medio segundo de
// demora no se nota cuando estas cambiando de la ventana del editor a esta.
static int cmd_mirar(const char *dir) {
    int ultimo_ej = -1;
    long long ultima_fecha = -1;

    printf("Mirando los ejercicios. Guardá el archivo y se vuelve a correr solo.\n");
    printf("Ctrl-C para salir.\n");

    for (;;) {
        int i = actual(dir);
        if (i < 0) return 4;

        if (i >= PAED_EJERCICIOS_N) {
            printf("\nTerminaste los %d ejercicios.\n", PAED_EJERCICIOS_N);
            return 0;
        }

        char ruta[MAX_RUTA];
        ruta_de(dir, PAED_EJERCICIOS[i].nombre, ruta, sizeof(ruta));

        long long fecha = 0;
        paed_mtime(ruta, &fecha);

        if (i != ultimo_ej || fecha != ultima_fecha) {
            // Limpia la pantalla y sube el cursor: sin esto cada corrida deja
            // el intento anterior arriba y a los cinco guardados no se ve nada.
            printf("\033[2J\033[H");
            if (ultimo_ej >= 0 && i != ultimo_ej)
                printf("Pasaste el %s\n", PAED_EJERCICIOS[ultimo_ej].nombre);
            mostrar_ejercicio(dir, i);
            fflush(stdout);
            ultimo_ej    = i;
            ultima_fecha = fecha;
        }
        paed_dormir_ms(500);
    }
}

static void ayuda_aprender(void) {
    printf("paed aprender — el tutorial de PAED: %d ejercicios rotos, de menos a mas\n\n",
           PAED_EJERCICIOS_N);
    printf("  paed aprender init [dir]   desempaca los ejercicios (por defecto ./%s)\n", DIR_POR_DEFECTO);
    printf("  paed aprender              muestra el ejercicio actual y por que no pasa\n");
    printf("  paed aprender --mirar      igual, pero se vuelve a correr cada vez que guardas\n");
    printf("  paed aprender pista        la pista del ejercicio actual\n");
    printf("  paed aprender lista        cuales pasaste y cuales no\n");
    printf("  paed aprender reset        vuelve el ejercicio actual a como venia (pregunta antes)\n\n");
    printf("  El ejercicio actual es el primero que todavia no pasa: no hay archivo\n");
    printf("  de progreso que se pueda desincronizar. Se calcula corriendolos.\n\n");
    printf("  La carpeta sale de, en orden: el argumento, $PAED_APRENDER, el\n");
    printf("  directorio actual si los ejercicios estan ahi, o ./%s\n", DIR_POR_DEFECTO);
}

int paed_aprender(int argc, char **argv) {
    const char *sub = argc >= 2 ? argv[1] : NULL;
    const char *arg = argc >= 3 ? argv[2] : NULL;

    if (PAED_EJERCICIOS_N == 0) {
        fprintf(stderr, "paed: este binario se compilo sin ejercicios\n");
        return 4;
    }

    if (sub && (strcmp(sub, "--help") == 0 || strcmp(sub, "-h") == 0 ||
                strcmp(sub, "help")   == 0 || strcmp(sub, "ayuda") == 0)) {
        ayuda_aprender();
        return 0;
    }

    // `init` es el unico que puede recibir una carpeta que todavia no existe,
    // asi que se resuelve distinto: los demas la BUSCAN, este la CREA.
    if (sub && strcmp(sub, "init") == 0)
        return cmd_init(arg ? arg : DIR_POR_DEFECTO);

    // Un subcomando conocido se lleva la carpeta en argv[2]; cualquier otra
    // palabra ES la carpeta (`paed aprender ~/curso`). La lista se consulta una
    // sola vez para las dos decisiones: si se preguntara por separado, agregar
    // un subcomando y olvidarse de la otra lista lo convertiria en el nombre de
    // una carpeta que no existe, y el error diria cualquier cosa.
    static const char *SUBCOMANDOS[] = { "mirar", "pista", "lista", "reset" };

    int es_sub = 0;
    if (sub && sub[0] == '-') es_sub = 1;
    else if (sub)
        for (size_t i = 0; i < sizeof(SUBCOMANDOS) / sizeof(*SUBCOMANDOS); i++)
            if (strcmp(sub, SUBCOMANDOS[i]) == 0) { es_sub = 1; break; }

    char dir[MAX_RUTA];
    if (!sub)        elegir_dir(NULL, dir, sizeof(dir));
    else if (es_sub) elegir_dir(arg,  dir, sizeof(dir));
    else             elegir_dir(sub,  dir, sizeof(dir));

    if (!sub)                             return cmd_estado(dir);
    if (strcmp(sub, "--mirar") == 0 ||
        strcmp(sub, "mirar")   == 0)      return cmd_mirar(dir);
    if (strcmp(sub, "pista") == 0)        return cmd_pista(dir);
    if (strcmp(sub, "lista") == 0)        return cmd_lista(dir);
    if (strcmp(sub, "reset") == 0)        return cmd_reset(dir);
    if (sub[0] == '-') {
        fprintf(stderr, "paed aprender: no conozco '%s'\n", sub);
        fprintf(stderr, "               probá:  paed aprender --help\n");
        return 2;
    }
    return cmd_estado(dir);
}

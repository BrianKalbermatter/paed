// ============================================================
// paed-lsp — el servidor de lenguaje de PAED.
//
// ── QUE ES UN LANGUAGE SERVER ───────────────────────────────
// Un programa aparte que el editor arranca y con el que HABLA. No dibuja nada
// ni sabe de teclas: solo contesta preguntas sobre el codigo.
//
//   Helix                        paed-lsp
//     |  "abri laberinto.paed, dice esto"  ->  |
//     |                                        | (parsea con parser.c)
//     |  <- "linea 12: falta el FIN_MIENTRAS"  |
//   subraya la linea 12 en rojo
//
// Se hablan por la ENTRADA y SALIDA estandar, con mensajes JSON. El editor no
// sabe nada de PAED; el servidor no sabe nada de Helix. Por eso el mismo
// servidor sirve para VS Code, Neovim o cualquier otro: el protocolo (LSP) es
// el mismo para todos.
//
// ── POR QUE ESTE Y NO OTRO PARSER ───────────────────────────
// Este servidor NO tiene gramatica propia: llama a lang/src/parser.c, el mismo
// que ejecuta los programas. Un error que ves subrayado es EXACTAMENTE el error
// que vas a ver al correr. No hay dos opiniones posibles.
//
// Eso es lo que tree-sitter no puede hacer. tree-sitter pinta por forma, mirando
// una ventanita de texto; parser.c leyo el AMBIENTE entero y SABE. Los dos
// conviven porque hacen cosas distintas — ver docs/LECCIONES.md.
// ============================================================

#include "paed/parser.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ── El sobre de los mensajes ────────────────────────────────
//
// Cada mensaje LSP viene con una cabecera que dice cuanto mide el cuerpo:
//
//     Content-Length: 128\r\n
//     \r\n
//     {"jsonrpc":"2.0", ...}
//
// Hace falta porque stdin es un chorro de bytes sin cortes: sin el largo, el
// servidor no sabria donde termina un mensaje y empieza el siguiente.

// Lee un mensaje entero. Devuelve el cuerpo (hay que free) o NULL si se corto
// la entrada, que es como el editor avisa que se cierra.
static char *leer_mensaje(void)
{
    char linea[512];
    long largo = -1;

    // Cabeceras, hasta la linea vacia.
    for (;;) {
        if (!fgets(linea, sizeof(linea), stdin)) return NULL;
        if (linea[0] == '\r' || linea[0] == '\n') break;   // linea vacia: se acabo
        if (strncasecmp(linea, "Content-Length:", 15) == 0)
            largo = strtol(linea + 15, NULL, 10);
    }

    if (largo <= 0 || largo > 8 * 1024 * 1024) return NULL;

    char *cuerpo = malloc((size_t)largo + 1);
    if (!cuerpo) return NULL;

    // fread en bucle y no de una: en un pipe una lectura puede volver corta
    // aunque falten bytes por llegar, y ahi el JSON sale partido al medio.
    size_t leido = 0;
    while (leido < (size_t)largo) {
        size_t n = fread(cuerpo + leido, 1, (size_t)largo - leido, stdin);
        if (n == 0) { free(cuerpo); return NULL; }
        leido += n;
    }
    cuerpo[largo] = '\0';
    return cuerpo;
}

static void enviar(cJSON *msg)
{
    char *texto = cJSON_PrintUnformatted(msg);
    if (!texto) return;
    printf("Content-Length: %zu\r\n\r\n%s", strlen(texto), texto);
    fflush(stdout);   // sin esto el editor espera para siempre
    free(texto);
}

// ── Diagnosticos ────────────────────────────────────────────

// Parsea el texto y le manda al editor la lista de errores.
//
// Se escribe a un archivo temporal porque paed_parse_file solo lee de disco, y
// el editor manda el texto del BUFFER — que puede tener cambios sin guardar.
// Justamente eso es lo que se quiere revisar: los errores mientras escribis,
// no los de la ultima vez que guardaste.
static void publicar_diagnosticos(const char *uri, const char *texto)
{
    char plantilla[] = "/tmp/paed-lsp-XXXXXX";
    int fd = mkstemp(plantilla);
    if (fd < 0) return;

    if (texto) {
        size_t largo = strlen(texto);
        if (write(fd, texto, largo) != (ssize_t)largo) { close(fd); unlink(plantilla); return; }
    }
    close(fd);

    static PAEDProgram prog;   // static: 'PAEDProgram' es grande y la pila no lo aguanta
    memset(&prog, 0, sizeof(prog));
    paed_parse_file(plantilla, &prog);
    unlink(plantilla);

    cJSON *lista = cJSON_CreateArray();
    for (int i = 0; i < prog.error_count; i++) {
        // PAED cuenta las lineas desde 1 y LSP desde 0. Un error de estos hace
        // que el subrayado aparezca una linea corrida, que es de los bugs mas
        // molestos de encontrar porque "casi" anda.
        int linea = prog.errors[i].line - 1;
        if (linea < 0) linea = 0;

        // PAEDError trae linea pero NO columna, asi que se marca la linea
        // entera. El editor recorta el final al largo real.
        cJSON *rango = cJSON_CreateObject();
        cJSON *ini   = cJSON_CreateObject();
        cJSON *fin   = cJSON_CreateObject();
        cJSON_AddNumberToObject(ini, "line", linea);
        cJSON_AddNumberToObject(ini, "character", 0);
        cJSON_AddNumberToObject(fin, "line", linea);
        cJSON_AddNumberToObject(fin, "character", 4096);
        cJSON_AddItemToObject(rango, "start", ini);
        cJSON_AddItemToObject(rango, "end",   fin);

        cJSON *d = cJSON_CreateObject();
        cJSON_AddItemToObject(d, "range", rango);
        cJSON_AddNumberToObject(d, "severity", 1);            // 1 = Error
        cJSON_AddStringToObject(d, "source",  "paed");
        cJSON_AddStringToObject(d, "message", prog.errors[i].msg);
        cJSON_AddItemToArray(lista, d);
    }

    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "uri", uri);
    cJSON_AddItemToObject(params, "diagnostics", lista);

    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "jsonrpc", "2.0");
    cJSON_AddStringToObject(msg, "method",  "textDocument/publishDiagnostics");
    cJSON_AddItemToObject(msg, "params", params);
    enviar(msg);
    cJSON_Delete(msg);
}

// ── El apreton de manos ─────────────────────────────────────
//
// Lo primero que pregunta todo editor: "¿que sabes hacer?". El servidor
// contesta con sus capacidades y el editor deja de pedirle lo que no tiene.
static void responder_initialize(cJSON *id)
{
    cJSON *sync = cJSON_CreateObject();
    cJSON_AddBoolToObject(sync, "openClose", 1);
    // 1 = FULL: en cada cambio llega el texto entero.
    // La alternativa (2 = incremental) manda solo el pedacito que cambio y hay
    // que ir aplicando parches. Para un lenguaje de archivos chicos como PAED,
    // reparsear entero es instantaneo y no vale la pena el codigo extra.
    cJSON_AddNumberToObject(sync, "change", 1);

    cJSON *caps = cJSON_CreateObject();
    cJSON_AddItemToObject(caps, "textDocumentSync", sync);

    cJSON *info = cJSON_CreateObject();
    cJSON_AddStringToObject(info, "name", "paed-lsp");

    cJSON *res = cJSON_CreateObject();
    cJSON_AddItemToObject(res, "capabilities", caps);
    cJSON_AddItemToObject(res, "serverInfo",   info);

    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "jsonrpc", "2.0");
    cJSON_AddItemToObject(msg, "id", cJSON_Duplicate(id, 1));
    cJSON_AddItemToObject(msg, "result", res);
    enviar(msg);
    cJSON_Delete(msg);
}

// ── El bucle ────────────────────────────────────────────────
int main(void)
{
    // Sin esto, en algunos sistemas la salida se guarda en un buffer y el editor
    // no recibe nada hasta que se llena. Parece que el servidor esta colgado.
    setvbuf(stdout, NULL, _IOFBF, 0);

    // El parser busca sintaxis.json en el directorio de datos. Si no lo
    // encuentra usa la copia embebida, asi que el servidor arranca igual desde
    // donde sea que el editor lo haya lanzado.
    paed_syntax_load();

    char *cuerpo;
    while ((cuerpo = leer_mensaje()) != NULL) {
        cJSON *msg = cJSON_Parse(cuerpo);
        free(cuerpo);
        if (!msg) continue;   // JSON roto: se ignora, no se corta la sesion

        const cJSON *metodo = cJSON_GetObjectItem(msg, "method");
        if (!cJSON_IsString(metodo)) { cJSON_Delete(msg); continue; }
        const char *m = metodo->valuestring;

        cJSON *params = cJSON_GetObjectItem(msg, "params");
        cJSON *doc    = params ? cJSON_GetObjectItem(params, "textDocument") : NULL;
        cJSON *uri    = doc    ? cJSON_GetObjectItem(doc, "uri") : NULL;

        if (strcmp(m, "initialize") == 0) {
            responder_initialize(cJSON_GetObjectItem(msg, "id"));

        } else if (strcmp(m, "textDocument/didOpen") == 0) {
            cJSON *texto = doc ? cJSON_GetObjectItem(doc, "text") : NULL;
            if (cJSON_IsString(uri) && cJSON_IsString(texto))
                publicar_diagnosticos(uri->valuestring, texto->valuestring);

        } else if (strcmp(m, "textDocument/didChange") == 0) {
            // Con sync FULL viene un solo cambio y trae el archivo entero.
            cJSON *cambios = cJSON_GetObjectItem(params, "contentChanges");
            cJSON *ultimo  = cambios ? cJSON_GetArrayItem(cambios,
                                cJSON_GetArraySize(cambios) - 1) : NULL;
            cJSON *texto   = ultimo ? cJSON_GetObjectItem(ultimo, "text") : NULL;
            if (cJSON_IsString(uri) && cJSON_IsString(texto))
                publicar_diagnosticos(uri->valuestring, texto->valuestring);

        } else if (strcmp(m, "textDocument/didClose") == 0) {
            // Lista vacia: le dice al editor que borre los subrayados. Sin esto
            // quedan pegados aunque el archivo ya no este abierto.
            if (cJSON_IsString(uri)) publicar_diagnosticos(uri->valuestring, "");

        } else if (strcmp(m, "shutdown") == 0) {
            cJSON *res = cJSON_CreateObject();
            cJSON_AddStringToObject(res, "jsonrpc", "2.0");
            cJSON_AddItemToObject(res, "id", cJSON_Duplicate(cJSON_GetObjectItem(msg, "id"), 1));
            cJSON_AddNullToObject(res, "result");
            enviar(res);
            cJSON_Delete(res);

        } else if (strcmp(m, "exit") == 0) {
            cJSON_Delete(msg);
            return 0;
        }
        // Cualquier otro metodo se ignora a proposito: el editor ya sabe, por
        // las capacidades del apreton de manos, que este servidor no lo tiene.

        cJSON_Delete(msg);
    }
    return 0;
}

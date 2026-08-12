// gen_sintaxis — genera los archivos derivados de data/sintaxis.json.
//
// sintaxis.json es la fuente unica de verdad del lenguaje PAED. Todo lo que
// necesite la lista de palabras clave se GENERA desde aca; nada se copia a mano.
//
//   data/sintaxis.json  ──┬──> syntaxes/paed.tmLanguage.json  (VSCode/TextMate)
//                         └──> core/palabras.sh               (validador bash)
//
// syntax/paed.lua no se genera: lee el JSON directo en runtime.
//
// Uso:  gen_sintaxis <sintaxis.json> <salida.tmLanguage.json> <palabras.sh>

#include "../../../cjson/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *leer_archivo(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "gen_sintaxis: no se pudo abrir %s\n", path); return NULL; }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return NULL; }

    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    buf[fread(buf, 1, (size_t)size, f)] = '\0';
    fclose(f);
    return buf;
}

// Solo las palabras que son \w+ pueden ir en un regex de TextMate con \b.
static int es_palabra(const char *s) {
    for (const char *c = s; *c; c++)
        if (!((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || *c == '_'))
            return 0;
    return *s != '\0';
}

static int tiene_palabras(cJSON *cat) {
    cJSON *palabras = cJSON_GetObjectItem(cat, "palabras");
    cJSON *p = NULL;
    cJSON_ArrayForEach(p, palabras)
        if (cJSON_IsString(p) && es_palabra(p->valuestring)) return 1;
    return 0;
}

static void gen_tmlanguage(cJSON *raiz, FILE *out) {
    cJSON *categorias = cJSON_GetObjectItem(raiz, "categorias");
    cJSON *cat = NULL;

    fprintf(out, "{\n");
    fprintf(out, "    \"$schema\": \"https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json\",\n");
    fprintf(out, "    \"name\": \"Paed\",\n");
    fprintf(out, "    \"scopeName\": \"source.paed\",\n");
    fprintf(out, "    \"fileTypes\": [\"paed\"],\n");
    fprintf(out, "    \"_generado\": \"NO EDITAR A MANO — sale de data/sintaxis.json via tools/generar.sh\",\n");

    // 1. Orden de aplicacion: comentarios y strings primero.
    fprintf(out, "    \"patterns\": [\n");
    int primero = 1;
    cJSON_ArrayForEach(cat, categorias) {
        cJSON *nombre = cJSON_GetObjectItem(cat, "nombre");
        if (!cJSON_IsString(nombre)) continue;
        if (!cJSON_GetObjectItem(cat, "patron") && !tiene_palabras(cat)) continue;
        fprintf(out, "%s        { \"include\": \"#%s\" }", primero ? "" : ",\n", nombre->valuestring);
        primero = 0;
    }
    fprintf(out, "\n    ],\n");

    // 2. Repositorio: una regla por categoria.
    fprintf(out, "    \"repository\": {\n");
    primero = 1;
    cJSON_ArrayForEach(cat, categorias) {
        cJSON *nombre = cJSON_GetObjectItem(cat, "nombre");
        cJSON *scope  = cJSON_GetObjectItem(cat, "scope");
        cJSON *patron = cJSON_GetObjectItem(cat, "patron");
        if (!cJSON_IsString(nombre) || !cJSON_IsString(scope)) continue;

        if (cJSON_IsString(patron)) {
            const char *pat = patron->valuestring;
            if (!primero) fprintf(out, ",\n");
            primero = 0;
            if (strcmp(pat, "//") == 0) {
                fprintf(out, "        \"%s\": { \"match\": \"//.*$\", \"name\": \"%s\" }",
                        nombre->valuestring, scope->valuestring);
            } else if (strcmp(pat, "\".*\"") == 0) {
                fprintf(out, "        \"%s\": { \"begin\": \"\\\"\", \"end\": \"\\\"\", \"name\": \"%s\" }",
                        nombre->valuestring, scope->valuestring);
            } else if (strcmp(pat, "'.*'") == 0) {
                fprintf(out, "        \"%s\": { \"begin\": \"'\", \"end\": \"'\", \"name\": \"%s\" }",
                        nombre->valuestring, scope->valuestring);
            } else {
                fprintf(out, "        \"%s\": { \"match\": \"\\\\b[0-9]+(\\\\.[0-9]+)?\\\\b\", \"name\": \"%s\" }",
                        nombre->valuestring, scope->valuestring);
            }
            continue;
        }

        if (!tiene_palabras(cat)) continue;
        if (!primero) fprintf(out, ",\n");
        primero = 0;

        fprintf(out, "        \"%s\": { \"match\": \"\\\\b(", nombre->valuestring);
        cJSON *p = NULL;
        int primera_palabra = 1;
        cJSON_ArrayForEach(p, cJSON_GetObjectItem(cat, "palabras")) {
            if (!cJSON_IsString(p) || !es_palabra(p->valuestring)) continue;
            fprintf(out, "%s%s", primera_palabra ? "" : "|", p->valuestring);
            primera_palabra = 0;
        }
        fprintf(out, ")\\\\b\", \"name\": \"%s\" }", scope->valuestring);
    }
    fprintf(out, "\n    }\n}\n");
}

static void gen_palabras_sh(cJSON *raiz, FILE *out) {
    fprintf(out, "#!/bin/bash\n");
    fprintf(out, "# GENERADO — NO EDITAR A MANO.\n");
    fprintf(out, "# Sale de data/sintaxis.json via tools/generar.sh\n");
    fprintf(out, "# Uso: source core/palabras.sh\n\n");

    cJSON *cat = NULL;
    cJSON_ArrayForEach(cat, cJSON_GetObjectItem(raiz, "categorias")) {
        cJSON *nombre = cJSON_GetObjectItem(cat, "nombre");
        if (!cJSON_IsString(nombre) || !tiene_palabras(cat)) continue;

        fprintf(out, "PAED_%s=(", nombre->valuestring);
        cJSON *p = NULL;
        int primera = 1;
        cJSON_ArrayForEach(p, cJSON_GetObjectItem(cat, "palabras")) {
            if (!cJSON_IsString(p) || !es_palabra(p->valuestring)) continue;
            fprintf(out, "%s\"%s\"", primera ? "" : " ", p->valuestring);
            primera = 0;
        }
        fprintf(out, ")\n");
    }

    fprintf(out, "\nPAED_procedimientos=(");
    cJSON *proc = NULL;
    int primera = 1;
    cJSON_ArrayForEach(proc, cJSON_GetObjectItem(raiz, "procedimientos")) {
        cJSON *n = cJSON_GetObjectItem(proc, "nombre");
        if (!cJSON_IsString(n)) continue;
        fprintf(out, "%s\"%s\"", primera ? "" : " ", n->valuestring);
        primera = 0;
    }
    fprintf(out, ")\n");
}

// Header para los consumidores en C (el resaltador del editor, por ejemplo).
// Se emiten macros y no arrays: asi quien lo incluye decide el storage y no se
// come warnings de "variable declarada y no usada".
static void gen_header_c(cJSON *raiz, FILE *out) {
    fprintf(out, "// GENERADO — NO EDITAR A MANO.\n");
    fprintf(out, "// Sale de PseudoGames/Frankly/data/sintaxis.json via tools/generar.sh\n");
    fprintf(out, "//\n");
    fprintf(out, "// Uso:  static const char *kw_struct[] = PAED_KW_ESTRUCTURA;\n\n");
    fprintf(out, "#ifndef PAED_KEYWORDS_H\n#define PAED_KEYWORDS_H\n\n");

    cJSON *cat = NULL;
    cJSON_ArrayForEach(cat, cJSON_GetObjectItem(raiz, "categorias")) {
        cJSON *nombre = cJSON_GetObjectItem(cat, "nombre");
        if (!cJSON_IsString(nombre) || !tiene_palabras(cat)) continue;

        fprintf(out, "#define PAED_KW_");
        for (const char *c = nombre->valuestring; *c; c++)
            fputc((*c >= 'a' && *c <= 'z') ? *c - 32 : *c, out);
        fprintf(out, " { ");

        cJSON *p = NULL;
        cJSON_ArrayForEach(p, cJSON_GetObjectItem(cat, "palabras")) {
            if (!cJSON_IsString(p) || !es_palabra(p->valuestring)) continue;
            fprintf(out, "\"%s\", ", p->valuestring);
        }
        fprintf(out, "NULL }\n");
    }

    fprintf(out, "\n#endif // PAED_KEYWORDS_H\n");
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "uso: %s <sintaxis.json> <salida.tmLanguage.json> <palabras.sh> <paed_keywords.h>\n", argv[0]);
        return 1;
    }

    char *texto = leer_archivo(argv[1]);
    if (!texto) return 1;

    cJSON *raiz = cJSON_Parse(texto);
    free(texto);
    if (!raiz) {
        fprintf(stderr, "gen_sintaxis: %s tiene JSON invalido\n", argv[1]);
        return 1;
    }

    FILE *tm = fopen(argv[2], "w");
    if (!tm) { fprintf(stderr, "gen_sintaxis: no se pudo escribir %s\n", argv[2]); cJSON_Delete(raiz); return 1; }
    gen_tmlanguage(raiz, tm);
    fclose(tm);

    FILE *sh = fopen(argv[3], "w");
    if (!sh) { fprintf(stderr, "gen_sintaxis: no se pudo escribir %s\n", argv[3]); cJSON_Delete(raiz); return 1; }
    gen_palabras_sh(raiz, sh);
    fclose(sh);

    FILE *h = fopen(argv[4], "w");
    if (!h) { fprintf(stderr, "gen_sintaxis: no se pudo escribir %s\n", argv[4]); cJSON_Delete(raiz); return 1; }
    gen_header_c(raiz, h);
    fclose(h);

    cJSON_Delete(raiz);
    printf("gen_sintaxis: regenerados desde %s\n  %s\n  %s\n  %s\n", argv[1], argv[2], argv[3], argv[4]);
    return 0;
}

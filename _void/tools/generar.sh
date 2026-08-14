#!/bin/bash
# generar.sh — regenera todo lo que deriva de data/sintaxis.json.
#
# Corre esto CADA VEZ que toques data/sintaxis.json. Es lo unico que mantiene
# a raya la duplicacion: nadie copia palabras clave a mano.
#
# Uso:  ./tools/generar.sh   (desde PseudoGames/Frankly)

set -euo pipefail

FRANKLY="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(cd "$FRANKLY/../.." && pwd)"

CC=${CC:-clang}
BIN="$FRANKLY/tools/gen_sintaxis"

echo "==> compilando gen_sintaxis"
"$CC" -Wall -Wextra -O2 \
    "$FRANKLY/tools/gen_sintaxis.c" \
    "$REPO/cjson/cJSON.c" \
    -o "$BIN"

echo "==> generando archivos derivados"
mkdir -p "$FRANKLY/generated"
"$BIN" \
    "$FRANKLY/data/sintaxis.json" \
    "$FRANKLY/syntaxes/paed.tmLanguage.json" \
    "$FRANKLY/core/palabras.sh" \
    "$FRANKLY/generated/paed_keywords.h"

chmod +x "$FRANKLY/core/palabras.sh"
rm -f "$BIN"

echo "==> listo"

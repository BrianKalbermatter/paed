#!/usr/bin/env bash
# Instala el resaltado de PAED en Helix.
#
# Helix NO colorea con JSON: usa tree-sitter. Hacen falta tres piezas:
#
#   grammar.js            la gramatica (lee las keywords de data/sintaxis.json)
#   src/parser.c          el parser en C que sale de generarla
#   queries/*.scm         que color le toca a cada cosa
#
#   ./instalar.sh          compila el parser e instala las queries
#   ./instalar.sh --regen  ademas regenera src/parser.c desde grammar.js
#
# --regen solo hace falta si tocaste grammar.js o agregaste palabras a
# sintaxis.json, y necesita el CLI de tree-sitter:
#
#   npm install -g tree-sitter-cli
#
# Sin --regen alcanza con gcc, que ya tenes.

set -euo pipefail

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DESTINO="${XDG_CONFIG_HOME:-$HOME/.config}/helix/runtime"

cd "$AQUI"

if [[ "${1:-}" == "--regen" ]]; then
    if ! command -v tree-sitter >/dev/null 2>&1; then
        echo "falta el CLI de tree-sitter. Instalalo con:" >&2
        echo "    npm install -g tree-sitter-cli" >&2
        exit 1
    fi
    echo "==> regenerando src/parser.c desde grammar.js"
    tree-sitter generate
fi

if [[ ! -f src/parser.c ]]; then
    echo "falta src/parser.c — corré: ./instalar.sh --regen" >&2
    exit 1
fi

echo "==> compilando el grammar"
mkdir -p "$DESTINO/grammars" "$DESTINO/queries/paed"
gcc -shared -fPIC -O2 -I src src/parser.c -o "$DESTINO/grammars/paed.so"

echo "==> instalando las queries"
cp queries/*.scm "$DESTINO/queries/paed/"

echo "==> listo. Verificá con:"
echo "    helix --health paed"
echo
echo "Si 'Tree-sitter parser' sale con ✘, falta el bloque de PAED en"
echo "~/.config/helix/languages.toml — está de ejemplo en languages.toml.ejemplo"

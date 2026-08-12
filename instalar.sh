#!/usr/bin/env bash
# Instala PAED desde el paquete ya compilado.
#
#   ./instalar.sh                      elige solo donde (ver abajo)
#   ./instalar.sh /opt/paed            lo pone donde vos digas
#   PREFIX=~/.local ./instalar.sh      lo mismo, por variable
#
# Copia dos cosas y nada mas:
#
#   $PREFIX/bin/paed                   el interprete
#   $PREFIX/share/paed/sintaxis.json   la definicion del lenguaje
#
# El .json NO es un extra: sin el, el binario no sabe que es una palabra clave.
# El lenguaje se define ahi, no en el C.

set -euo pipefail

aqui=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# ── Donde instalar ──────────────────────────────────────────────────────────
# /usr/local es lo estandar para lo que uno instala a mano, pero necesita root.
# Si no lo tenemos, ~/.local es el equivalente para un solo usuario y no pide
# permiso a nadie. Se elige mirando si SE PUEDE ESCRIBIR, no si el usuario "es"
# root: en un contenedor sos root sin sudo, y con sudo -E sos root sin serlo.
if [ $# -ge 1 ]; then
    PREFIX="$1"
elif [ -n "${PREFIX:-}" ]; then
    :
elif [ -w /usr/local/bin ] 2>/dev/null; then
    PREFIX=/usr/local
else
    PREFIX="$HOME/.local"
fi

BINDIR="$PREFIX/bin"
DATADIR="$PREFIX/share/paed"

echo "Instalando PAED en $PREFIX"

mkdir -p "$BINDIR" "$DATADIR"
install -m 755 "$aqui/bin/paed"                 "$BINDIR/paed"
install -m 644 "$aqui/share/paed/sintaxis.json" "$DATADIR/sintaxis.json"

echo "  $BINDIR/paed"
echo "  $DATADIR/sintaxis.json"
echo

# ── ¿Va a poder llamarse por su nombre? ─────────────────────────────────────
# Instalar y que el comando no exista es la forma mas comun de que esto parezca
# roto. Se avisa ANTES de que pase, con la linea exacta para arreglarlo.
if command -v paed >/dev/null 2>&1 && [ "$(command -v paed)" = "$BINDIR/paed" ]; then
    echo "Listo. Probalo:"
    echo "    paed tu_programa.paed"
else
    echo "OJO: $BINDIR no esta en tu PATH, asi que el comando 'paed' todavia no existe."
    echo "Agregalo una sola vez:"
    echo
    echo "    echo 'export PATH=\"$BINDIR:\$PATH\"' >> ~/.bashrc && source ~/.bashrc"
    echo
    echo "Mientras tanto anda por ruta completa:  $BINDIR/paed tu_programa.paed"
fi

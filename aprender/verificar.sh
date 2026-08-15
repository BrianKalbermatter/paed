#!/usr/bin/env bash
# Verifica el tutorial entero, de punta a punta.
#
# Son dos afirmaciones, y las dos importan:
#
#   1. TODOS los ejercicios fallan tal como vienen. Un ejercicio que ya pasa no
#      es un ejercicio: es ruido que el alumno saltea sin leer, y peor, es un
#      hueco en la progresion que nadie nota.
#   2. TODAS las soluciones pasan. Es lo que protege al curso el dia que se
#      toque el evaluador: si un cambio en expr.c altera como se imprime un
#      real, la solucion del ejercicio 11 deja de coincidir y esto lo caza acá,
#      no en la cara de alguien que esta aprendiendo.
#
# Se prueba a traves del propio `paed aprender`, no reimplementando la
# comparacion en bash: asi esto tambien verifica el tutor, no solo los .paed.
#
#   make test-aprender

set -uo pipefail

raiz=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$raiz" || exit 1

paed=build/paed
if [ ! -x "$paed" ]; then
    echo "falta $paed — corré 'make' primero" >&2
    exit 1
fi

total=$(ls aprender/[0-9]*.paed | wc -l)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

fallas=0

# ── 1. tal como vienen, ninguno pasa ────────────────────────────────────────
"$paed" aprender init "$tmp" > /dev/null
roto=$("$paed" aprender lista "$tmp" | tail -1)
if [ "$roto" = "0 de $total" ]; then
    echo "  ok       los $total ejercicios vienen rotos"
else
    echo "  FALLA    algun ejercicio ya pasa sin tocarlo ($roto)"
    "$paed" aprender lista "$tmp" | grep '^  ok' | sed 's/^/           ya pasa: /'
    fallas=$((fallas + 1))
fi

# ── 2. con las soluciones puestas, pasan todos ──────────────────────────────
cp aprender/solucion/*.paed "$tmp"/
hecho=$("$paed" aprender lista "$tmp" | tail -1)
if [ "$hecho" = "$total de $total" ]; then
    echo "  ok       las $total soluciones pasan"
else
    echo "  FALLA    alguna solucion no pasa ($hecho)"
    "$paed" aprender lista "$tmp" | grep '^  \.' | sed 's/^/           no pasa: /'
    fallas=$((fallas + 1))
fi

# ── 3. cada ejercicio tiene su solucion, y al reves ─────────────────────────
for e in aprender/[0-9]*.paed; do
    s="aprender/solucion/$(basename "$e")"
    [ -f "$s" ] || { echo "  FALLA    $(basename "$e") no tiene solucion"; fallas=$((fallas + 1)); }
done
for s in aprender/solucion/*.paed; do
    e="aprender/$(basename "$s")"
    [ -f "$e" ] || { echo "  FALLA    $(basename "$s") es una solucion sin ejercicio"; fallas=$((fallas + 1)); }
done

echo
if [ "$fallas" -gt 0 ]; then
    echo "el tutorial tiene $fallas problema(s)"
    exit 1
fi
echo "tutorial ok: $total ejercicios"

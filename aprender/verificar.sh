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

# Un ejercicio esta ESCRITO cuando declara su salida esperada. Los que todavia
# son molde no cuentan para nada: el tutorial los saltea, no se les pide
# solucion, y no entran en el "N de M". Asi se puede dejar el esqueleto de un
# ejercicio en el repo sin que `make test` se ponga rojo por algo que todavia
# no existe.
escrito() { grep -q '^// ── SALIDA ESPERADA' "$1"; }

listos=()
molde=()
for e in aprender/[0-9]*.paed; do
    if escrito "$e"; then listos+=("$e"); else molde+=("$e"); fi
done
total=${#listos[@]}
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

# ── 3. cada ejercicio ESCRITO tiene su solucion, y al reves ─────────────────
for e in "${listos[@]}"; do
    sol="aprender/solucion/$(basename "$e")"
    [ -f "$sol" ] || { echo "  FALLA    $(basename "$e") no tiene solucion"; fallas=$((fallas + 1)); }
done
for sol in aprender/solucion/*.paed; do
    e="aprender/$(basename "$sol")"
    [ -f "$e" ] || { echo "  FALLA    $(basename "$sol") es una solucion sin ejercicio"; fallas=$((fallas + 1)); }
done

# Un molde con solucion es una inconsistencia: o el ejercicio esta escrito y le
# falta el bloque de salida, o la solucion sobra.
for e in "${molde[@]:-}"; do
    [ -n "$e" ] || continue
    sol="aprender/solucion/$(basename "$e")"
    [ -f "$sol" ] && { echo "  FALLA    $(basename "$e") es un molde pero ya tiene solucion"; fallas=$((fallas + 1)); }
done
if [ "${#molde[@]}" -gt 0 ]; then
    echo "  ok       ${#molde[@]} molde(s) sin escribir, salteados a proposito"
fi

# ── 3b. el tutorial saltea los moldes y muestra el modulo ───────────────────
#
# Dos afirmaciones sobre la estructura en modulos:
#   - `lista` cuenta SOLO los ejercicios escritos: un molde no es un ejercicio
#   - el encabezado dice a que modulo pertenece el ejercicio actual, porque
#     "ejercicio 13 de 18" no le dice a nadie que ya entro en archivos
lst=$("$paed" aprender lista "$tmp")
if printf '%s' "$lst" | grep -qF "$(basename "${molde[0]:-__ninguno__}")"; then
    echo "  FALLA    'lista' muestra un ejercicio que todavia es molde"
    fallas=$((fallas + 1))
else
    echo "  ok       'lista' no muestra los moldes"
fi

# En carpeta propia y recien desempacada: en $tmp ya estan las soluciones
# puestas por el paso 2, o sea que ahi no hay ningun ejercicio actual que
# mostrar y el encabezado nunca se imprimiria.
fresco=$(mktemp -d)
"$paed" aprender init "$fresco" > /dev/null
cab=$("$paed" aprender "$fresco" 2>&1 | head -3)
rm -rf "$fresco"
if printf '%s' "$cab" | grep -qF "Introduccion"; then
    echo "  ok       el encabezado nombra el modulo"
else
    echo "  FALLA    el encabezado no nombra el modulo"
    printf '%s\n' "$cab" | sed 's/^/           | /'
    fallas=$((fallas + 1))
fi

# ── 4. un ejercicio que no compila se reporta con la linea marcada ──────────
#
# Cuando el codigo del alumno ni siquiera parsea, comparar la salida no sirve
# de nada: no hubo salida. Lo unico util ahi es DONDE esta el error, y por eso
# el tutorial tiene que mostrar el codigo con la linea culpable senalada.
#
# Se prueba con dos archivos rotos a proposito, porque son dos casos distintos:
#
#   a) el pedazo que el parser cita aparece UNA sola vez en la linea -> flecha
#   b) aparece DOS veces -> no hay flecha, porque no se sabe a cual apunta, y
#      una flecha en el lugar equivocado manda a leer donde no esta el problema
roto=$(mktemp -d)
"$paed" aprender init "$roto" > /dev/null
primero=$(basename "$(ls aprender/[0-9]*.paed | head -1)")

esperar() {
    if printf '%s' "$rep" | grep -qF "$1"; then
        echo "  ok       $2"
    else
        echo "  FALLA    $2"
        printf '%s\n' "$rep" | sed 's/^/           | /'
        fallas=$((fallas + 1))
    fi
}

# (a) el '(' que el parser cita aparece una sola vez en la linea 4
cat > "$roto/$primero" <<'FIN'
ACCION roto ES
PROCESO
    ESCRIBIR("uno");
    ("dos");
FIN_ACCION

// ── SALIDA ESPERADA ──
// uno
// dos
FIN

rep=$("$paed" aprender "$roto" 2>&1)
esperar "no compila"                         "avisa que no compila"
esperar '4 │     ("dos");'                   "muestra la linea 4 con su codigo"
esperar "falta el nombre del procedimiento"  "trae el mensaje del parser"
esperar "▲"                                  "senala la columna cuando es inequivoca"

# (b) el '(' aparece DOS veces en la misma linea: no se puede afirmar cual es
cat > "$roto/$primero" <<'FIN'
ACCION roto ES
PROCESO
    ESCRIBIR("uno"); ("dos");
FIN_ACCION

// ── SALIDA ESPERADA ──
// uno
// dos
FIN

rep=$("$paed" aprender "$roto" 2>&1)
if printf '%s' "$rep" | grep -qF "▲"; then
    echo "  FALLA    puso flecha en una linea donde el pedazo citado es ambiguo"
    fallas=$((fallas + 1))
else
    echo "  ok       no inventa columna cuando el pedazo citado es ambiguo"
fi
esperar "falta el nombre del procedimiento"  "igual reporta el error sin flecha"

rm -rf "$roto"


echo
if [ "$fallas" -gt 0 ]; then
    echo "el tutorial tiene $fallas problema(s)"
    exit 1
fi
echo "tutorial ok: $total ejercicios"

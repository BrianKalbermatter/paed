#!/usr/bin/env bash
# Corre cada programa .paed de esta carpeta y compara su salida contra la que
# el propio archivo declara al final, en un bloque de comentarios:
#
#     FIN_ACCION
#
#     // ── ENTRADA ──
#     // 10
#     // 45
#
#     // ── SALIDA ESPERADA ──
#     // 1..10 suman 55
#     // Gauss tenia razon
#
# El bloque ENTRADA es OPCIONAL y alimenta a los LEER, una linea por destino.
# Va ANTES del de salida, y por la misma razon que la salida vive en el .paed:
# un test es UN archivo, y los datos que lo hacen andar no se guardan lejos del
# codigo que los pide.
#
# Un test es UN archivo. Antes la salida vivía en un .esperado al lado, y eso
# tenía dos problemas: duplicaba archivos, y la referencia quedaba lejos del
# código que la produce.
#
# NO existe un modo que regrabe la salida sola. Si un test falla, el runner
# muestra el diff y el bloque se corrige A MANO. Regrabar sin leer es cómo un
# test deja de proteger: "arregla" el test en vez del bug.
#
#   make test          (o: bash tests/correr.sh)
#
# Agregar un test = dejar el .paed con su bloque al final. Nada más: no hay
# ninguna lista que mantener.

set -uo pipefail

MARCA='// ── SALIDA ESPERADA'
MARCA_IN='// ── ENTRADA'
MARCA_LIB='// ── LIBRERIA'

# La raiz del repo de PAED: un nivel arriba de tests/
raiz=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$raiz" || exit 1

runner=build/paed
if [ ! -x "$runner" ]; then
    echo "falta $runner — corré 'make lang' primero" >&2
    exit 1
fi

tests_dir=tests
pasaron=0
fallaron=0
fallidos=()

# Saca el bloque de salida esperada de un .paed: desde la marca hasta el final,
# quitando la marca y el '// ' de cada línea.
#
# Va con awk y no con sed porque la marca EMPIEZA con '//', y esas barras le
# cierran el patrón a sed en la cara.
esperado_de() {
    awk -v m="$MARCA" 'index($0, m) == 1 { visto = 1; next } visto' "$1" \
        | sed 's|^// \?||'
}

# Saca el bloque de entrada: desde su marca hasta la de SALIDA ESPERADA (o el
# final del archivo). Se corta en la marca de salida a proposito — si no, la
# salida esperada terminaria entrando por stdin como si fueran datos.
#
# Las lineas en blanco se descartan: un LEER trata la linea vacia como un dato
# que falta, y no queremos que un renglon de aire separando los bloques rompa el
# test.
entrada_de() {
    awk -v m="$MARCA_IN" -v s="$MARCA" '
        index($0, s) == 1 { exit }
        index($0, m) == 1 { visto = 1; next }
        visto
    ' "$1" | sed 's|^// \?||' | grep -v '^[[:space:]]*$'
}

for prog in "$tests_dir"/*.paed; do
    [ -e "$prog" ] || continue
    nombre=$(basename "$prog")

    if ! grep -q "^${MARCA}" "$prog"; then
        echo "  SIN BLOQUE   $nombre (falta '${MARCA} ──' al final)"
        fallaron=$((fallaron + 1))
        fallidos+=("$nombre")
        continue
    fi

    # stderr se junta con stdout: los mensajes de error son parte de lo que se
    # verifica. Un error con el texto equivocado es un test fallado.
    #
    # stdin viene SIEMPRE del bloque ENTRADA, aunque este vacio: asi un test que
    # se olvido de declarar sus datos falla con "la entrada se termino" en vez de
    # quedarse esperando que alguien tipee y colgar la corrida entera.
    # Un test puede pedir una libreria que NO es del lenguaje, declarandolo en
    # el archivo:  // ── LIBRERIA escena
    # Se pide por nombre y no por ruta: la ruta depende de donde este instalado
    # PAED, y el test tiene que dar lo mismo en cualquier maquina.
    lib=$(awk -v m="$MARCA_LIB" 'index($0, m) == 1 { print $NF; exit }' "$prog")
    lib_args=()
    [ -n "$lib" ] && lib_args=(--lib "$lib")

    real=$(entrada_de "$prog" | "$runner" "${lib_args[@]}" "$prog" 2>&1)

    if [ "$real" = "$(esperado_de "$prog")" ]; then
        echo "  ok       $nombre"
        pasaron=$((pasaron + 1))
    else
        echo "  FALLA    $nombre"
        diff -u <(esperado_de "$prog") <(printf '%s\n' "$real") \
            | sed 's/^/           /'
        fallaron=$((fallaron + 1))
        fallidos+=("$nombre")
    fi
done

# ── Guarda: las palabras del parser estan en sintaxis.json ───────────────────
#
# La regla del proyecto es una sola: toda palabra que reconozca lang/src/parser.c
# tiene que estar en data/sintaxis.json, porque de ahi las leen los resaltadores
# (tree-sitter para Helix, y lo que venga despues).
#
# No se cumple acordandose. El 2026-08-27 se agrego USAR al parser y NO al json:
# los 49 tests pasaron igual, el lenguaje funciono igual, y lo unico roto era el
# color. Es el tipo de error que nadie encuentra hasta que se acumulan cuatro.
#
# Las palabras se sacan del propio parser.c con una regex sobre las formas con
# que compara palabras clave. Es una heuristica, no un analisis: cubre las que
# abren bloque, que son donde vive el riesgo. Si manana el parser compara de
# otra forma, hay que agregar el patron aca.
faltantes=$(
    rg -o 'empieza_con\(linea, "[A-Z_]+"\)|strncasecmp\(linea, "[A-Z_]+"|palabra_suelta\([a-z]+, "[A-Z_]+"\)|kw_es\(linea, "[A-Z_]+"\)' \
       lang/src/parser.c \
    | rg -o '"[A-Z_]+"' | tr -d '"' | sort -u \
    | while IFS= read -r palabra; do
        python3 -c "
import json, sys
d = json.load(open('data/sintaxis.json'))
todas = {p.upper() for c in d.get('categorias', []) for p in c.get('palabras', [])}
sys.exit(0 if sys.argv[1].upper() in todas else 1)
" "$palabra" || echo "$palabra"
      done | tr '\n' ' '
)

if [ -n "${faltantes// /}" ]; then
    echo
    echo "  FALLA    sintaxis.json"
    echo "           el parser reconoce estas palabras y el json no las tiene:"
    echo "             $faltantes"
    echo "           agregalas a data/sintaxis.json o los resaltadores no las pintan"
    fallaron=$((fallaron + 1))
    fallidos+=("sintaxis.json")
else
    echo "  ok       sintaxis.json cubre las palabras del parser"
    pasaron=$((pasaron + 1))
fi

echo
echo "$pasaron pasaron, $fallaron fallaron"
if [ "$fallaron" -gt 0 ]; then
    printf 'fallaron: %s\n' "${fallidos[*]}"
    echo "corregí el bloque SALIDA ESPERADA del .paed a mano, leyendo el diff"
    exit 1
fi

#!/usr/bin/env bash
# Mete los ejercicios ADENTRO del binario.
#
# Genera lang/src/ejercicios_embebidos.c a partir de aprender/NN_*.paed. Es
# exactamente el mismo truco que el Makefile usa con data/sintaxis.json, y por
# la misma razon: `paed` tiene que ser UN archivo que se baja suelto y ya sabe
# todo lo que necesita. Un tutorial que exige clonar el repo no es un tutorial
# que viene con el binario.
#
# El orden lo da el NOMBRE del archivo, y por eso los ejercicios empiezan con
# un numero. No hay ninguna lista que mantener: agregar un ejercicio es dejar
# un .paed en esta carpeta y volver a compilar.
#
#   bash aprender/generar.sh > lang/src/ejercicios_embebidos.c

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

echo '// GENERADO por aprender/generar.sh desde aprender/*.paed — NO EDITAR'
echo '#include <paed/aprender.h>'
echo

# Cada archivo se convierte en un arreglo de char: se escapan las barras
# invertidas PRIMERO (si no, las que agrega el escape de comillas se
# volverian a escapar), despues las comillas, y cada linea queda como un
# literal propio terminado en \n. Los caracteres UTF-8 de las rayas viajan
# como bytes crudos, que es lo que el compilador espera.
n=0
nombres=()
for f in [0-9]*.paed; do
    var="EJ_$n"
    nombres+=("$f|$var")
    echo "static const char $var[] ="
    sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$/\\n"/' "$f"
    echo ';'
    echo
    n=$((n + 1))
done

if [ "$n" -eq 0 ]; then
    echo "aprender/generar.sh: no hay ningun ejercicio en $(pwd)" >&2
    exit 1
fi

echo 'const PaedEjercicio PAED_EJERCICIOS[] = {'
for par in "${nombres[@]}"; do
    printf '    { "%s", %s },\n' "${par%|*}" "${par#*|}"
done
echo '};'
echo "const int PAED_EJERCICIOS_N = $n;"
echo

# ── Los modulos ─────────────────────────────────────────────────────────────
#
# modulos.txt le pone nombre a cada prefijo: el ejercicio 1-03_cadenas.paed es
# del modulo 1, y el modulo 1 se llama "Secuencias de datos elementales y
# subacciones". Sin esto el tutorial solo podria decir "ejercicio 13 de 18", y
# ese numero no le avisa a nadie que acaba de entrar en otro tema.
#
# Cada renglon util es:  <numero><espacios><nombre hasta el fin de linea>
# Los que empiezan con '#' y los vacios se ignoran.
if [ ! -f modulos.txt ]; then
    echo "aprender/generar.sh: falta $(pwd)/modulos.txt" >&2
    exit 1
fi

m=0
echo 'const PaedModulo PAED_MODULOS[] = {'
while IFS= read -r renglon || [ -n "$renglon" ]; do
    renglon=${renglon%$'\r'}
    case "$renglon" in ''|'#'*) continue ;; esac

    num=${renglon%%[![:digit:]]*}
    [ -n "$num" ] || { echo "aprender/generar.sh: renglon sin numero en modulos.txt: $renglon" >&2; exit 1; }

    nombre=${renglon#"$num"}
    # se le sacan los espacios de los dos lados sin llamar a nadie de afuera
    nombre=${nombre#"${nombre%%[![:space:]]*}"}
    nombre=${nombre%"${nombre##*[![:space:]]}"}

    printf '    { %d, "%s" },\n' "$num" "$(printf '%s' "$nombre" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g')"
    m=$((m + 1))
done < modulos.txt
echo '};'
echo "const int PAED_MODULOS_N = $m;"

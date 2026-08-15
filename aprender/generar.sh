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

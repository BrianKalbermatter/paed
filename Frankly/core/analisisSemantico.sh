#!/bin/bash
# Analisis semantico: tabla de simbolos del programa .paed en curso.
# Se sourcea desde el interprete `paed`.

# declare -A crea un array asociativo (diccionario clave -> valor).
declare -A variables

# OJO con la sintaxis de bash: en una asignacion NO puede haber espacios
# alrededor del '='. `variables["x"] ="Hola"` no asigna, intenta ejecutar
# el comando `variables[x]` con el argumento `=Hola`.
variables["mensaje"]="Hola"

echo "${variables["mensaje"]}"

#!/bin/bash

#Esta funcion valida al final lo que la otra funcion leyo que
# esta ahora en paed. Probablemente la cambie a una funcion
# aparte para ser mas legible.
validacion_end(){
    if [ $find_ACCION -eq 0 ]; 
    then
        echo "$ARCHIVO: error: falta ACCION"
        return 1
    fi
    if [ $find_ES -eq 0 ]; then
        echo "$ARCHIVO: error: falta ES"
        return 1
    fi
    if [ $find_FIN_ACCION -eq 0 ]; then
        echo "$ARCHIVO: error: falta FIN_ACCION"
        return 1
    fi
}

verif_punto_coma(){
    local linea=$1
    local linea_num=$2

    case $linea in
        *";") ;; # termina ; ok, no hace nada
        *) echo "$ARCHIVO:$linea_num: error: falta ';'" ;;
    esac
}

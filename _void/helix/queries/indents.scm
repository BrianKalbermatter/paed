; Indentado automatico de PAED en Helix.
;
; Como la gramatica es lexica (reconoce tokens, no la estructura del programa),
; el indentado se hace por PALABRA CLAVE y no por bloque: la que abre suma un
; nivel, la que cierra lo saca.
;
; Es menos preciso que el indentado de C, que sabe donde empieza y termina cada
; llave. A cambio, no hace falta una gramatica completa que despues haya que
; mantener sincronizada con el parser de verdad, el de plugins/ide/.

; Abren bloque: lo que viene abajo va un nivel adentro
((estructura) @indent
 (#match? @indent "(?i)^(PROCESO|AMBIENTE|REGISTRO)$"))

((condicional) @indent
 (#match? @indent "(?i)^(ENTONCES|SINO)$"))

((bucle) @indent
 (#match? @indent "(?i)^(HACER|REPETIR)$"))

; Cierran bloque: la linea vuelve un nivel atras
((estructura) @outdent
 (#match? @outdent "(?i)^(FIN_ACCION|FINACCION|FIN_REGISTRO|FIN_FUNCION|FIN_PROCEDIMIENTO)$"))

((condicional) @outdent
 (#match? @outdent "(?i)^(SINO|FIN_SI)$"))

((bucle) @outdent
 (#match? @outdent "(?i)^(FIN_MIENTRAS|FIN_PARA|FIN_SEGUN)$"))

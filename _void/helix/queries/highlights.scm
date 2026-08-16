; Colores de PAED en Helix.
;
; Los nombres de la derecha (los "captures") NO son colores: son roles. El tema
; que tengas puesto decide de que color se pinta cada rol. Por eso se usan
; exactamente los mismos que usa C — asi PAED queda en la misma paleta que el
; resto del editor, con el tema que sea, sin elegir ningun color a mano.
;
; El ORDEN importa: gana la primera regla que matchea. Por eso `identificador`
; va ultimo, despues de todas las palabras clave.

; ── Comentarios ──────────────────────────────────────────────────────────────
(comentario) @comment

; ── Control de flujo ─────────────────────────────────────────────────────────
; Igual que if/else y for/while en C: Helix los distingue, y varios temas les
; dan matices distintos.
(condicional) @keyword.control.conditional
(bucle)       @keyword.control.repeat

; ── Estructura del programa ──────────────────────────────────────────────────
; PROCESO, AMBIENTE, FIN_ACCION, FUNCION... el andamiaje del algoritmo.
(estructura) @keyword

; El nombre de la ACCION se pinta como el nombre de una funcion en C, porque es
; lo mismo: el nombre del algoritmo que estas definiendo.
(cabecera_accion nombre: (identificador) @function)

; ── Tipos ────────────────────────────────────────────────────────────────────
; ENTERO, REAL, CARACTER se pintan como int, float, char.
(tipo) @type.builtin

; ── Procedimientos y funciones del lenguaje ──────────────────────────────────
; ESCRIBIR y LEER son a PAED lo que printf y scanf a C.
(entrada_salida)  @function.builtin
(funcion_builtin) @function.builtin
(secuencia)       @function.builtin

; ── Literales ────────────────────────────────────────────────────────────────
(booleano)  @constant.builtin.boolean   ; V y F, como true/false
(texto)     @string
(caracter)  @constant.character
(numero)    @constant.numeric

; ── Operadores ───────────────────────────────────────────────────────────────
; Y, O, NO, MOD, DIV son palabras pero operan, igual que `and`/`or` en Python.
(operador_palabra) @keyword.operator

; := y los simbolos
(asignacion) @operator
(operador)   @operator

; ── Puntuacion ───────────────────────────────────────────────────────────────
["(" ")" "[" "]"] @punctuation.bracket
[";" "," ":" "." ".."] @punctuation.delimiter

; ── Todo lo demas es una variable ────────────────────────────────────────────
; Va al final a proposito: cualquier palabra que no haya matcheado antes con
; una regla de arriba, es un nombre que puso el programador.
(identificador) @variable

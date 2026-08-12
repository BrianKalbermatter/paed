#Identacion, 4 espacios
#----------------------------
#Acciones basicas
find_ACCION=0
find_ES=0
find_FIN_ACCION=0
find_AMBIENTE=0
find_PROCESO=0
#----------------------------
find_ENTERO=0
find_REAL=0
find_CONSTANTE=0
#----------------------------
#Operadores logicos
find_Y=0
find_NO=0
find_O=0
#----------------------------
#Operadores arimeticos
# Suma                 +
# Resta                -
# Multiplicar          *
# Dividir              /
# Exponencial          **   (NO es '^' — ver TEORIA_COMPLETA.txt:364)
# Resto div. entera    MOD  (operador infijo: a MOD b)
# Cociente entero      DIV  (operador infijo: a DIV b)
# Igual                =
#----------------------------
# Asignacion          := 
# Definir ENTERO       :
# Constante            =
#----------------------------
#Mientras 
find_MIENTRAS=0
find_HACER=0
find_FIN_MIENTRAS=0
#----------------------------
#Condicionales
find_SI=0
find_SINO=0
# Debe tener la estructura de operadores logicos de usarlos si o si o no usarlos
find_FIN_SI=0 #Solo debe aparecer una sola vez al cerrar al igual que en bash con fi
#Parentesis
find_ENTONCES=0

#Registro
#Ultimo de la linea $2 = $last REGISTRO

#Archivo

#Lista

#Arreglo


#Funciones
find_FUNCION=0
#Parentesis
find_FIN_FUNCION=0

#Procedimiento/Metodo
find_PROCEDIMIENTO=0
#Parentesis
find_FIN_PROCEDIMIENTO=0

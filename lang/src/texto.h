#ifndef PAED_TEXTO_INTERNO_H
#define PAED_TEXTO_INTERNO_H

// Cortar y reconocer texto. Las usa todo el parser y no dependen de nada.
//
// Vive en src/ y no en include/paed/ porque no son parte de la API de PAED:
// son lo que los archivos del parser se prestan entre ellos.

// Saca los espacios de los dos extremos, EN EL LUGAR. Devuelve el arranque
// nuevo: el puntero que entra no sirve mas si habia espacios adelante.
char *trim(char *s);

// Borra el comentario de la linea, respetando los textos: un // adentro de
// comillas es parte del dato y no un comentario.
void strip_comment(char *s);

// Si el texto es un nombre valido: arranca con letra o guion bajo y sigue con
// letras, digitos o guiones bajos. Acepta tildes y ñ.
int es_identificador(const char *s);

// Si el texto es un acceso a campo de registro: 'reg.campo'.
int es_campo(const char *s);

// ── Palabras clave ───────────────────────────────────────────────────────────
//
// Las palabras clave NO distinguen mayusculas: la wiki escribe ARREGLO y
// arreglo[ para lo mismo. Los identificadores si las distinguen, y se comparan
// con strcmp: 'total' y 'Total' son dos variables distintas.

// La palabra clave adentro de la linea, como PALABRA ENTERA. NULL si no esta.
char *palabra_en(char *s, const char *kw);

// Si la linea ES exactamente esta palabra clave.
int kw_es(const char *linea, const char *kw);

// Saca la palabra clave del principio y el terminador del final:
// "SI (a = 1) ENTONCES" -> "(a = 1)". NULL si falta el terminador.
char *cuerpo_cabecera(char *linea, const char *kw, const char *fin);

// Si la linea empieza con esta palabra clave, como palabra entera.
int empieza_con(const char *linea, const char *kw);

#endif // PAED_TEXTO_INTERNO_H

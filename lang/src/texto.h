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

#endif // PAED_TEXTO_INTERNO_H

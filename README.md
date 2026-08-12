# PAED

## Que es esto?
    PseudoCodigo para la consola
    
```paed
ACCION promedio ES
    AMBIENTE
        n, i, suma: ENTERO;
    PROCESO
        ESCRIBIR("Cuantos numeros:");
        LEER(n);
        suma := 0;
        PARA i := 1 HASTA n HACER
            LEER(A[i]);
            suma := suma + A[i];
        FIN_PARA
        ESCRIBIR("promedio = ", suma / n);
FIN_ACCION
```
Como ejecutarlo?

```
$ paed promedio.paed
```

## Instalación

```bash
curl -L https://github.com/BrianKalbermatter/paed/releases/latest/download/paed -o paed
chmod +x paed
./paed install
```

El binario **lleva la definición del lenguaje adentro**, así que anda sin nada
al lado, y `install` se copia a sí mismo a `/usr/local` — o a `~/.local` si no
puede escribir ahí. Para elegir destino: `./paed install /opt/paed`.

```bash
paed --version      # que version es
paed --help         # todo lo que sabe hacer
```

**Desde el código**, si querés compilarlo vos:

```bash
make install                      # /usr/local (pide sudo)
make install PREFIX=$HOME/.local  # sin sudo
```

Deja dos cosas: el binario `paed` y el directorio de datos con
`sintaxis.json`. **Los datos no son un extra**: sin ellos el binario no sabe qué
es una palabra clave, porque la definición del lenguaje vive ahí y no en el C.

## Usarlo desde otro programa

```c
#include <paed/parser.h>
#include <paed/interpreter.h>

paed_syntax_load();

PAEDProgram prog;
if (paed_parse_file("mi_programa.paed", &prog) == 0)
    interp_exec(&prog);
```

Y podés **agregarle procedimientos** que no son del lenguaje, con
`paed_register_proc()`.
Nota IMPORTANTE: Aun se estan haciendo pruebas, puede que no funcione muy bien o que no FUNCIONE directamente.

## Licencia

Ver [LICENSE](LICENSE).

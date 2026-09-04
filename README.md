# PAED

## Que es esto?
    PseudoCodigo para la consola
    
```paed
ACCION promedio ES
AMBIENTE
    n, i, suma: ENTERO;
    A: ARREGLO[1..100] DE ENTERO;
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

## Aprenderlo

Si es tu primera vez, el binario trae un tutorial adentro: 12 programas rotos a
propósito, de menos a más. Arreglás uno, el tutor lo corre, y si la salida es la
que el ejercicio pide, pasás al siguiente.

```bash
paed aprender init      # desempaca los ejercicios en ./paed-aprender
paed aprender           # muestra el actual y por qué no pasa
paed aprender --mirar   # igual, pero se recorre solo cada vez que guardás
paed aprender pista     # cuando te trabás
```

No hay archivo de progreso: **el ejercicio actual es el primero que todavía no
pasa**, y eso se calcula corriéndolos. Un archivo de estado sería una segunda
verdad que se puede desincronizar del disco.

## Los archivos de un ejercicio

En el parcial la cátedra te **da** los archivos, cargados y ordenados. Acá los
armás vos, y el `AMBIENTE` del `.paed` es la única fuente de qué columnas y qué
clave tiene cada uno:

```paed
mae: ARCHIVO DE remedio ORDENADO POR farmacia, medicamento;
```

```bash
paed asistente ejercicio.paed   # qué archivos declaraste y de qué tipo es cada uno
paed datos     ejercicio.paed   # cargar las filas, ordenadas por su clave
```

`paed datos` saca de esa declaración las columnas y sus tipos, valida cada valor
al tipearlo (un `ENTERO` no acepta `abc`), trae primero lo que el `.csv` ya
tenía, y escribe todo **ordenado por la clave**. El orden es estable, y los
números se comparan como números: `10` va después de `9`, no antes.

El orden de trabajo que habilita: escribís el `AMBIENTE`, cargás los datos, y
recién ahí el `PROCESO`. Un algoritmo de mezcla o de actualización secuencial
**supone** que la entrada viene ordenada, y depurarlo contra datos desordenados
es perseguir un bug que no está en el código.

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

## Documentación

| Documento | Qué hay adentro |
|---|---|
| [`docs/PAED.md`](docs/PAED.md) | La spec del lenguaje. **Es la única.** |
| [`aprender/`](aprender/) | Los ejercicios del tutorial, y sus soluciones |
| [`KANBAN.md`](KANBAN.md) | Qué está hecho y qué falta |
| [`docs/ARCHIVOS.md`](docs/ARCHIVOS.md) | Organización de archivos, juego de archivos y plan |
| [`docs/ESTRUCTURA.md`](docs/ESTRUCTURA.md) | Qué hay en cada carpeta del repo y por qué |
| [`docs/ESCENA.md`](docs/ESCENA.md) | La librería de escena 3D de VimMon — no es PAED |

## Licencia

Ver [LICENSE](LICENSE).

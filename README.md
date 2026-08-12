# PAED

**PAED es el pseudocódigo AED de la cátedra, ejecutable.** Escribís el mismo
pseudocódigo que entregás en un parcial, y corre.

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

```
$ paed promedio.paed
```

## Instalación

```bash
make install                      # /usr/local (pide sudo)
make install PREFIX=$HOME/.local  # sin sudo
```

Deja dos cosas: el binario `paed` y el directorio de datos con
`sintaxis.json`. **Los datos no son un extra**: sin ellos el binario no sabe qué
es una palabra clave, porque la definición del lenguaje vive ahí y no en el C.

Para desactualizar:

```bash
make uninstall
```

## Cómo encuentra su definición

En este orden, el primer directorio que tenga `sintaxis.json`:

1. `$PAED_HOME` — lo pisa todo. Para probar una definición sin instalarla.
2. La ruta de instalación, compilada adentro del binario por `make install`.
3. `Frankly/data` — corriendo parado en este repo.
4. `paed/Frankly/data` — corriendo parado en un proyecto que lo tenga adentro.

Es la misma idea de `PYTHONHOME`. Sin esto, el binario solo funcionaría desde la
carpeta donde se compiló.

## Qué hay acá adentro

```
lang/          el LENGUAJE: libpaed.a + el binario `paed`. C puro, sin SDL.
  include/paed/  headers publicos
  src/           parser, evaluador de expresiones, interprete
  cli/           el binario
  vendor/cjson/  unica dependencia, adentro
Frankly/       la definicion del lenguaje, su documentacion y sus tests
  data/          sintaxis.json — la fuente de verdad, legible por maquina
  docs/PAED.md   la especificacion
  tests/         programas .paed que declaran su propia salida esperada
  core/          la implementacion en bash (anterior a la de C)
src/           PseudoGames: el editor/IDE con SDL2 (`make` a secas)
```

## Construir

```bash
make lang     # libpaed.a + el binario `paed`   (no necesita SDL)
make          # el editor PseudoGames (`aed`)   (necesita SDL2)
make test     # corre todos los .paed de Frankly/tests
```

## Usarlo desde otro programa

```c
#include <paed/parser.h>
#include <paed/interpreter.h>

paed_syntax_load();

PAEDProgram prog;
if (paed_parse_file("mi_programa.paed", &prog) == 0)
    interp_exec(&prog);
```

Enlazá con `-lpaed`. El intérprete **no** abre `stdin` por su cuenta: quien lo
hospeda decide de dónde salen los datos de `LEER` con `interp_set_entrada()`.
Suena raro hasta que lo corrés adentro de un game loop, donde un `fgets`
bloqueante te congela la ventana entera.

Y podés **agregarle procedimientos** que no son del lenguaje, con
`paed_register_proc()`. Así es como VimMon le suma su escena 3D (`CUBO`, `MOVER`,
`GIRAR`) sin que PAED sepa qué es un cubo. Los procedimientos de AED se resuelven
primero: una librería extiende el lenguaje, no lo redefine.

## Tests

Un test es **un archivo**. Cada `.paed` de `Frankly/tests/` lleva al final los
datos que necesita y la salida que espera:

```paed
FIN_ACCION

// ── ENTRADA ──
// 10
// 45

// ── SALIDA ESPERADA ──
// 1..10 suman 55
```

Agregar un test es dejar el archivo ahí. No hay ninguna lista que mantener, y no
existe un modo que regrabe la salida sola: si un test falla, se lee el diff y se
corrige a mano. Regrabar sin leer es cómo un test deja de proteger.

## Licencia

Ver [LICENSE](LICENSE).

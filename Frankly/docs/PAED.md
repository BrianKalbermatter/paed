# PAED — Especificación del lenguaje v4.0

**PAED es el pseudocódigo AED de la cátedra. Nada más que eso.**

Este es el **único** documento de sintaxis del lenguaje. Absorbió a
`docs/paed_spec.md` (v2.0) el 2026-08-10: si algo sobre PAED no está acá, no
está documentado.

La definición formal legible por máquina está en
[`../data/sintaxis.json`](../data/sintaxis.json): el resaltador de Neovim y el
parser en C la leen en runtime, y `tools/generar.sh` genera el resto desde ahí.

Los procedimientos de escena 3D de VimMon **no son parte del lenguaje**: son una
librería aparte, ver [`ESCENA.md`](ESCENA.md).

---

## 0. Fuentes de verdad — SOLO DOS

La sintaxis de PAED **no se inventa y no se deduce del código existente**.
Únicamente estas dos fuentes definen el lenguaje:

| # | Fuente | Ruta | Por qué |
|---|---|---|---|
| 1 | **Material de cátedra** | `paed/solutions/AED_Teoria/*.pdf` | Oficial, elaborado por los docentes |
| 2 | **La wiki** | `paed/data/wiki.txt`, `paed/data/OnlySintaxis.md` | Escrita por Brian verificando contra la cátedra |

Nada más es autoridad. Punto.

### 0.1 Lo que NO define la sintaxis

Todo lo siguiente es **implementación o notas personales**. Puede contener
errores, y de hecho los contiene:

| Fuente | Estado | Error conocido |
|---|---|---|
| `Frankly/paed` (715 líneas bash) | Implementación | Acepta `PARA ... a ...` y comillas dobles — ninguna es de cátedra |
| `Frankly/DOC.txt` | Notas de aprendizaje | Desactualizado seguido; redirige a este documento |
| `paed/AlgebraRectas/recta.paed` | Ejercicio propio | Sintaxis incorrecta (ver §12.1) |
| `Frankly/data/sintaxis.json` | Lista de keywords | Incompleta |
| `Frankly/syntaxes/paed.tmLanguage.json` | Resaltador | Incompleta, comillas dobles |
| `paed/src/editorText.c` | Resaltador | Incompleta, desincronizada |

**Regla dura:** si una de estas contradice a la cátedra o a la wiki, **la fuente
está mal y se corrige** — no se adapta la spec para acomodarla.

**Regla de trazabilidad:** ninguna construcción entra a esta spec sin cita a
línea de la cátedra o de la wiki.

### 0.2 Los tres estados de cada regla

Este documento distingue tres cosas que es fácil confundir, y confundirlas es
lo que hace que una spec mienta:

| Marca | Significa |
|---|---|
| **cátedra** | Lo dice el material oficial o la wiki. Es autoridad. |
| **decidido** | Se eligió una forma entre varias posibles, con fecha y motivo. Todavía puede no estar implementado. |
| **implementado** | El parser en C lo hace hoy, verificado con `make test`. |

Una regla puede estar **decidida pero no implementada**. Cuando eso pasa, se
dice: hoy el caso vivo es el `;` (§11.1), que la cátedra usa como separador y el
parser exige como terminador.

---

## 1. Estructura de un programa

```paed
ACCION nombre_algoritmo ES
    AMBIENTE
        variable: TIPO;
    PROCESO
        instrucciones;
FIN_ACCION
```

- Palabras clave en MAYÚSCULAS, identificadores en minúsculas.
- Comentarios con `//` hasta el fin de línea.
- Declaraciones e instrucciones terminan en `;`.
- Las palabras de bloque (`SI`, `MIENTRAS`, `FIN_SI`, …) **no** llevan `;`.
- El `AMBIENTE` es opcional: un programa que solo usa escalares puede no tenerlo.
- El cierre se escribe `FIN_ACCION` **o** `FINACCION`, las dos válidas (§10.7).

## 2. Tipos de datos

| Tipo | Ejemplo en tus apuntes | Estado |
|---|---|---|
| `ENTERO` | `cont_pal: ENTERO;` | implementado |
| `REAL` | `tini: REAL;` | implementado |
| `BOOLEANO` | | implementado |
| `CARACTER` / `CARACTERES` | `SECUENCIA DE CARACTERES` | implementado |
| `AN(n)` | `a: AN(5);` | implementado |
| `N(n)` | | implementado |
| `ARREGLO[d..h] DE <tipo>` | `A: ARREGLO[1..10] DE ENTERO;` | implementado |
| `SECUENCIA DE <tipo>` | `sec1: SECUENCIA DE CARACTER;` | ❌ |
| `VENTANA DE <tipo>` | `recorrido: VENTANA DE CARACTER;` | ❌ |
| `SECUENCIA DE SALIDA` | `secSalida: SECUENCIA DE SALIDA;` | ❌ |
| `REGISTRO` / `FIN_REGISTRO` | `vector2 = REGISTRO ... FIN_REGISTRO` | implementado |
| `ARCHIVO DE <tipo>` | `arch: ARCHIVO DE venta;` | se declara y se valida; no lee disco |
| `CONSTANTE`, `PUNTERO` | | ❌ |

El tipo se guarda como texto y **no se valida**: hoy nada impide asignarle un
texto a algo declarado `ENTERO`. Los tipos escalares se aceptan todos por igual.

### 2.1 Arreglos

Los límites los elige el programador y **no arrancan en 0**: `ARREGLO[1..10]`
va del 1 al 10, y `ARREGLO[5..9]` del 5 al 9.

```paed
AMBIENTE
    A: ARREGLO[1..10] DE ENTERO;
```

El índice es una **expresión completa**, no solo un número, así que `A[i]`,
`A[i + 1]` y `A[(izq + der) DIV 2]` funcionan sin ningún caso especial. Se
evalúa recién al ejecutar: en `A[i] := 0` dentro de un bucle, `i` vale distinto
en cada vuelta.

**Los límites se chequean en cada acceso:**

```
indice 4 fuera de rango: 'A' va de 5 a 9
```

Esto es lo que hace útil declarar el rango. En C, `A[99]` sobre un arreglo de
10 escribe en memoria ajena y el programa sigue como si nada hasta reventar en
otro lado sin relación aparente.

Los elementos arrancan en 0, no en basura: leer `A[3]` antes de cargarlo da algo
previsible.

### 2.2 Registros

Es el `struct` de C con otro nombre.

```paed
AMBIENTE
    vector2 = REGISTRO
        vx: REAL;
        vy: REAL;
    FIN_REGISTRO

    pori: vector2;      // una VARIABLE de ese tipo
```

`vector2` es un **tipo**; `pori` es una **variable**; `pori.vx` es un **campo**
de esa variable. El `.` se lee "de adentro de": `pori.vx` es "el vx de pori".

Sin registros, un punto necesita dos variables sueltas (`pori_x`, `pori_y`) y
nada las une. Con registro, `pori` es **una** cosa que tiene dos partes. En un
programa de geometría con cuatro puntos, la diferencia es entre 8 variables
sueltas y 4 objetos con sentido.

Un campo vale en cualquier lado donde valga una variable: como destino
(`pori.vx := 0`) y dentro de expresiones (`pori.vx ** 2 + pori.vy ** 2`).

**Por dentro se aplana.** `pori` de tipo `vector2` se guarda como dos variables
llamadas `"pori.vx"` y `"pori.vy"`. El entorno no sabe nada de registros: para
él son dos variables comunes que tienen un punto en el nombre. El precio de esa
simplicidad es que no se puede asignar un registro entero (`p1 := p2`), que no
aparece en el corpus.

**Un campo no nace al asignarlo**, al revés que un escalar: se crean todos al
declarar la variable. Un campo que el registro no declara se rechaza:

```
'p' no tiene un campo 'vz'
```

Sin ese chequeo el registro no serviría de nada — admitiría cualquier campo
inventado, y declarar el tipo no impediría absolutamente nada.

**El punto decimal no se confunde con el de campo.** `1.5` es un número y
`pori.vx` es un campo. Se resuelve mirando qué viene después del punto: un
dígito lo hace parte del número, una letra lo hace acceso a campo. Un número
nunca llega al lector de identificadores, porque empieza con dígito.

### 2.3 Archivos y las dos formas de `LEER`

```paed
AMBIENTE
    arch: ARCHIVO DE venta;    // el tipo de adentro es el REGISTRO que guarda
```

`LEER` significa **dos cosas distintas** que se escriben igual:

| Forma | Qué hace |
|---|---|
| `LEER(salario)` · `LEER(A,B)` | pide datos por consola |
| `LEER(arch, reg)` | lee el próximo registro y **avanza** |

La teoría lo pone en la misma fila que `Avanzar(Sec, v)`
(`TEORIA_COMPLETA.txt:1107`): **`Leer(Arch, Reg)` es el avanzar de los
archivos**, no una entrada de usuario. `ESCRIBIR` tiene el mismo doble rol
(`ESCRIBIR(texto)` vs `ESCRIBIR(arch, reg)`).

**No se distinguen contando argumentos.** En `wiki.txt:2761` y `:2765`, dentro
del mismo algoritmo:

```paed
LEER(clave, cod_mov)      // consola, 2 argumentos
LEER(arch_mae, reg_mae)   // archivo, 2 argumentos
```

**Lo único que las separa es la declaración.** Si el primer argumento se declaró
`ARCHIVO DE X`, es operación de archivo; si no, es consola. Lo resuelve el
**parser**, mirando el `AMBIENTE`, antes de ejecutar — y se decide por
instrucción, así que un programa puede tener tres archivos arriba y diez `LEER`
de consola abajo sin que se pisen.

Un `REGISTRO` **no puede** tener un `ARCHIVO` adentro: el registro es el molde
de lo que se lee o se graba, y vive en memoria.

**El archivo sin declarar lo caza `ABRIR`, no `LEER`.** `LEER` con un primer
argumento desconocido degrada a consola a propósito, porque un escalar no
necesita declararse. `ABRIR` sí exige que el archivo exista, y ese es el que
avisa.

## 3. Asignación y operadores

```paed
cont_pal := 0;
cont_pal := cont_pal + 1;
A[i] := A[i + 1];
```

Corroborado contra `TEORIA_COMPLETA.txt:307-371`:

| Grupo | Operadores |
|---|---|
| Asignación | `:=` |
| Aritméticos | `+` `-` `*` `/` `MOD` `DIV` `**` |
| Relacionales | `=` `<>` `<` `<=` `>` `>=` |
| Lógicos | `Y` (`AND`) · `O` (`OR`) · `NO` |
| Literales lógicos | `V` `F` (también `VERDADERO` / `FALSO`) |

`MOD` y `DIV` son operadores **infijos** (`a MOD b`), no funciones.
`**` es la potencia — **no** `^`.

**`==` no existe en AED.** La igualdad es `=` (`TEORIA_COMPLETA.txt:324`). Tus
`.paed` lo usan 91 veces: es un error de escritura arrastrado, no del lenguaje.
El parser lo acepta como sinónimo para no romperte los archivos, pero debería
avisar (pendiente en el KANBAN).

### 3.1 Precedencia

De **menor** a **mayor** ligadura. Esta tabla no es decorativa: es literalmente
el orden en que se llaman las funciones de `plugins/ide/expr.c`.

| Prioridad | Operadores | Función en `expr.c` |
|---|---|---|
| 9 (liga menos) | `O` `OR` | `eval_o` |
| 8 | `Y` `AND` | `eval_y` |
| 7 | `=` `<>` | `igualdad` |
| 6 | `<` `<=` `>` `>=` | `relacional` |
| 4 y 5 | `+` `-` y concatenación | `suma` |
| 3 | `*` `/` `DIV` `MOD` | `producto` |
| 2 | `**` | `potencia` |
| 1 (liga más) | `+` `-` `NO` unarios | `unario` |
| — | literales, variables, funciones, `( )`, `A[i]` | `primario` |

**No hay una tabla de números en el código.** La prioridad *es* el orden de las
llamadas: cuando `suma()` pide su operando derecho llama a `producto()`, que ya
se comió el `3 * 4`. Por eso `2 + 3 * 4` da 14 y no 20.

Tres consecuencias que conviene tener presentes:

| Expresión | Da | Por qué |
|---|---|---|
| `2 + 3 * 4` | `14` | el producto liga más que la suma |
| `2 ** 3 ** 2` | `512` | la potencia es asociativa a **derecha**: `2**(3**2)` |
| `-2 ** 2` | `4` | por la tabla de AED el unario liga **más** que `**` |

El último es el raro: en casi todos los lenguajes `-2 ** 2` da `-4`, porque la
potencia liga más fuerte que el signo. Acá se siguió la tabla de la cátedra.
**Pendiente de confirmar con los docentes** (anotado en el KANBAN).

### 3.2 Cortocircuito en `Y` y `O`

`TEORIA_COMPLETA.txt` lo dice textual:

> *"En AND, si el primer operando es Falso, el segundo no se evalúa."*

Eso **cambia el comportamiento**, no solo la velocidad. El caso que lo
justifica es la búsqueda lineal:

```paed
MIENTRAS (i <= n) Y (A[i] <> buscado) HACER
```

Cuando el elemento no está, `i` llega a `n + 1`. Sin cortocircuito se evaluaría
`A[n+1]`, fuera de rango, y el programa cortaría con un error en vez de
contestar "no está".

### 3.3 Tipos en las comparaciones

Las comparaciones entre textos usan **ASCII**, como manda la teoría: `'A' < 'K'`
es verdadero y `'MARIA' < 'JUAN'` es falso.

El `+` también concatena (prioridad 5 de la tabla). Se decide por el tipo: si
alguno de los dos lados es texto, se pegan.

## 4. Estructuras de control

```paed
SI (condicion) ENTONCES
    ...
SINO
    ...
FIN_SI

MIENTRAS (condicion) HACER
    ...
FIN_MIENTRAS

PARA i := 1 HASTA n HACER
    ...
FIN_PARA

PARA i := 5 HASTA 1; -1 HACER      // paso opcional, acá en reversa
    ...
FIN_PARA
```

Reservadas, **no implementadas**:

```paed
REPETIR
    ...
HASTA (condicion)

SEGUN variable HACER
    valor: ...;
FIN_SEGUN
```

### 4.1 El paso del `PARA`

El paso es **opcional** y por defecto 1, corroborado en
`TEORIA_COMPLETA.txt:565-571` (*"Si el incremento es distinto de 1, debe
indicarse"*). En reversa se usa **paso negativo**, no una palabra tipo `downto`.

El corte usa el signo del paso: con paso positivo termina al pasarse del final,
con paso negativo al bajar de él. Sin eso, un `PARA` en reversa no terminaría
nunca.

### 4.2 Anidamiento

Las estructuras de control usan una **pila** de bloques, no una máquina de
estados plana: una variable sola no puede representar un CAMINO de anidamiento,
y es pila porque los bloques cierran en el orden inverso al que se abren.

Un `PARA` inicializa su variable la primera vez que se entra, pero no en cada
vuelta, y el `FIN_PARA` salta de vuelta al `PARA`. Por eso la marca de "ya
arrancó" es **por instrucción** y no una sola bandera: con una sola, dos `PARA`
anidados se pisarían y el interno arrancaría una única vez.

## 5. Procedimientos y funciones

Las llamadas en AED son **posicionales**:

```paed
ESCRIBIR("La cantidad de palabras es:", cont_pal);
LEER(variable);
ARR(secCaracter);
AVZ(secCaracter, venCaracter);
CREAR(secSalida);
CERRAR(sec1, recorrido);
```

De estos, hoy ejecutan **`ESCRIBIR` y `LEER` de consola**. El resto parsea pero
no hace nada: `ARR`/`AVZ`/`CREAR`/`CERRAR` y el `LEER` de archivo están en el
KANBAN.

**El intérprete no trae nada más que el lenguaje.** Todo lo que no sea AED lo
agrega quien lo hospeda, registrándolo con `paed_register_proc`. Así es como
VimMon le suma su escena 3D sin que el lenguaje sepa qué es un cubo — ver
[`ESCENA.md`](ESCENA.md). Los procedimientos de AED se resuelven primero, así
que una librería del host no puede redefinir `LEER`.

`ESCRIBIR` **evalúa** sus argumentos: `ESCRIBIR(cont_pal)` imprime el valor, no
el nombre.

### 5.1 `LEER` de consola

```paed
LEER(edad);              // una variable
LEER(a, b);              // dos destinos: consume DOS datos
LEER(A[i]);              // elemento de arreglo, el índice se evalúa al leer
LEER(p.vx, p.vy);        // campos de un registro
```

**Un dato por destino, uno por línea.** No se parte por espacios: así un texto
con espacios (`Juan Perez`) entra entero en un solo destino.

**El tipo lo decide el dato, no la declaración.** Si la línea entera es un
número, es número; si no, es texto. `12abc` es el texto `12abc`, no el número
12 — un número a medias escondería el error de quien cargó el dato. Esto es
consecuencia de que el `AMBIENTE` todavía no se use para chequear tipos (está
en el KANBAN); cuando se use, el tipo declarado va a mandar.

**De dónde salen los datos lo decide el host, no el intérprete.** El intérprete
corre dentro del game loop del renderer, y un `fgets` bloqueante ahí congelaría
la ventana entera. Por eso hay un puerto — `interp_set_entrada` — que el host
engancha: `paedrun` engancha `stdin`, y la ventana SDL todavía no engancha nada,
así que ahí `LEER` falla con un mensaje claro en vez de colgarse.

**Errores que avisan** (`tests/leer_errores.paed` los fija con su texto exacto):
destino que no es un destino (`LEER("hola")`), `LEER()` sin destinos, índice
fuera de los límites, campo que el registro no declara, y entrada agotada.

Funciones usadas dentro de expresiones:

| Función | Estado |
|---|---|
| `TRUNC`, `ABSO`, `REDOND` | implementadas |
| `MOD`, `DIV` | son operadores infijos, no funciones |
| `NFDS`, `FDS` | ❌ necesitan secuencias; avisan en vez de inventar un valor |

Definidas por el usuario, dentro de `AMBIENTE` — **no implementadas**:

```paed
FUNCION car_a_num(c: caracter) ES: ENTERO
    PROCESO
        ...
FIN_FUNCION
```

## 6. Inventario de tokens

Cada grupo lleva la cita que le da autoridad.

### 6.1 Estructura

`ACCION` `ES` `FIN_ACCION` `AMBIENTE` `PROCESO`
`FUNCION` `FIN_FUNCION` `PROCEDIMIENTO` `FIN_PROCEDIMIENTO`
`SUBACCION` `FIN_SUBACCION` `RETORNAR`

> `OnlySintaxis.md:9-14` (ACCION/AMBIENTE/PROCESO), `:58-64` (FUNCION),
> `:74-79` (PROCEDIMIENTO), `:177-183` (SUBACCION)

### 6.2 Control de flujo

`SI` `ENTONCES` `SINO` `FIN_SI`
`MIENTRAS` `HACER` `FIN_MIENTRAS`
`PARA` `FIN_PARA`
`REPETIR` `HASTA`
`SEGUN`

> `OnlySintaxis.md:189-199` (SI anidado), `:164-167` (MIENTRAS), `:124-127` (PARA)

### 6.3 Tipos

`ENTERO` `REAL` `CARACTER` `BOOLEANO`
`AN` `N`
`ARREGLO` `DE` `SECUENCIA` `VENTANA` `CONSTANTE` `VAR`
`REGISTRO` `FIN_REGISTRO`
`ARCHIVO` `PUNTERO`

> `OnlySintaxis.md:19-24` (escalares, `AN(20)`, `N(5)`), `:100-103`
> (REGISTRO/FIN_REGISTRO), `:121` (arreglo), `:143` (archivo), `:502` (puntero),
> `wiki.txt:750-751` (SECUENCIA DE CARACTERES, VENTANA DE CARACTER)

### 6.4 Operadores lógicos

`Y` `O` `NO`

> `OnlySintaxis.md:227` (`NFDA(arch1) Y NFDA(arch2)`), `:260` (`O`),
> `:427` (`MIENTRAS NO Bandera`)

### 6.5 Operadores aritméticos con nombre

`MOD` `DIV` `TRUNC` `ABSO` `REDOND`

> `OnlySintaxis.md:379` (`(iz + de) DIV 2`)

### 6.6 Entrada/salida y archivos

`LEER` `ESCRIBIR` `ABRIR` `CERRAR` `BORRAR`
`FDA` `NFDA` `FDS` `NFDS` `AVZ` `ARR`
`NUEVO` `DISPONER`

> `OnlySintaxis.md:46,48` (LEER/ESCRIBIR consola), `:148-158` (ABRIR/CERRAR),
> `:341` (BORRAR), `:508-510` (NUEVO/DISPONER)

**`FDA` y `FDS` NO son lo mismo**, y el resaltador actual los confunde:

| Token | Significado | Fuente |
|---|---|---|
| `FDA` / `NFDA` | Fin De **Archivo** | `wiki.txt:1905-1907` |
| `FDS` / `NFDS` | Fin De **Secuencia** | `wiki.txt:1003,1009` |

### 6.7 Literales

| Token | Forma | Fuente |
|---|---|---|
| `NUMERO` | `23`, `1.5`, `-4` | `OnlySintaxis.md:29` |
| `CADENA` | `'Hola'` (comilla simple, ver §10.4) | `AED_2021_UnI.pdf:10` |
| `CARACTER_LIT` | `'M'`, `'A'` | `OnlySintaxis.md:337,371` |
| `VERDADERO` | `Verdadero`, `V` | `OnlySintaxis.md:426`, `wiki.txt:131` |
| `FALSO` | `Falso`, `F` | `OnlySintaxis.md:428` |
| `NIL` | `nil` | `OnlySintaxis.md:503` — ❌ no implementado |
| `IDENTIFICADOR` | `pori`, `t_label_x`, `a` | `OnlySintaxis.md:41` |

### 6.8 Operadores y delimitadores

```
Asignación     :=                        OnlySintaxis.md:29
Comparación    =   <>   <   <=   >   >=  OnlySintaxis.md:460, :189, :380
Aritméticos    +   -   *   /             OnlySintaxis.md:30-31, :463
Puntero        *                         OnlySintaxis.md:514  (prefijo, deref)
Delimitadores  (  )  [  ]  ,  ;  :  .    OnlySintaxis.md:125, :114
Rango          ..                        OnlySintaxis.md:121  (arreglo[1..30])
```

## 7. Reglas léxicas

- **Comentarios:** `//` hasta fin de línea (`OnlySintaxis.md:36`). El PAED
  declarativo v1 usaba `#`; ya no es válido.
- **Identificadores:** letra o `_` inicial, luego letras, dígitos o `_`.
  Ejemplos reales: `t_label_x`, `mi_primer_algoritmo` (`OnlySintaxis.md:41`).
- **Números:** `[0-9]+` con parte decimal opcional `.` `[0-9]+`.
- **Cadenas:** `'...'` o `"..."`. Sin escapes — la wiki no los usa.
- **Caracteres:** `'` un carácter `'`. Misma forma que la cadena: se distinguen
  por el largo, no por la comilla.
- **UTF-8:** todas las keywords son ASCII puro. Los caracteres no-ASCII (`ñ`,
  tildes) solo pueden aparecer dentro de cadenas y comentarios.
- **Espacios y saltos de línea:** no significativos. La indentación es estética.
- **Keywords:** case-insensitive (§10.1). Los identificadores no.

## 8. Gramática

Solo las construcciones en alcance. Las diferidas no figuran.

```ebnf
programa    = accion ;
accion      = "ACCION" IDENT "ES" [ ambiente ] proceso "FIN_ACCION" ;

ambiente    = "AMBIENTE" { declaracion | registro } ;
registro    = IDENT "=" "REGISTRO" { declaracion } "FIN_REGISTRO" ;
declaracion = IDENT ":" tipo ";" ;

tipo        = "ENTERO" | "REAL" | "CARACTER" | "BOOLEANO"
            | "AN" "(" NUMERO ")"
            | "N"  "(" NUMERO ")"
            | "ARREGLO" "[" NUMERO ".." NUMERO "]" "DE" tipo
            | IDENT ;

proceso     = "PROCESO" { sentencia } ;
sentencia   = asignacion | si | mientras | para | llamada ;

asignacion  = lvalue ":=" expr ";" ;
lvalue      = IDENT ( [ "[" expr "]" ] | { "." IDENT } ) ;

si          = "SI" expr "ENTONCES" { sentencia }
              [ "SINO" { sentencia } ] "FIN_SI" ;
mientras    = "MIENTRAS" expr "HACER" { sentencia } "FIN_MIENTRAS" ;
para        = "PARA" IDENT ":=" expr "HASTA" expr [ ";" expr ] "HACER"
              { sentencia } "FIN_PARA" ;

llamada     = IDENT "(" [ arg { "," arg } ] ")" ";" ;
arg         = expr | IDENT "=" expr ;
```

Diferencias con la gramática de la v2.0, que documentaba intención y no
implementación:

- `declaracion` acepta **un solo** identificador. `A,B,SUMA: entero` del
  `AED_2021_UnI.pdf:10` **no parsea** (ver §11.2).
- El `;` es **terminador obligatorio**, no separador opcional (ver §11.1).
- `ambiente` no acepta `VARIABLES` (ver §11.2).

## 9. Errores

El parser **nunca ignora en silencio**. Formato clang:

```
archivo.paed:12: error: falta ';' al final de la instruccion
```

Si hay al menos un error, **no se ejecuta nada**. Análisis primero, ejecución
después. Los errores de runtime usan el mismo formato `archivo:línea`.

Hay una guarda de bucle infinito de 2.000.000 de pasos. No es opcional: el
intérprete corre dentro del game loop, así que un programa colgado cuelga la
ventana entera y hay que matar el proceso sin saber por qué.

## 10. Decisiones de diseño

Ambigüedades del lenguaje, resueltas. **Sujetas a revisión contra la cátedra.**

### 10.1 Sensibilidad a mayúsculas — RESUELTO 2026-08-10

La wiki usa `ARREGLO` (`wiki.txt:1791`) y `arreglo[` (`wiki.txt:1798`) para lo
mismo. También `puntero a ENTERO` (`OnlySintaxis.md:502`) y `archivo de TIPO`
(`:143`) en minúscula. Y el único ejemplo con autoridad de cátedra
(`AED_2021_UnI.pdf:10`) declara los tipos en minúscula: `A,B,SUMA: entero`.

Obligar a una sola forma sería inventar una regla que las fuentes no tienen.

**Decisión, ya implementada:** las **palabras clave** no distinguen mayúsculas.
Los **identificadores** sí.

| Qué | Distingue mayúsculas | Ejemplo |
|---|---|---|
| Palabras clave y tipos | **no** | `MIENTRAS` = `mientras` = `MiEnTrAs` |
| Nombres de procedimiento | **no** | `ESCRIBIR` = `escribir` |
| Claves de parámetro | **no** | `nombre =` = `NOMBRE =` |
| Operadores con nombre | **no** | `Y` `O` `NO` `DIV` `MOD` |
| **Nombres de variable** | **sí** | `total` y `Total` son **dos** variables |
| **Nombres de entidad de escena** | **sí** | `nave` y `Nave` son **dos** cuerpos |

La regla en una línea: **lo que define el lenguaje no distingue; lo que nombrás
vos, sí.**

Antes de esto el parser era case-sensitive pero `expr.c` no, así que
`MIENTRAS (a Y b)` andaba y `mientras (a Y b)` no. La inconsistencia estaba en
que cada archivo había elegido por su cuenta.

### 10.2 El separador del `PARA` es `HASTA` — RESUELTO 2026-08-07

```paed
PARA i := 1 HASTA n HACER
```

Es la forma de la cátedra (10 apariciones en los apuntes, cero de `a`) y
**elimina el problema de raíz**: `HASTA` es keyword normal y nadie llama `hasta`
a una variable.

La alternativa descartada (`a`, como en `recta.paed:47`) habría obligado a
implementar **keyword contextual**: emitir `a` como identificador y hacer que el
parser la aceptara por posición, porque `a: ENTERO;` es una declaración válida.

`HASTA` cumple dos roles sin ambigüedad:

| Rol | Forma | Posición |
|---|---|---|
| Separador de rango | `PARA i := 1 HASTA n HACER` | dentro del encabezado del `PARA` |
| Cierre de ciclo post-test | `REPETIR ... HASTA cond` | a nivel de sentencia |

Las posiciones gramaticales son distintas, así que un parser de descenso
recursivo las distingue sin lookahead extra.

### 10.3 `AN(20)` y `N(5)` son tipos, no llamadas

`OnlySintaxis.md:22-23`. El paréntesis pertenece al **tipo**, no a una
invocación. La desambiguación es del parser: en PAED un tipo nunca aparece donde
se espera una expresión.

### 10.4 Comillas simples — RESUELTO por la cátedra

`AED_2021_UnI.pdf:10` escribe `ESCRIBIR('Ingrese 2 números')`. La comilla simple
es cadena, no solo carácter.

**Estado real:** el evaluador acepta las dos formas y no distingue cadena de
carácter — se diferencian por el largo. Frankly acepta comillas dobles, que no
son de cátedra.

### 10.5 El `=` tiene tres roles

| Rol | Ejemplo | Fuente |
|---|---|---|
| Comparación | `SI n = 0 ENTONCES` | `OnlySintaxis.md:460` |
| Constante | `MAX = 100;` | `OnlySintaxis.md:24` |
| Tipo registro | `Nodo = REGISTRO` | `OnlySintaxis.md:100` |

**Decisión:** un solo token. El parser desambigua por contexto — dentro de
`AMBIENTE` es declaración, dentro de una expresión es comparación.

Hoy solo existe el rol de comparación; los otros dos llegan con `CONSTANTE` y
`REGISTRO`.

### 10.6 El `.` decimal contra el `.` de campo

`1.5` es un número; `pori.vx` es acceso a campo. El mismo carácter.

**Decisión:** se resuelve dentro del escaneo del número. Si ya venía consumiendo
dígitos y encuentra `.` **seguido de un dígito**, lo absorbe como parte decimal.
En cualquier otro caso `.` es un token propio.

Caso a testear cuando lleguen los registros: `arreglo[1..30]` — tras el `1`
viene `.` y después otro `.`, que no es dígito, así que el número corta.

### 10.7 El cierre de la `ACCION` — RESUELTO 2026-08-10

Era el único punto **bloqueante** que le quedaba a la spec. Las fuentes se
contradecían:

| Fuente | Escribe |
|---|---|
| `AED_2021_UnI.pdf:10` (cátedra) | `FIN ACCION` — **con espacio** |
| `OnlySintaxis.md:14` (wiki) | `FIN_ACCION` — con guión bajo |

**Decisión: se aceptan `FIN_ACCION` y `FINACCION`.** Las dos son una sola
palabra, así que cuestan un `strcmp` y ningún lookahead.

`FACCION` **se rechaza**: se entiende la intención, pero abreviar `FIN` a `F`
deja el cierre incompleto.

La forma de la cátedra (`FIN ACCION`, partida en dos) **queda afuera**. Es la
única que cuesta código: partida en dos palabras obliga a mirar la siguiente
antes de decidir si es un cierre o el principio de otra cosa.

Las tres formas rechazadas igual **cierran el bloque** después de reportar el
motivo. Si no lo hicieran, el `PROCESO` quedaría abierto y cada línea siguiente
sumaría un error más, enterrando el verdadero.

`FIN ACCION` con espacio tiene mensaje propio, aunque no se acepte:

```
el apunte escribe 'FIN ACCION' con espacio, pero en PAED el cierre es una
sola palabra: FIN_ACCION o FINACCION
```

Es la forma que uno copia del apunte sin pensar. Sin ese caso el error sería
"falta `;`", que manda a buscar el problema al lugar equivocado.

## 11. Pendiente de revisión con la cátedra

### 11.1 El `;`: ¿separador o terminador?

| Fuente | Dice |
|---|---|
| `AED_2021_UnI.pdf:10` | **separador** — la última sentencia no lo lleva |
| Implementación | **terminador obligatorio** en toda instrucción |

La cátedra gana en el papel, pero cambiarlo ahora rompe todos los `.paed` del
repo. Sin decidir.

### 11.2 Puntos abiertos del ejemplo de cátedra

El cierre de bloque **ya no está acá**: se decidió el 2026-08-10, ver §10.7.

Del ejemplo de `AED_2021_UnI.pdf:10` quedan dos puntos abiertos, ninguno
implementado:

- **`VARIABLES` como sub-sección de `AMBIENTE`**. Keyword ausente de
  `OnlySintaxis.md` y del resaltador. ¿Es obligatoria?
- **Declaración múltiple `A,B,SUMA: entero`**. La gramática de §8 admite un solo
  identificador por declaración.

### 11.3 Abiertos, sin bloquear

| Punto | Evidencia |
|---|---|
| `ES` con dos significados: cierre de `ACCION` y modo de parámetro | `OnlySintaxis.md:9` vs `:91` |
| `..` de rango: ¿token propio o dos `PUNTO`? | `OnlySintaxis.md:121` |
| `*p.Dato`: ¿`(*p).Dato` o `*(p.Dato)`? | `OnlySintaxis.md:546` |
| `EN` sin definición — probablemente prosa | `OnlySintaxis.md:331` |
| `-2 ** 2` da 4 por la tabla de prioridad. En casi todos los lenguajes da -4 | `TEORIA_COMPLETA.txt:361-371` |
| Menos unario sin fuente confirmada en una expresión real | `OnlySintaxis.md:19` solo lo menciona en prosa |
| `RETORNAR`, `TRUNC`, `ABSO`, `REDOND` | 0 apariciones en los apuntes |

### 11.4 Evidencia de primera mano de la cátedra

`AED_2021_UnI.pdf`, página 10 — ejemplo canónico publicado por los docentes:

```
ACCION SUMA ES
AMBIENTE
    VARIABLES
        A,B,SUMA: entero
PROCESO
    ESCRIBIR('Ingrese 2 números');
    LEER(A,B);
    SUMA := A + B;
    ESCRIBIR('El resultado es', SUMA)
FIN ACCION
```

Seis hechos que establece, y que ninguna otra fuente del repo registraba:

| # | Hecho | Estado |
|---|---|---|
| A | `FIN ACCION` con **espacio** | abierto, §11.2 |
| B | `VARIABLES` es sub-sección de `AMBIENTE` | abierto, §11.2 |
| C | Declaración múltiple `A,B,SUMA: entero` | abierto, §11.2 |
| D | Los tipos van en minúscula (`entero`) | refuerza §10.1 |
| E | Las cadenas usan **comilla simple** | resuelto, §10.4 |
| F | La última sentencia no lleva `;` | abierto, §11.1 |

> El PDF es una captura de Sublime Text dentro del material, no un BNF formal.
> Es la mejor evidencia disponible, pero conviene contrastarla con
> `AED_2018_UnI_B.pdf` y las resoluciones de parcial antes de congelar.

Material de la cátedra todavía sin explotar, que puede cerrar los puntos
abiertos: los 6 PDF de `paed/solutions/AED_Teoria/`, y las resoluciones de
parcial de `paed/solutions/`.

## 12. Estado real del parser

Esto es lo que `plugins/ide/parser.c` entiende **hoy**, verificado con
`make test`. El resto se reconoce y se reporta como no implementado, con número
de línea — nunca se ignora.

| Construcción | Estado |
|---|---|
| `ACCION` / `AMBIENTE` / `PROCESO` | ✅ |
| Cierre `FIN_ACCION` y `FINACCION` | ✅ §10.7 |
| `FACCION` y `FIN ACCION` rechazados, con mensaje propio | ✅ §10.7 |
| Declaraciones `nombre: TIPO;` | ✅ (el tipo se guarda como texto, no se valida) |
| Comentarios `//`, incluso dentro de strings | ✅ |
| Llamadas posicionales `PROC(a, b);` | ✅ |
| `;` obligatorio, error con línea | ✅ |
| Asignación `:=` | ✅ |
| `SI` / `SINO` / `FIN_SI`, anidados | ✅ |
| `MIENTRAS` / `FIN_MIENTRAS`, anidados | ✅ |
| `PARA ... HASTA ... [; paso] HACER`, incluso en reversa | ✅ |
| Expresiones y operadores de comparación | ✅ |
| `ARREGLO[desde..hasta] DE <tipo>`, en expresión y como destino | ✅ |
| `ESCRIBIR` que evalúa sus argumentos | ✅ |
| Keywords en minúscula o mezcladas (`accion`, `MiEnTrAs`) | ✅ §10.1 |
| Declaración múltiple `A,B: ENTERO` | ❌ §11.2 |
| `VARIABLES` dentro de `AMBIENTE` | ❌ §11.2 |
| `FIN ACCION` con espacio | ❌ a propósito, §10.7 |
| `;` omitido en la última sentencia | ❌ §11.1 |
| Nombre de `ACCION` con espacios | ❌ |
| `REPETIR` / `SEGUN` | ❌ |
| `FUNCION` / `PROCEDIMIENTO` anidados en `AMBIENTE` | ❌ |
| `REGISTRO` / `FIN_REGISTRO` y acceso `pori.vx` | ✅ §2.2 |
| Un campo que el registro no declara | ❌ rechazado, §2.2 |
| Instrucción partida en dos líneas | ❌ el parser lee línea por línea |
| `ARCHIVO` / `CONSTANTE` | ❌ |
| `SECUENCIA`, y por lo tanto `NFDS` / `FDS` | ❌ |
| `ARCHIVO DE <tipo>` en el AMBIENTE | ✅ §2.3 |
| `LEER`/`ESCRIBIR`: consola vs archivo, distinguidos | ✅ §2.3 |
| `FDA` / `NFDA` reconocidas (avisan que faltan archivos) | ✅ |
| `LEER` de consola: escalar, `A[i]` y `p.campo` | ✅ §5.1 |
| `ARR` / `AVZ` / `CREAR` / `CERRAR` / `ABRIR` / `LEER` de archivo | parsean, no ejecutan |

Lo que falta está en el KANBAN con su ticket. Esta tabla y el KANBAN se
actualizan juntos.

## 13. Cómo se prueba

`build/paedrun` corre un `.paed` en la terminal, sin abrir la ventana:

```bash
make paedrun
build/paedrun paed/Frankly/tests/busqueda_binaria.paed
```

El intérprete vive dentro del game loop, así que antes la única forma de probar
el lenguaje era abrir la ventana SDL y mirar — y **un test que hay que mirar no
es un test**.

```bash
make test    # corre todos y compara
```

**Un test es UN archivo.** Cada `.paed` declara su propia salida al final, en un
bloque de comentarios:

```paed
FIN_ACCION

// ── SALIDA ESPERADA ──
// 1..10 suman 55
// Gauss tenia razon
```

El runner lo extrae y compara contra lo que el programa imprime de verdad —
incluidos los errores, que salen por `stderr`. Agregar un caso es dejar el
`.paed` con su bloque: no hay lista que mantener ni archivo espejo.

**No hay ningún modo que regrabe la salida sola.** Si un test falla, el runner
muestra el diff y el bloque se corrige **a mano**, leyéndolo. Regrabar sin leer
es exactamente cómo un test deja de proteger: "arregla" el test en vez del bug.

### 13.1 El corpus

| Programa | Qué ejercita |
|---|---|
| `busqueda_lineal.paed` | recorrido con corte, y el caso que justifica el cortocircuito |
| `busqueda_binaria.paed` | `DIV`, aritmética de índices, rango que se cierra |
| `ordenamiento_burbuja.paed` | `PARA` anidado, `A[j+1]` como destino |
| `expresiones.paed` | Euclides, primos, Fibonacci, factorial, trampas de prioridad |
| `arreglo_limites.paed` | índice fuera de rango — **termina con error a propósito** |
| `errores.paed` | que el parser nunca ignore en silencio |
| `hola_mundo.paed` | concatenación y `AN(n)` |

**Regla de oro:** ninguna construcción entra a esta spec sin un `.paed` que la
ejercite y corra. La v1 se rompió justamente por eso —
`_void/docs/paed_spec_v1.md:29` documentaba `mover <id> a=<x,y,z>` mientras el
intérprete leía `nombre=` y `posicion=`, y `escalar`/`girar`/`oscilar` quedaron
documentados pero nunca implementados.

### 13.2 Los `.paed` viejos no son corpus

Fueron escritos contra Frankly, no contra la cátedra. Frankly es permisivo:
que un archivo corra ahí **no lo hace correcto**.

`paed/AlgebraRectas/recta.paed` corre en Frankly e imprime la salida completa
sin errores, y aun así tiene sintaxis inválida:

| Línea | Escrito | Correcto | Fuente |
|---|---|---|---|
| `:47` | `PARA coord := -4 a 4 HACER` | `PARA coord := -4 HASTA 4 HACER` | wiki + cátedra |
| `:29-37` | `t_eje_x := "EJE_X";` | `'EJE_X'` | `AED_2021_UnI.pdf:10` |

> Por qué Frankly lo acepta: `paed:437-439` lee el `PARA` **por posición**
> (`$2`, `$4`, `$6`) y nunca examina el separador (`$5`). El separador le da
> igual.

---

## 14. Historial

| Versión | Fecha | Cambio |
|---|---|---|
| v1 | — | PAED declarativo (`cubo nombre=x posicion=0,0,0`). Discontinuado, archivado en `_void/docs/paed_spec_v1.md`. Sus primitivas de escena son hoy una librería, no sintaxis: ver [`ESCENA.md`](ESCENA.md) |
| v2.0 | 2026-08-07 | Unificación en el dialecto AED. Vivía en `docs/paed_spec.md` |
| v3.0 | 2026-08 | Reescritura corta al mudarse a `paed/Frankly/docs/` |
| v4.0 | 2026-08-10 | Absorbe la v2.0 completa. Se separan **cátedra** / **decidido** / **implementado**, porque había decisiones documentadas que el parser no cumplía |

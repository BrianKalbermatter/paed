# Archivos: organización, juego de archivos y plan

Cómo se declara un archivo, qué archivos hacen falta para resolver un ejercicio
de la cátedra, y en qué orden se implementa.

La sintaxis del lenguaje es [`PAED.md`](PAED.md); acá está el diseño y el plan.

## 0. Lo que se decide

1. La **organización** del archivo se escribe en la declaración, con las
   cláusulas que ya usa la cátedra: `ORDENADO POR` e `INDEXADO POR`.
2. Un ejercicio no usa *un* archivo: usa un **juego de archivos** con roles
   distintos. El asistente del editor trabaja sobre el juego, no sobre uno.
3. En disco todo es **CSV con encabezado** (`PAED.md §2.6`).
4. El asistente **escribe código en el `.paed`**. No guarda configuración
   aparte, en ningún lado.

## 1. La declaración

**cátedra.** Medido sobre el corpus: `ordenado por` aparece **68 veces**, y
`ARCHIVO SECUENCIAL` / `ARCHIVO INDEXADO` unas 20 entre las dos. Aparece dentro
del `AMBIENTE`, como código, no solo en la prosa del enunciado:

```paed
movi:     archivo de novedades ordenado por clave, tipo_novedad y f_novedad
mov:      archivo de Formato_Movimiento ordenado por clave
Arch:     archivo de reg ordenado por clave3, clave2, clave1, clave0
arch_mae: Archivo de Formato_mae indexado por clave
```

Las tres formas válidas:

| Declaración | Organización |
|---|---|
| `arch: ARCHIVO DE reg;` | secuencial **sin orden** |
| `arch: ARCHIVO DE reg ORDENADO POR a, b, c;` | secuencial **ordenado** |
| `arch: ARCHIVO DE reg INDEXADO POR clave;` | **indexado** |

La organización **no lleva palabra propia**: la dice la cláusula. No hace falta
agregar `SECUENCIAL` ni `INDEXADO` como keyword — el corpus no los escribe en la
declaración, los escribe en el enunciado.

`Archivo SECUENCIAL (no ordenado)` aparece tal cual en el corpus: la primera
forma es legítima, no un olvido de escribir el orden.

### Diferencias entre las dos cláusulas

| | `ORDENADO POR` | `INDEXADO POR` |
|---|---|---|
| Cantidad de campos | **lista** (clave compuesta) | **uno** |
| Separador en el corpus | comas, a veces con `y` final | — |
| Acceso | secuencial, hacia adelante | directo por clave |
| Operaciones | `LEER`, `ESCRIBIR` | `LEER` directo, `RE-ESCRIBIR`, `BORRAR` |

El `y` final es real: `ordenado por clave, tipo_novedad y f_novedad`. Se acepta
como separador equivalente a la coma.

### Se valida contra el REGISTRO

Los campos nombrados tienen que existir en el `REGISTRO` del archivo. Escribir
`ordenado por precio` cuando el registro no tiene `precio` es error en el
`AMBIENTE`, con la línea.

Sin esto la cláusula sería decorativa. Con esto, es la única declaración del
lenguaje que se verifica contra otra declaración — y es la que evita el error
que después aparece como datos desordenados en la salida.

## 2. El juego de archivos

Un ejercicio de actualización no tiene un archivo: tiene **cuatro o cinco, con
roles distintos**, y algunos aparecen solo según qué decisiones se tomen.

| Rol | Modo | Cuándo existe |
|---|---|---|
| **maestro** | `E/` | siempre |
| **movimientos** | `E/` | cuando la actualización viene de un lote |
| **maestro nuevo** | `S/` | solo en la actualización **secuencial** |
| **bajas** | `S/` | solo si hay baja **física** en la secuencial |
| **errores / listado** | `S/` | casi siempre |

Esa es la información que el asistente muestra: **el juego completo**, no la
línea que se está escribiendo. Declarar el maestro sin el archivo de errores es
el olvido típico, y no se nota hasta que el programa tiene que reportar el primer
movimiento inválido y no tiene dónde.

### Actualización secuencial

```
maestro     (E/) ─┐
                  ├──> maestro nuevo (S/)
movimientos (E/) ─┘    bajas (S/)   errores (S/)
```

Se leen los dos de entrada en paralelo comparando claves. Seis casos:

| Comparación | `Cod_Mov` | Acción | Avanza |
|---|---|---|---|
| `clave_mae < clave_mov` | — | no hay movimiento: se copia al nuevo | mae |
| `clave_mae = clave_mov` | 1 alta | **error**: ya existe | ambos |
| `clave_mae = clave_mov` | 2 baja | no se copia; va al archivo de bajas | ambos |
| `clave_mae = clave_mov` | 3 modif | se actualiza y se graba en el nuevo | ambos |
| `clave_mae > clave_mov` | 1 alta | alta nueva | mov |
| `clave_mae > clave_mov` | 2 o 3 | **error**: no existe | mov |

**El maestro nunca se modifica.** Se lee y se escribe uno nuevo. La baja es
simplemente *no copiar*.

Cuando un archivo se agota, su clave pasa a valer **`HV`** — alto valor, la
clave infinita, que siempre gana la comparación. Eso hace que lo que queda del
otro archivo se procese con las mismas seis reglas, sin un segundo bucle.

`HV` es lo que sostiene el algoritmo: sin él hacen falta tres bucles (mientras
quedan los dos, mientras queda solo el maestro, mientras quedan solo los
movimientos) y cada uno repite las mismas reglas con variantes.

### Actualización indexada

```
maestro (E/S) <──> movimientos por clave
```

No hay maestro nuevo: **se modifica el mismo archivo en el lugar**, con `LEER`
directo por clave, `RE-ESCRIBIR` y `BORRAR`.

```paed
SI cod_mov = 'M' ENTONCES
    // modificar campos y RE-ESCRIBIR
SINO
    // BAJA logica:  marcar y RE-ESCRIBIR
    // BAJA fisica:  BORRAR(arch_mae, reg_mae)
FIN_SI
```

> **Corrección a `PAED.md §2.6`.** Ahí se afirma que el patrón de la cátedra
> nunca modifica un registro en el lugar. Eso vale para la actualización
> **secuencial**, no para la **indexada**: `RE-ESCRIBIR` y `BORRAR` son
> exactamente modificación en el lugar, y están en el corpus.
>
> El CSV sigue sirviendo, pero por otro motivo del que decía esa sección: no
> porque no haga falta modificar en el lugar, sino porque **a la escala de estos
> programas reescribir el archivo entero no cuesta nada**. Es O(n) por
> operación, y `n` son decenas de registros.

## 3. Baja lógica y baja física

**cátedra**, las dos formas aparecen en el corpus como alternativas explícitas.

| | Qué hace | Qué necesita |
|---|---|---|
| **física** | el registro **desaparece** | en secuencial: un archivo de bajas. En indexada: `BORRAR` |
| **lógica** | el registro **queda**, marcado | un **campo marca** en el `REGISTRO` |

```paed
// Baja logica:
reg_mae.Marca_baja := '*'
RE-ESCRIBIR(arch_mae, reg_mae)

// Baja fisica:
BORRAR(arch_mae, reg_mae)
```

La diferencia importa para el asistente, y es la razón de que tenga que
preguntarlo en vez de asumirlo:

- **La baja lógica cambia el `REGISTRO`.** Hace falta un campo (`Marca_baja`)
  que no existe si nadie lo declaró. Elegir baja lógica y no tener el campo es
  un programa que no puede dar de baja.
- **La baja física cambia el juego de archivos.** En la secuencial, los
  registros dados de baja tienen que ir a algún lado.

Ninguna de las dos se puede deducir del código ya escrito. Por eso se pregunta.

Restricción del corpus, para los ejercicios con varios movimientos por maestro:
*"solo el último movimiento puede ser una baja lógica"*, y *"no existen altas o
bajas entre modificaciones"*.

## 4. El contrato del asistente

Se abre cuando la declaración de un archivo está **incompleta**, y deja de
abrirse cuando queda **completa**. Borrar la declaración y volver a escribirla la
abre de nuevo.

**No hay estado guardado en ningún lado.** El disparador se deriva del texto
fuente, no de un registro de "a este archivo ya lo configuré".

Esto no es un detalle de implementación, es lo que mantiene el `.paed`
autosuficiente. Si la organización viviera en un archivo de configuración al
costado, el programa dejaría de correr en otra máquina — y el resto del proyecto
está construido sobre lo contrario: el binario lleva el lenguaje adentro, y cada
test lleva su salida esperada adentro.

El asistente es **un ayudante de tipeo**. Lo único que produce es texto en el
`.paed`:

```
arch: ARCHIVO DE remedio                    <- incompleta, se abre
arch: ARCHIVO DE remedio ORDENADO POR ...;  <- completa, no se abre
```

Lo que muestra:

1. El **juego de archivos** del ejercicio, con el rol de cada uno y cuáles
   faltan declarar.
2. La **organización** del que se está escribiendo: secuencial u ordenado o
   indexado.
3. Si hay bajas: **lógica o física**, con lo que cada una arrastra.
4. Los **campos del `REGISTRO`**, para elegir los de la clave sin tipearlos.

Las opciones salen de `data/sintaxis.json`, no de una lista en C. Es la misma
regla que gobierna todo lo demás: agregar una organización nueva la hace
aparecer en el asistente sin tocar el editor. Ver `ESTRUCTURA.md`.

## 5. En disco

Un archivo declarado se materializa como un `.csv` con encabezado
(`PAED.md §2.6`). La organización **no cambia el formato**, cambia cómo se
accede:

| Organización | En disco | Acceso |
|---|---|---|
| secuencial | un `.csv` | de la primera fila a la última |
| ordenado | un `.csv`, filas ordenadas por la clave | ídem, y la clave permite comparar |
| indexado | un `.csv` | por clave; la búsqueda recorre hasta encontrarla |

El índice de verdad (una estructura que evite recorrer) es una optimización
posterior. Con decenas de registros, recorrer el CSV buscando la clave da el
mismo resultado y es mucho menos código. **Primero que funcione, después que sea
rápido** — y con estos tamaños, "rápido" ya lo es.

## 6. Plan

### Fase 1 — la declaración parsea y se valida

Sin esto no hay nada: el asistente no tiene qué escribir y el runtime no sabe
qué crear.

1. **`data/sintaxis.json`**: las cláusulas `ORDENADO POR` e `INDEXADO POR` como
   modificadores de la declaración `ARCHIVO`, con sus organizaciones nombradas
   (`secuencial`, `ordenado`, `indexado`) para que el asistente las lea de ahí.

2. **`PAEDDecl`** (`lang/include/paed/parser.h`) — campos nuevos, al lado de
   `es_archivo`:

   ```c
   // Organizacion del archivo. Sale de la clausula de la declaracion:
   // sin clausula es secuencial, ORDENADO POR es ordenado, INDEXADO POR
   // es indexado. No hay keyword propia: la organizacion la dice la
   // clausula, igual que en el corpus.
   PAEDOrg  org;                              // SECUENCIAL | ORDENADO | INDEXADO
   char     clave[PAED_MAX_CLAVE][PAED_NAME_MAX];
   int      clave_count;                      // INDEXADO POR siempre da 1
   ```

   `PAED_MAX_CLAVE` en 4: el corpus tiene como máximo cuatro campos
   (`clave3, clave2, clave1, clave0`).

3. **`lang/src/parser.c`** — en el parseo del `AMBIENTE`, después de resolver
   `ARCHIVO DE <tipo>`, mirar si sigue una cláusula. Separar la lista por comas
   **y por la palabra `y`**, que el corpus usa como separador final.

4. **Validar contra el `REGISTRO`**: cada campo de la clave tiene que estar en
   `PAEDRegistro.campos`. Esto va **después** de parsear todo el `AMBIENTE`, no
   durante: el archivo puede declararse antes que el registro.

5. **Tests**: `archivos_organizacion.paed` (las tres formas, con la `y` final) y
   `archivos_organizacion_errores.paed` (campo que no existe, `INDEXADO POR` con
   dos campos, cláusula sobre algo que no es archivo).

**Termina cuando**: las tres formas parsean, los errores se reportan con línea,
y `make test` sigue en verde.

### Fase 2 — el CSV existe en disco

6. **`CREAR`** escribe el encabezado con los campos del `REGISTRO`, en orden de
   declaración.
7. **`ABRIR`** con su modo (§2.5): lee el encabezado y lo **compara contra el
   `REGISTRO`**. Si no coincide, error diciendo qué campo esperaba y cuál
   encontró.
8. **`LEER(arch, reg)`** trae la próxima fila y convierte cada columna al tipo
   declarado. Un `ENTERO` que en el archivo dice `abc` es error de lectura, no
   un cero silencioso.
9. **`ESCRIBIR(arch, reg)`** agrega una fila.
10. **`FDA`** verdadero cuando no quedan filas después del encabezado. Un
    archivo recién creado tiene solo encabezado: `FDA` desde el arranque.
11. **`CERRAR`** cierra y descarga.

**Termina cuando**: un `.paed` crea un archivo, escribe registros, lo cierra, lo
vuelve a abrir, lo lee entero y llega a `FDA` — y el `.csv` se abre en una
planilla y se entiende.

### Fase 3 — el algoritmo completo

12. **`HV`** como constante del lenguaje, junto a `V` y `F` en `primario()` de
    `lang/src/expr.c`. Compara mayor que cualquier clave.
13. **Test de actualización secuencial**: maestro + movimientos → maestro nuevo
    + bajas + errores, con los seis casos y el agotamiento de los dos archivos.

Ese test es el que demuestra que los archivos sirven: es el ejercicio que toma
la cátedra.

### Fase 4 — el asistente

14. Detectar la declaración incompleta y su posición.
15. Leer las organizaciones desde `sintaxis.json`.
16. Mostrar el juego de archivos, la organización, la baja, y los campos del
    registro.
17. Escribir la cláusula elegida en el buffer.

Va cuarto a propósito: hasta que la Fase 1 no exista, el asistente no tiene
sintaxis válida que escribir, y hasta que no exista la Fase 2, lo que escribe no
produce ningún archivo.

### Fase 5 — indexado de verdad

18. `RE-ESCRIBIR(arch, reg)` y `BORRAR(arch, reg)`, reescribiendo el `.csv`.
19. `LEER` directo por clave.
20. Campo marca y baja lógica.

**Hasta que esta fase no esté, el asistente ofrece `INDEXADO POR` pero avisa que
la organización todavía no ejecuta.** Ofrecer una opción que después falla en
`ABRIR` es peor que no ofrecerla: el error aparece lejos de la decisión que lo
causó.

## 7. Lo que queda abierto

| Punto | Estado |
|---|---|
| Separador del CSV: `,` o `;` | **abierto** — bloquea la Fase 2 |
| De dónde sale el nombre del `.csv` en disco | **abierto** |
| `CREAR` sobre un archivo que ya existe: ¿pisa o es error? | **abierto** |
| Entrecomillado de texto con separador adentro | propuesto: RFC 4180 |

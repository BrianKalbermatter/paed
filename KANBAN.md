---

kanban-plugin: board

---

## PROXIMO — los dos tests grandes (2026-08-19)

> El lenguaje YA DA para escribirlos: se verificó con un corte de control de dos
> niveles sobre un archivo real, con la forma exacta del template. Lo que falta
> es la LOGICA de cada uno, que la escribe Brian.

- [ ] Test: un corte de control de dos niveles escrito con subacciones, como lo escribe `CORTE DE CONTROL [TEMPLATE Rev2].txt` #subacciones
- [ ] Test: la actualización secuencial unitaria de `ACTUALIZACION INC UNI [TEMPLATE].txt`, con `LEER_arch_mae`, `IGUALES`, `DISTINTOS` y `PASO_DIRECTO` #subacciones
- [ ] **Archivos y secuencias LOCALES a una subacción.** Hoy se rechazan con mensaje propio: solo se declaran en el `AMBIENTE` del programa. Pasarlas por parámetro sí anda, que es lo que hace el template `InicializarSecuencia(VAR sec_local: SECUENCIA de caracter)` #subacciones

## PROXIMO — lo otro que falta para los templates

- [ ] **Matrices: `ARREGLO[1..3, 1..3] DE ENTERO`** y el acceso `M[i, j]`. Es lo único que bloquea a `ARREGLOS_Conceptos.txt`. **Es del TP 3**, así que no corre prisa hasta que se dé arreglos en la cátedra #parser
- [ ] `HV` declarado como constante: los templates escriben `HV = 99999999;` en el `AMBIENTE`. Hoy PAED lo tiene como **tipo de valor propio** y lo rechaza si se declara. Hay que **aceptar la declaración sin romper la semántica**: las claves de los parciales son texto, y `strcmp("99999999", "F1-Ibuprofeno")` da que HV es MENOR — justo al revés de lo que HV significa (PAED.md §2.8) #decidir #f3-algoritmo
- [ ] `ABRIR(arch, lectura)`, la grafía de la wiki Cap. 5. La de cátedra (`ABRIR E/`) ya anda con la barra opcional; esta es la que falta #parser

## Decisiones pendientes

- [ ] `-2 ** 2` da 4 porque la tabla de prioridad pone los unarios ARRIBA de la potencia. En casi todos los lenguajes da -4. ¿Es lo que quiere AED? (`TEORIA_COMPLETA.txt:361-371`) #decidir
- [ ] `VARIABLES` como sub-sección de `AMBIENTE`: aparece en `AED_2021_UnI.pdf:10` y en ninguna otra fuente. Confirmar si es obligatoria antes de implementar #decidir

## Backlog — Fase 1: la declaración parsea

- [ ] *(fases 1, 2 y 3 COMPLETAS. La batería entera va en 46/46 — `make test`. Sigue la fase 4: el asistente del editor)* #f1-declaracion

## Backlog — Fase 2: el CSV existe en disco


## Backlog — Fase 3: el algoritmo completo

- [ ] `LV` (Low Value): CERO apariciones en wiki, OnlySintaxis y TEORIA_COMPLETA. **No inventarlo** — si algún día aparece en un parcial, ahí se agrega #f3-algoritmo

## Backlog — Fase 4: el asistente del editor

- [ ] Detectar la declaración de archivo incompleta y su posición en el buffer #f4-asistente
- [ ] Leer las organizaciones desde `sintaxis.json`, no de una lista en C: agregar una organización nueva tiene que hacerla aparecer sola en el asistente #f4-asistente
- [ ] Mostrar el **juego de archivos** del ejercicio con el rol de cada uno y cuáles faltan declarar: maestro, movimientos, maestro nuevo, bajas, errores #f4-asistente
- [ ] Preguntar la organización: secuencial, ordenado o indexado #f4-asistente
- [ ] Preguntar la baja cuando corresponda: **lógica** (cambia el `REGISTRO`, necesita campo marca) o **física** (cambia el juego de archivos) #f4-asistente
- [ ] Listar los campos del `REGISTRO` para elegir los de la clave sin tipearlos #f4-asistente
- [ ] Escribir la cláusula elegida en el buffer. El asistente **solo produce texto en el `.paed`**: no guarda configuración en ningún lado, el disparador se deriva del texto fuente #f4-asistente
- [ ] Mientras la fase 5 no exista, ofrecer `INDEXADO POR` **avisando que todavía no ejecuta**. Ofrecer una opción que después falla en `ABRIR` es peor que no ofrecerla: el error aparece lejos de la decisión que lo causó #f4-asistente

## Backlog — Fase 5: indexado de verdad

- [ ] `RE-ESCRIBIR(arch, reg)` reescribiendo el `.csv` (`TEORIA_COMPLETA.txt:1112-1114`) #f5-indexado
- [ ] `BORRAR(arch, reg)` — baja física en archivo indexado #f5-indexado
- [ ] `LEER` directo por clave: **el TERCER `LEER`** (`reg.clave := x; LEER(arch, reg)`, `wiki.txt:2764`) NO avanza, **busca**. Hoy se marca igual que el secuencial; distinguirlos necesita saber si el archivo es indexado — por eso depende de la fase 1 #f5-indexado
- [ ] Campo marca y baja lógica (`reg_mae.Marca_baja := '*'` + `RE-ESCRIBIR`) #f5-indexado
- [ ] `GRABAR` de archivos indexados #f5-indexado
- [ ] Restricción del corpus a respetar: "solo el último movimiento puede ser una baja lógica" y "no existen altas o bajas entre modificaciones" #f5-indexado

## Backlog — lenguaje, sin relación con archivos

- [ ] **Errores ESTATICOS que se reportan tarde, en runtime.** El parser ya tiene el AMBIENTE y ya sabe que `'"hola"'` no es un destino de LEER, que un registro no tiene tal campo, y que una SECUENCIA DE SALIDA se abre con CREAR y no con ARR. Hoy todos esos saltan al EJECUTAR. Moverlos al parser es el arreglo de fondo: ahi reportar de a muchos SI corresponde, porque son independientes entre si #parser
- [ ] **Cobertura perdida por el corte de ejecución (2026-08-14)**: `leer_errores`, `secuencia_errores`, `archivos_formas` y `archivos_modos` eran CATALOGOS que fijaban ~20 mensajes de error en una sola corrida. Desde que la ejecución corta en el primero, solo se verifica ese. No se partieron en 20 archivos a propósito: si los errores estáticos se mueven al parser (item de arriba), los catálogos vuelven a funcionar solos #parser
- [ ] Nombre de `ACCION` con espacios (`ACCION Ejercicio de Parcial ES`) #parser
- [ ] `CONSTANTE` #parser
- [ ] Una instrucción no puede partirse en dos líneas — el parser lee línea por línea. Un `ESCRIBIR` largo hay que dejarlo en una sola #parser
- [ ] La rama de `ARREGLO` en `parse_decl` no exige espacio después de la palabra, así que un tipo llamado `ARREGLOS` entraría por ahí. La de `ARCHIVO` sí lo exige #parser
- [ ] Subir `PAED_VAL_MAX` (128 bytes) o sacarle el tope al texto de `ESCRIBIR`: hoy un marco decorativo Unicode de 40 símbolos ya no entra #parser
- [ ] Usar el `AMBIENTE` para chequear tipos. Hoy se parsea pero no se usa: asignarle un texto a algo declarado `ENTERO` no da error #evaluador
- [ ] Los arreglos no chequean el TIPO declarado: `A: ARREGLO[1..5] DE ENTERO` acepta un texto en `A[2]` #evaluador
- [ ] Guardar un árbol de la expresión en vez de re-parsear el texto en CADA vuelta del bucle. Hoy es simple y correcto, pero un `MIENTRAS` largo paga el costo en cada iteración #evaluador
- [ ] Avisar cuando se usa `==`: ya está resuelto que NO existe en AED (`TEORIA_COMPLETA.txt:324` define `=`, y la wiki lo marca como error arrastrado, 91 usos). Hoy se acepta callado para no romper los archivos; debería avisar **sin frenar** la ejecución #evaluador
- [ ] Corroborar contra la wiki: `RETORNAR`, `TRUNC`, `ABSO`, `REDOND` — 0 apariciones en los apuntes #decidir

## Backlog — infraestructura

- [ ] **Lo que toca a VimMon vive en el `KANBAN.md` de VimMon, no acá.** `escena.json` del lado equivocado y las dos copias de cJSON son problemas de VimMon: PAED no sabe que VimMon existe #infra
- [ ] Los tests traen su propia ruta en el bloque `SALIDA ESPERADA`, porque los errores del parser la imprimen. Mover `tests/` los rompe a todos. Hoy no molesta, pero está a la vista #infra
- [ ] El objeto del CLI se compila en `build/lang/lang/cli/main.o`, con `lang` repetido. Cosmético #infra
- [ ] Runner de tests en C en vez de `correr.sh`: hoy la batería **no puede correr en Windows**, aunque ya se cross-compila `paed.exe` #infra

## Backlog — tutorial

- [ ] Segunda tanda de ejercicios: `ARCHIVO`, corte de control, actualización secuencial — el ejercicio que toma la cátedra #tutorial
- [ ] `paed aprender` no puede detectar un ciclo infinito por sí mismo: se apoya en `timeout` de coreutils, que en Windows no existe. Ahí un ejercicio colgado se sale con Ctrl-C #tutorial #infra

## En progreso

## Hecho

- [x] **Las formas de llamada de la cátedra — 2026-08-19.** Cuatro cosas, y sin las cuatro no se puede escribir un corte de control: **llamada SIN paréntesis** (`Inicializar`, `tratar_corte;`, `corte_3;` — así lo escribe todo el corpus), **`SUBACCION <nombre> ES`** (el `ES` es opcional, igual que en la cabecera de `ACCION`), **cierre con `Fin;` a secas** (resuelto por CONTEXTO y no en `GRAFIAS[]`: `Fin;` también cierra la `ACCION`, traducirlo a ciegas rompería el programa), y **`HV = 99999999;` en el `AMBIENTE`** (se acepta y se IGNORA — HV sigue siendo un valor de alto propio del lenguaje, porque las claves de los parciales son texto). Verificado con un corte de control de dos niveles sobre un archivo real #subacciones #parser
- [x] **`VAL_VACIO`: declarada pero sin valor — 2026-08-19.** Antes una variable declarada NO estaba en la tabla, y "no está" significaba "no tiene valor". Las subacciones rompieron eso: si una subacción le asigna a un global que todavía no tenía valor, `env_set` no lo encuentra, lo crea ARRIBA — o sea adentro del marco — y al volver el marco se lo lleva puesto. El `Inicializar` del template hace exactamente eso, así que el corte de control habría fallado en la primera línea. Ahora las declaradas están en la tabla desde el arranque con tipo `VAL_VACIO`, leerlas sigue diciendo "no tiene valor todavía", y de paso una local tapa a una global desde que se declara #evaluador #subacciones

- [x] **Subacciones — 2026-08-19. EL bloqueante, resuelto.** `FUNCION` y `PROCEDIMIENTO` con su propio `AMBIENTE` y su propio `PROCESO`, declarados antes del `PROCESO` principal. Los cuerpos van en el MISMO `instrs[]` que el programa, cada uno con su rango `[inicio, fin)`: llamar es correr un rango, y como el intérprete ya era un contador de programa, no hizo falta una segunda maquinaria. 5 tests nuevos, `make test` en 46/46 #subacciones #parser #evaluador
- [x] **Pila de llamadas y ámbito local — 2026-08-19.** `ejecutar_rango()` se llama a sí misma: la pila de C hace de pila de llamadas. Marcos de variables con `env_push` / `env_truncar`, y `env_buscar` pasó a recorrer la tabla DESDE EL FINAL para que una local tape a una global. La marca de los `PARA` dejó de ser `static`: si no, una subacción llamada dos veces se pisaba con su propia llamada de afuera. Guarda de 64 llamadas anidadas #evaluador
- [x] **Parámetros `E` / `S` / `ES` / `VAR` — 2026-08-19.** `E` copia; `S` no trae valor y devuelve; `ES` y `VAR` entran con valor y salen modificados. `VAR` es `ES`: por referencia es entrar y salir, no hace falta un cuarto modo. Los argumentos se evalúan ANTES de abrir el marco y se devuelven DESPUES de cerrarlo — al revés, el nombre de afuera está tapado por el de adentro #subacciones
- [x] **Retorno de `FUNCION` por asignación al nombre — 2026-08-19.** `sumar := a + b`, y usable dentro de una expresión: `ESCRIBIR(sumar(doble(2), sumar(1,1)))`. `expr.c` no aprendió a ejecutar instrucciones — recibe dos ganchos del intérprete (`expr_set_funcion`), así que sigue siendo un evaluador de expresiones y nada más #evaluador
- [x] **Errores del parser ordenados por línea — 2026-08-19.** Las claves, los modos y las llamadas a subacciones se verifican al final, con el programa entero leído, así que sus errores caían todos juntos DESPUES de los demás. Quien leía el reporte arreglaba la línea 30 y recién en la vuelta siguiente se enteraba de que la 9 también estaba mal. `ordenar_errores()` es una inserción estable: dos errores de la misma línea conservan el orden en que se detectaron #parser

- [x] **La cátedra manda — 2026-08-17.** Decisión: cuando PAED y la cátedra se contradicen, se corrige PAED. De los 26 templates oficiales parseaban **2**; ahora parsean **9**. Un lenguaje que dice ser el pseudocódigo de la cátedra y no puede correr el código de la cátedra no es el pseudocódigo de la cátedra. Ver `docs/PAED.md` §15 #decidir #parser
- [x] **Dos fuentes de verdad nuevas, con MÁS peso que la wiki**: los 27 templates oficiales de `github.com/UTN-FRRe/isi-aed/tree/master/Pseudocodigo` y la guía de TPs de `aed-frre.github.io`. No son prosa sobre el lenguaje: son el lenguaje. Cuando un template contradice a la wiki o a una decisión de PAED, gana el template #decidir #docs
- [x] **Capa de traducción de grafías**: la tabla `GRAFIAS[]` y `normalizar_catedra()` en `parser.c`, que corren sobre cada línea antes que todo lo demás. Los cierres se analizan en cuatro puntos distintos del parser; con un `if` por variante en cada punto, agregar una grafía obliga a tocar cuatro lugares y el día que se olvida uno la misma palabra anda en un contexto y no en el otro. Solo se traducen líneas que son UNA palabra clave sola, así que ningún `;` de instrucción corre peligro #parser
- [x] **El paso del `PARA` va tras COMA** (`Para c := 1 hasta 10, 1 hacer`), la forma de `Para.txt`, `SUBSECUENCIA.txt` y `ARREGLOS_Conceptos.txt`. El `;` que usaba PAED sigue valiendo. El separador se busca a nivel superior, así la coma de `f(a, b)` no parte el `PARA` #parser
- [x] **El `;` del final pasa a ser OPCIONAL.** Sigue obligatorio como SEPARADOR entre sentencias del mismo renglón, que es el trabajo por el que estaba: el caso que protegía (`s: SECUENCIA DE ENTERO; n: ENTERO;` dejando a `s` con tipo basura) lo cubre el corte por `;`, no el `;` del final #parser
- [x] `ALGORITMO` = `PROCESO` y `CONTRARIO` = `SINO` #parser
- [x] Los cierres de la cátedra: `FinSi;` `FSI;` `FIN SI;` `FinMientras;` `FMientras;` `FinPara;` `FinSegun;` `fin_reg;` `FinReg` `freg;` #parser
- [x] `Accion X ES;` con `;`, el `ES` **opcional** (`accion archivo_corte;` de CORTE DE CONTROL Rev2), y `FinAccion.` con punto #parser
- [x] Comentarios `{entre llaves}` y `* entre asteriscos *`, cuando **abren la línea**. La restricción no es caprichosa: `d: {1..31}` es un tipo rango y `a := b * c` es multiplicación, así que en el medio de una línea los dos caracteres ya significan otra cosa #parser
- [x] `Esc` / `ESC` / `GRABAR` como alias de `ESCRIBIR`. Viven en `data/sintaxis.json` y **no en el C**, por la misma razón que las organizaciones de archivo: con dos listas, un día el parser y el asistente del editor dicen cosas distintas. El parser guarda el nombre canónico, así que el intérprete nunca ve un alias #parser
- [x] `ABRIRe(arch)` y `ABRIRs(arch)`: la barra del modo pasa a ser **opcional**. Dos barras sigue siendo error — ahí no hay una forma de la cátedra que interpretar, hay un modo escrito mal #parser
- [x] `NOFDA` / `NoFDA` / `NOFDS` como `NFDA` / `NFDS`. (`No FDA(arch)` con espacio ya andaba solo: `NO` es el operador lógico y `FDA` la función) #evaluador
- [x] **`REPETIR ... HASTA [QUE] cond`** — el ciclo post-test (`Repetir.txt`). La condición dice cuándo TERMINAR, al revés que la del `MIENTRAS`, y el cuerpo siempre corre al menos una vez. Test: `tests/catedra_repetir.paed` #parser #evaluador
- [x] **`SEGUN ... FIN_SEGUN`** con etiquetas múltiples (`'B', 'M':`), `CONTRARIO` y `CONTRARIO:` como rama por defecto, y **sin caída** de una rama a la siguiente. Las ramas se encadenan al parsear (campo `siguiente` en `PAEDInstr`), así un `SEGUN` anidado no se mezcla con el de afuera. Usa `expr_comparar()` para elegir rama con la MISMA semántica que un `SI ... = ...`. Test: `tests/catedra_segun.paed` #parser #evaluador
- [x] **`SEGUN` sí es de cátedra.** `sintaxis.json` decía que "solo aparece como prosa" — estaba equivocado: está en `Segun.txt` y en `ACT INDEX [TEMPLATE].txt`. La nota se escribió antes de que existieran esas fuentes #decidir
- [x] **Nombres con `ñ` y tildes**: `año` es un campo en `REGISTRO.txt`, `ARCHIVO_CREAR.txt` y `ARCHIVO_LEER.txt`. Se aceptan los bytes UTF-8 sin decodificar el punto de código: los nombres se comparan y se guardan como bytes. Las keywords siguen siendo ASCII puro, así que ningún nombre con tilde puede chocar con una. Test: `tests/catedra_utf8.paed` #parser #evaluador
- [x] **Bug: la comilla SIMPLE no delimitaba texto en el troceador de argumentos.** Una coma adentro de `'...'` cortaba el argumento al medio y salía "falta la comilla de cierre". La comilla simple es la forma de la cátedra (`AED_2021_UnI.pdf:10`), o sea que la forma correcta era justo la que no andaba: `ESCRIBIR('Ingrese un valor entero, vamos a...')` —una línea de `Si.txt`— no parseaba. Ahora se recuerda CUÁL comilla abrió, no un toggle. Test: `tests/catedra_comilla_simple.paed` #parser
- [x] **El ejercicio 1 del tutorial enseñaba el `;` terminador**, y desde el cambio pasaba sin tocarlo: el alumno lo abría, no cambiaba nada y el verificador le decía que estaba bien. Reescrito alrededor del `;` que SEPARA, que sí sigue siendo cierto #tutorial
- [x] Pushear a `origin`: hecho el 2026-08-17, cinco commits #infra

- [x] **Bug: el `=` de comparación desaparecía adentro de un argumento.** `ESCRIBIR("x ", 3 = 3)` imprimía `3` y `3 = 4` imprimía `4`. El parser buscaba `clave = valor` en TODO argumento de TODO procedimiento, partía la comparación y tiraba el lado izquierdo. El troceado `clave = valor` ahora solo se busca en procedimientos que **declaran parámetros** — los de una librería (`escena.json`), nunca los del lenguaje, que son variádicos. Adentro de `SI (...)` andaba bien, y por eso sobrevivió tanto #evaluador #parser
- [x] **Los `.paed` se leen igual que en la cátedra.** `AMBIENTE`, `PROCESO` y `FIN_ACCION` van **sin sangría**, al mismo nivel que `ACCION`, como los dibuja `TEORIA_COMPLETA.txt:201-206`. Se reescribieron los 12 ejercicios, sus 12 soluciones y `ejercicios/control_ventas.paed` — solo espacios, `git diff -w` da vacío. Los ejercicios embebidos se regeneraron, si no `paed aprender reset` restauraba la sangría vieja #tutorial #docs
- [x] **`a, doble: entero;` — varias variables en una declaración.** Sale del ejemplo canónico de la cátedra (`TEORIA_COMPLETA.txt:440`). `wiki_paed.txt:149-150` lo tenía como pregunta abierta contestada con un "Frankly dice NO", y **Frankly no es autoridad**: `docs/PAED.md:1319` dice que es permisivo y que correr ahí no hace correcto a un archivo — tampoco hace incorrecto a lo que rechaza. Vale para todos los tipos, `ARREGLO`/`ARCHIVO`/`SECUENCIA` incluidos #parser
- [x] El corte por coma **no colapsa los vacíos**: `a,,b` y `a,b,` dan error con el nombre vacío a la vista. Tragarse una coma de más era exactamente lo que este parser promete no hacer #parser
- [x] Test `tests/catedra_estructura.paed`: reproduce el ejemplo de `TEORIA_COMPLETA.txt:438-446` tal como lo escribe la cátedra — sin sangría, tipo en minúscula y declaración múltiple. Es el ancla que impide que el estilo vuelva a driftear #infra
- [x] **Decisión: una comparación NO va como argumento.** Un comparador suelto (`=` `<>` `<` `<=` `>` `>=`) adentro de un argumento se **rechaza** nombrándolo; la comparación va en la condición de un `SI` o un `MIENTRAS`. Vale para todos los variádicos, no solo `ESCRIBIR`. **Es una decisión de PAED, NO una regla de la cátedra**, y así está documentado: la teoría solo dice que los relacionales devuelven `V` o `F` (`TEORIA_COMPLETA.txt:319-320`) y nunca se pronuncia sobre si pueden ser argumento — sus dos ejemplos de `ESCRIBIR` (`:442`, `:445`) pasan un texto y una variable. Se rechaza para no aceptarlo callado, que es el bug de arriba #decidir #parser
- [x] **Corregida documentación inventada.** `parser.c` y `tests/comparaciones.paed` citaban `wiki.txt:148 "ESCRIBIR -- muestra algo por pantalla"` — esa frase **no existe en ningún archivo del repo**, y `wiki_paed.txt:148` es un encabezado `A COMPARAR` del bloque de declaraciones. Regla que queda: no se cita la teoría sin la línea textual de `TEORIA_COMPLETA.txt` #docs
- [x] **Bug: cualquier operador entre dos textos literales se imprimía crudo.** `ESCRIBIR("Ana" + "Beto")` daba `Ana" + "Beto`. `interpreter.c` tenía un atajo que imprimía sin evaluar cuando el argumento empezaba y terminaba con comilla, y esa condición la cumple una expresión entera. Se **sacó** el atajo en vez de ajustarlo: era una segunda implementación de lo que `primario()` ya hacía en `expr.c` #evaluador
- [x] `igual_separador` ya no corta por el `=` de `<=` y `>=`. Solo afecta a los argumentos con nombre de una librería, pero ahí el bug era el mismo #parser
- [x] **`==` sacado del detector de comparadores.** `==` NO EXISTE en AED: la igualdad es `=` sola (`TEORIA_COMPLETA.txt:324`, anotado en `wiki_paed.txt:225` como error de escritura arrastrado en los `.paed`). El código lo listaba como operador y el mensaje de error nombraba un operador que el lenguaje no tiene #parser
- [x] Mensaje de rechazo neutral respecto del procedimiento: decía *"y ARR muestra un valor"*, y `ARR` no muestra nada — la regla corre sobre todos los variádicos #parser
- [x] Test `tests/comparaciones.paed`: los dos bugs, más `<=`/`>=`/`<>`, textos literales comparados y concatenados, un `=` adentro de un texto, y combinados con `Y`/`O`. `tests/errores.paed` cubre los rechazos: `=`, el `==` mal escrito y un `<` en `ARR` #infra
- [x] **`paed aprender`: el tutorial adentro del binario** — 12 ejercicios rotos a propósito, al estilo de rustlings. Sin archivo de progreso: el ejercicio actual es el primero que no pasa, y se calcula corriéndolos, así el estado no puede contradecir al disco. El juez es el bloque `SALIDA ESPERADA`, el mismo formato que `tests/` — un ejercicio *es* un test que viene roto #tutorial
- [x] Los ejercicios viajan embebidos, como `sintaxis.json`: quien baja `paed` suelto tiene el tutorial sin clonar el repo, y por eso `reset` puede restaurar el original #tutorial
- [x] El ejercicio se corre como SUBPROCESO del propio binario: un ejercicio que revienta o se cuelga no se lleva puesto al tutor, y se juzga exactamente el comando que el alumno tipearía #tutorial
- [x] `make test-aprender`: demuestra que ningún ejercicio pasa sin tocarlo y que todos tienen solución que pasa. Sin eso, un ejercicio ya resuelto es un hueco silencioso en la progresión #tutorial #infra

- [x] Qué valor exacto vale `HV`. `DBL_MAX` lo hace incomparable de verdad pero se imprime feo; `999999999` es legible y alcanza para las claves de los ejercicios #decidir #f3-algoritmo
- [x] ¿`HV` distingue mayúsculas (`hv`, `Hv`)? Las keywords no distinguen, y esto es una constante del lenguaje: debería seguir la misma regla #decidir #f3-algoritmo
- [x] **`HV` (alto valor) como constante del lenguaje**, junto a `V`/`F` en `primario()` de `lang/src/expr.c`. No se declara, no se asigna, no ocupa entrada de variable. Hoy `SI (x <> HV)` da "la variable 'HV' no tiene valor todavía" #f3-algoritmo
- [x] `HV` también en `sintaxis.json`, para que el resaltador lo pinte como constante y no como variable #f3-algoritmo
- [x] Test de actualización secuencial: maestro + movimientos → maestro nuevo + bajas + errores, con los seis casos y el agotamiento de los dos archivos. **Es el ejercicio que toma la cátedra** #f3-algoritmo
- [x] De dónde sale el nombre del `.csv` en disco: ¿lo dice el programa en `ABRIR`, o sale del nombre de la variable? #decidir #f2-csv
- [x] `CREAR` sobre un archivo que ya existe: ¿lo pisa o es error? #decidir #f2-csv
- [x] Entrecomillado de texto con el separador adentro. Propuesto: RFC 4180 (comillas dobles, duplicando las internas). No choca con el `'` de PAED (§10.4) #decidir #f2-csv
- [x] `CREAR` escribe el encabezado con los campos del `REGISTRO`, en orden de declaración #f2-csv
- [x] `ABRIR` con su modo lee el encabezado y lo **compara contra el `REGISTRO`**. Si no coincide, error diciendo qué campo esperaba y cuál encontró. Es lo que ataja abrir el archivo equivocado, que hoy se leería sin protestar devolviendo basura con forma de dato válido #f2-csv
- [x] `LEER(arch, reg)` trae la próxima fila y convierte cada columna al tipo declarado. Un `ENTERO` que en el archivo dice `abc` es error de lectura, **no** un cero silencioso #f2-csv
- [x] `ESCRIBIR(arch, reg)` agrega una fila #f2-csv
- [x] `FDA` verdadero cuando no quedan filas después del encabezado. Un archivo recién creado tiene solo encabezado: `FDA` desde el arranque #f2-csv
- [x] `CERRAR` cierra y descarga #f2-csv
- [x] Handle de archivo de verdad en el intérprete: hoy la forma se distingue y se valida, pero ninguna operación toca el disco #f2-csv
- [x] Test de ida y vuelta: crear, escribir registros, cerrar, reabrir, leer entero hasta `FDA` — y que el `.csv` se abra en una planilla y se entienda #f2-csv
- [x] **Separador del CSV: `;`** — decidido 2026-08-14, contra el estandar a proposito. El motivo de usar CSV era abrirlo y ver la planilla, y con coma Excel en español lo apelmaza en la columna A. No hay choque con los numeros: el decimal de PAED es el punto. Es una constante en un solo lugar #decidir #f2-csv
- [x] `data/sintaxis.json`: cláusulas `ORDENADO POR` e `INDEXADO POR` como modificadores de la declaración `ARCHIVO`, con las organizaciones nombradas (`secuencial`, `ordenado`, `indexado`) para que el asistente las lea de ahí y no de una lista en C #f1-declaracion
- [x] `PAEDDecl` en `lang/include/paed/parser.h`: campos `org`, `clave[PAED_MAX_CLAVE][PAED_NAME_MAX]` y `clave_count`, al lado de `es_archivo`. `PAED_MAX_CLAVE` en 4 — el corpus llega a `clave3, clave2, clave1, clave0` #f1-declaracion
- [x] `lang/src/parser.c`: parsear la cláusula después de resolver `ARCHIVO DE <tipo>`. Separar la lista por comas **y por la palabra `y`**, que el corpus usa antes del último campo (`ordenado por clave, tipo_novedad y f_novedad`) #f1-declaracion
- [x] Validar los campos de la clave contra el `REGISTRO` del archivo. Va **después** de parsear todo el `AMBIENTE`, no durante: el archivo puede declararse antes que el registro #f1-declaracion
- [x] `INDEXADO POR` con más de un campo es error: lleva uno solo #f1-declaracion
- [x] Test `archivos_organizacion.paed`: las tres formas (sin cláusula, `ORDENADO POR` con lista y `y` final, `INDEXADO POR`) #f1-declaracion
- [x] Test `archivos_organizacion_errores.paed`: campo que el registro no declara, `INDEXADO POR` con dos campos, cláusula sobre algo que no es archivo #f1-declaracion
- [x] Actualizar la tabla de `PAED.md §12` cuando esto pase a ✅ — la tabla y el KANBAN se actualizan juntos #f1-declaracion #docs
- [x] **Modo de apertura de `ABRIR`: `E/`, `S/`, `E/S`** — sin importar espacios, mayúsculas ni de qué lado va la barra. Es campo de `PAEDInstr`, no argumento: va afuera del paréntesis y meterlo en `args[]` correría de lugar al primer argumento, que es el que decide si la operación es de archivo o de consola #f1-declaracion
- [x] `LEER E/` y cualquier modo sobre un procedimiento que no lo admite: rechazado con mensaje propio #f1-declaracion
- [x] `FIN_REGISTRO` faltante cuando ya empieza otro `REGISTRO`: antes entraba como campo del anterior y el error era "falta ';'" en la única línea que estaba bien #parser
- [x] **Fix de build `-MMD -MP`**: sin las dependencias de headers, tocar un `.h` no recompilaba los `.c` que lo usan. Agregar un campo a `PAEDInstr` dejaba `main.c` con el struct viejo y `parser.c` con el nuevo, y moría con `stack smashing detected` — que manda a buscar un desborde de buffer inexistente #infra
- [x] **Frankly retirado**: `data/`, `docs/`, `tests/` y `ejercicios/` salen a la raíz; el intérprete bash y lo generado muerto van a `_void/`. Documentado en `docs/ESTRUCTURA.md` #docs #infra
- [x] `docs/ARCHIVOS.md`: juego de archivos, baja lógica vs física, y el plan en cinco fases #docs
- [x] `PAED.md §2.5` el modo de apertura, `§2.6` el CSV en disco, `§2.7` `ORDENADO POR` / `INDEXADO POR` con la evidencia del corpus #docs
- [x] Verificado contra el corpus: `ordenado por` 68 apariciones, dentro del `AMBIENTE` y como código. La organización **no lleva keyword propia** — la dice la cláusula #docs
- [x] `LEER` de consola EJECUTA: destinos `x`, `A[i]` y `p.campo`; un dato por línea para que "Juan Perez" entre entero #parser
- [x] De dónde salen los datos lo decide el HOST, no el intérprete (`interp_set_entrada`). Sin ese puerto, un `fgets` adentro del game loop congela la ventana entera #parser
- [x] Tests con bloque `// ── ENTRADA` en el propio `.paed`, que `correr.sh` pasa por stdin. stdin viene siempre de ahí aunque el bloque no exista: así un test sin datos falla en vez de colgar la corrida #infra
- [x] `REGISTRO` / `FIN_REGISTRO`, acceso `p.campo`, y rechazo de un campo que el registro no declara #parser
- [x] `SECUENCIA DE <tipo>` / `DE SALIDA`, `VENTANA DE <tipo>`, `ARR` / `AVZ` / `NFDS` / `FDS`, y `CREAR`/`ESCRIBIR`/`CERRAR` sobre secuencias #parser
- [x] `ARCHIVO DE <tipo>` en el `AMBIENTE`, y `LEER`/`ESCRIBIR` distinguiendo consola de archivo por la declaración, no contando argumentos #parser
- [x] `ARREGLO[desde..hasta] DE <tipo>` con límites chequeados en cada acceso #parser
- [x] `SI`/`SINO`/`FIN_SI` y `MIENTRAS` anidados, `PARA ... HASTA ... [; paso] HACER` incluso en reversa #parser
- [x] Expresiones, operadores de comparación, cortocircuito en `Y`/`O` #evaluador
- [x] Keywords en minúscula o mezcladas (`accion`, `MiEnTrAs`) #parser
- [x] El `;` como terminador, y varias sentencias en una línea #parser
- [x] `FIN_ACCION` y `FINACCION`; `FACCION` y `FIN ACCION` rechazados con mensaje propio #parser
- [x] `paedrun`: corre un `.paed` en la terminal sin abrir la ventana SDL. Un test que hay que mirar no es un test #infra
- [x] `sintaxis.json` embebido en el binario: `paed` anda como archivo suelto, y el archivo del disco sigue ganando cuando existe #infra
- [x] `paed install` / `uninstall`, `--version`, cross-compile a `paed.exe`, y workflow de release #infra

***

## Archivado

%% kanban:settings
```
{"kanban-plugin":"board","show-checkboxes":true,"tag-colors":[],"move-tags":true}
```
%%

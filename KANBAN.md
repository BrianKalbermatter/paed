---

kanban-plugin: board

---

## Decisiones pendientes

- [ ] **Separador del CSV: `,` o `;`** — BLOQUEA la fase 2. Con `,` el archivo es portable (RFC 4180); con `;` lo abre Excel en es-AR sin preguntar. La gracia del CSV era abrirlo y ver la planilla #decidir #f2-csv
- [ ] De dónde sale el nombre del `.csv` en disco: ¿lo dice el programa en `ABRIR`, o sale del nombre de la variable? #decidir #f2-csv
- [ ] `CREAR` sobre un archivo que ya existe: ¿lo pisa o es error? #decidir #f2-csv
- [ ] Entrecomillado de texto con el separador adentro. Propuesto: RFC 4180 (comillas dobles, duplicando las internas). No choca con el `'` de PAED (§10.4) #decidir #f2-csv
- [ ] Qué valor exacto vale `HV`. `DBL_MAX` lo hace incomparable de verdad pero se imprime feo; `999999999` es legible y alcanza para las claves de los ejercicios #decidir #f3-algoritmo
- [ ] ¿`HV` distingue mayúsculas (`hv`, `Hv`)? Las keywords no distinguen, y esto es una constante del lenguaje: debería seguir la misma regla #decidir #f3-algoritmo
- [ ] Sintaxis de `ABRIR`: la wiki escribe `ABRIR(arch, lectura)` y la cátedra `Abrir E/(arch)` (`TEORIA_COMPLETA.txt:1104`). **Son incompatibles.** Hoy está implementada la de cátedra #decidir
- [ ] `SEGUN` — hay un conflicto que resolver antes de implementarlo #decidir
- [ ] `REPETIR`/`HASTA` — cero apariciones reales en el corpus, solo declaradas en `sintaxis.json`. Confirmar que existan antes de implementar #decidir
- [ ] `-2 ** 2` da 4 porque la tabla de prioridad pone los unarios ARRIBA de la potencia. En casi todos los lenguajes da -4. ¿Es lo que quiere AED? (`TEORIA_COMPLETA.txt:361-371`) #decidir
- [ ] `VARIABLES` como sub-sección de `AMBIENTE`: aparece en `AED_2021_UnI.pdf:10` y en ninguna otra fuente. Confirmar si es obligatoria antes de implementar #decidir
- [ ] `FIN ACCION` con espacio: hoy se rechaza a propósito (§10.7), pero hay evidencia de primera mano de cátedra. Revisar #decidir
- [ ] El `;` omitido en la última sentencia: la cátedra lo usa como separador, el parser lo exige como terminador (§10.8, §11.1) #decidir

## Backlog — Fase 1: la declaración parsea

- [ ] `data/sintaxis.json`: cláusulas `ORDENADO POR` e `INDEXADO POR` como modificadores de la declaración `ARCHIVO`, con las organizaciones nombradas (`secuencial`, `ordenado`, `indexado`) para que el asistente las lea de ahí y no de una lista en C #f1-declaracion
- [ ] `PAEDDecl` en `lang/include/paed/parser.h`: campos `org`, `clave[PAED_MAX_CLAVE][PAED_NAME_MAX]` y `clave_count`, al lado de `es_archivo`. `PAED_MAX_CLAVE` en 4 — el corpus llega a `clave3, clave2, clave1, clave0` #f1-declaracion
- [ ] `lang/src/parser.c`: parsear la cláusula después de resolver `ARCHIVO DE <tipo>`. Separar la lista por comas **y por la palabra `y`**, que el corpus usa antes del último campo (`ordenado por clave, tipo_novedad y f_novedad`) #f1-declaracion
- [ ] Validar los campos de la clave contra el `REGISTRO` del archivo. Va **después** de parsear todo el `AMBIENTE`, no durante: el archivo puede declararse antes que el registro #f1-declaracion
- [ ] `INDEXADO POR` con más de un campo es error: lleva uno solo #f1-declaracion
- [ ] Test `archivos_organizacion.paed`: las tres formas (sin cláusula, `ORDENADO POR` con lista y `y` final, `INDEXADO POR`) #f1-declaracion
- [ ] Test `archivos_organizacion_errores.paed`: campo que el registro no declara, `INDEXADO POR` con dos campos, cláusula sobre algo que no es archivo #f1-declaracion
- [ ] Actualizar la tabla de `PAED.md §12` cuando esto pase a ✅ — la tabla y el KANBAN se actualizan juntos #f1-declaracion #docs

## Backlog — Fase 2: el CSV existe en disco

- [ ] `CREAR` escribe el encabezado con los campos del `REGISTRO`, en orden de declaración #f2-csv
- [ ] `ABRIR` con su modo lee el encabezado y lo **compara contra el `REGISTRO`**. Si no coincide, error diciendo qué campo esperaba y cuál encontró. Es lo que ataja abrir el archivo equivocado, que hoy se leería sin protestar devolviendo basura con forma de dato válido #f2-csv
- [ ] `LEER(arch, reg)` trae la próxima fila y convierte cada columna al tipo declarado. Un `ENTERO` que en el archivo dice `abc` es error de lectura, **no** un cero silencioso #f2-csv
- [ ] `ESCRIBIR(arch, reg)` agrega una fila #f2-csv
- [ ] `FDA` verdadero cuando no quedan filas después del encabezado. Un archivo recién creado tiene solo encabezado: `FDA` desde el arranque #f2-csv
- [ ] `CERRAR` cierra y descarga #f2-csv
- [ ] Handle de archivo de verdad en el intérprete: hoy la forma se distingue y se valida, pero ninguna operación toca el disco #f2-csv
- [ ] Test de ida y vuelta: crear, escribir registros, cerrar, reabrir, leer entero hasta `FDA` — y que el `.csv` se abra en una planilla y se entienda #f2-csv

## Backlog — Fase 3: el algoritmo completo

- [ ] **`HV` (alto valor) como constante del lenguaje**, junto a `V`/`F` en `primario()` de `lang/src/expr.c`. No se declara, no se asigna, no ocupa entrada de variable. Hoy `SI (x <> HV)` da "la variable 'HV' no tiene valor todavía" #f3-algoritmo
- [ ] `HV` también en `sintaxis.json`, para que el resaltador lo pinte como constante y no como variable #f3-algoritmo
- [ ] Test de actualización secuencial: maestro + movimientos → maestro nuevo + bajas + errores, con los seis casos y el agotamiento de los dos archivos. **Es el ejercicio que toma la cátedra** #f3-algoritmo
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

- [ ] Declaración múltiple `A,B,SUMA: entero`. Es la forma del único ejemplo con autoridad de cátedra (`AED_2021_UnI.pdf:10`) y hoy da "nombre de variable invalido" #parser
- [ ] `FUNCION`/`PROCEDIMIENTO` anidados dentro de `AMBIENTE` #parser
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
- [ ] Pushear a `origin`: hay commits locales sin subir en `master` #infra

## En progreso

## Hecho

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

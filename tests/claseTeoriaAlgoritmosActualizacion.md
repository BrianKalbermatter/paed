Mecla - Actualizacion es indexada: Pero por ahora no es indexada, es actualizacion secuencial
Actualizacion -> Generica
Maestro - Actualizar maestro el movimiento y si esta queriendo hacer una alta, una baja, o una modificacion.
Tipos de bajas: hay bajas fisicas y logicas
Alta es un producto que se quiere dar de nuevo
Siempre se hace naja logica


# Ejercicio 2.2.19

En una Empresa Farmacéutica se posee un archivo MAE_REMEDIOS (ordenado por Clave: Farmacia + Medicamento), el que se actualiza semanalmente, a traves de la información que se encuentra cargada en un archivo de MOVIMIENTOS (ordenado por Clavem: Farmacia + Medicamento, y Cod_Mov), de la siguiente forma:

- Si Clave (MAE_REMEDIOS) es menor que Clavem (MOVIMIENTOS), simplemente se transfieren los datos del Maestro a la salida y se graban.

- Si Clave (MAE_REMEDIOS) es igual a Clavem (MOVIMIENTOS) y el Codmov es 1, se considera error y se lista un mensaje indicando el tipo de error; pero si el Codmov es 2, entonces es un remedio que deja de fabricarse y se transfiere el registro al archivo de Remedios vencidos (REM_VENC) ; pero si el Cod_Mov es 3, se modifica la cantidad actual con la cantidad recibida.

- Si Clave (MAE_REMEDIOS) es mayor que Clavem (MOVIMIENTOS) y el Codmov es 1, se incorpora el remedio a nuestro Vademecum, considerando que la cantidad recibida configura la cantidad actual y la Fecha_Vencimiento es 30 días posterior a la fecha actual; pero si el Codmov es 2 o 3 se considera error y se deben producir los correspondientes mensajes de error.

Se considera que solo existe un registro de movimiento para cada registro del maestro.

MAE_REMEDIOS Ordenado por Farmacia y Medicamento

    Farmacia Medicamento Cant_Actual Fecha_Vencimiento 

MOVIMIENTOS Ordenado por Farmacia, Medicamento y Cod_Mov

    Farmacia Medicamento Cod_Mov Cant_Recibida 

REM_VENC Ordenado por Medicamento

    Medicamento Cant_Vencida 

---

## La lógica de una actualización secuencial

Una actualización es una **mezcla con decisión**: se recorren Maestro y
Movimientos en paralelo (apareo por clave) y, en cada vuelta, se compara la
clave de ambos. De esa comparación salen exactamente tres casos:

| Comparación | Significado | Acción típica | Quién avanza |
|---|---|---|---|
| `Clave < Clavem` | Hay maestro sin movimiento | Se transfiere el registro tal cual a la salida | Maestro |
| `Clave = Clavem` | El movimiento afecta a un registro existente | Baja o modificación (el alta es error) | Ambos |
| `Clave > Clavem` | Hay movimiento sin maestro | Alta (baja y modificación son error) | Movimiento |

La regla general que se repite en todo ejercicio de actualización:

- **Alta** válida sólo cuando la clave **no existe** en el maestro (`>`).
- **Baja** y **modificación** válidas sólo cuando la clave **existe** (`=`).
- Cualquier otra combinación es **error** y se lista un mensaje.

### Técnica del valor alto (HV)

Cada archivo se lee a través de una subacción que, al llegar al fin de archivo,
carga la clave con `HV` (valor alto, mayor que cualquier clave posible). Así el
archivo agotado "pierde" siempre la comparación y el otro se sigue vaciando
solo, con un único ciclo incluyente:

```
Mientras (Clave <> HV) o (Clavem <> HV) hacer
```

Sin `HV` harían falta los tres ciclos del apareo excluyente. Es el mismo
mecanismo de [mezcla-apareo.md](mezcla-apareo.md), aplicado a la actualización.

### Baja lógica vs. baja física

- **Baja física**: el registro deja de existir en el maestro nuevo.
- **Baja lógica**: el registro queda, marcado con un campo de estado
  (`Activo: ("S","N")`), y se filtra al consultarlo.

En este ejercicio la baja es **física sobre el nuevo maestro**: el registro no
se graba en `NUE_MAE`, se transfiere a `REM_VENC`.

### Supuestos de este ejercicio

1. La salida es un **nuevo maestro** (`NUE_MAE`); el enunciado lo nombra sólo
   como "la salida".
2. `Cod_Mov = 3` acumula: `Cant_Actual := Cant_Actual + Cant_Recibida`. El
   enunciado dice "modifica la cantidad actual **con** la cantidad recibida",
   y en el alta usa otro verbo ("la cantidad recibida **configura** la cantidad
   actual", que sí es asignación). Si la cátedra lo interpreta como reemplazo,
   cambiá esa única línea por `:=`.
3. Sólo hay un movimiento por registro de maestro (lo dice el enunciado), por
   eso el caso `=` no necesita un ciclo interno de movimientos.

### Pseudocódigo

```
ACCION Actualizar_Remedios ES
    AMBIENTE

        REG_MAE = Registro
            Farmacia: AN(30); // Clave
            Medicamento: AN(30); // Clave
            Cant_Actual: N(5);
        Fecha_Vencimiento = Registro
            Dia: 1..31;
            Mes: 1..12;
            Anio: N(4);
        Fin Registro;
    Fin Registro;

    REG_MOV = Registro
        Farmacia: AN(30);
        Medicamento: AN(30);
        Cod_Mov: 1..3;
        Cant_Recibida: N(5);
    Fin Registro;

    REG_VENC = Registro
        Medicamento: AN(30);
        Cant_Vencida: N(5);
    Fin Registro;

    MAE_REMEDIOS: Archivo de REG_MAE  ordenado por Farmacia y Medicamento;
    MOVIMIENTOS : Archivo de REG_MOV  ordenado por Farmacia, Medicamento y Cod_Mov;
    NUE_MAE     : Archivo de REG_MAE  ordenado por Farmacia y Medicamento;
    REM_VENC    : Archivo de REG_VENC ordenado por Medicamento;

    mae: REG_MAE;
    mov: REG_MOV;
    ven: REG_VENC;
    alta: REG_MAE;                    // registro que se arma en el alta 

    Clave: AN(60);
    Clavem: AN(60);            // Farmacia + Medicamento concatenados 
    HV: ENTERO;                       // valor alto 

PROCESO
    HV:= 9999999999; // Es el valor mas grande
    // HV aca no se pone en la catedra, en PAED si.
    

    ABRIR E/(MAE_REMEDIOS);            //Arr(Sec)
    ABRIR E/(MOVIMIENTOS);             //Arr(Sec)
    ABRIR /S(NUE_MAE);
    ABRIR /S(REM_VENC);

    Leer_Maestro;                      //lecturas anticipadas 
    Leer_Movimiento;

    MIENTRAS (Clave <> HV) O (Clavem <> HV) HACER

        SI (Clave < Clavem) ENTONCES
            //Maestro sin movimiento: pasa tal cual
            Esc(NUE_MAE, mae)
            Leer_Maestro
        SINO
            SI (Clave = Clavem) ENTONCES

                SI (mov.Cod_Mov = 1) ENTONCES
                    ESCRIBIR("ERROR - Alta de un remedio ya existente: ", mov.Farmacia, mov.Medicamento);
                    Esc(NUE_MAE, mae);     // el maestro no se pierde
                SINO
                    SI mov.Cod_Mov = 2 entonces
                        //Baja: se transfiere a vencidos y NO se graba en NUE_MAE
                        ven.Medicamento  := mae.Medicamento;
                        ven.Cant_Vencida := mae.Cant_Actual;
                        ESCRIBIR(REM_VENC, ven);
                    SINO  // Cod_Mov = 3;
                        mae.Cant_Actual := mae.Cant_Actual + mov.Cant_Recibida;
                        ESCRIBIR(NUE_MAE, mae);
                    FIN_SI
                SIN_SI

                Leer_Maestro;          // avanzan los dos
                Leer_Movimiento;

            sino //Clave > Clavem

                SI (mov.Cod_Mov = 1) ENTONCES
                    alta.Farmacia    := mov.Farmacia;
                    alta.Medicamento := mov.Medicamento;
                    alta.Cant_Actual := mov.Cant_Recibida;
                    Vencimiento_A_30_Dias
                    Esc(NUE_MAE, alta)
                sino
                    Escribir('ERROR - Baja o modificacion de un remedio inexistente: ',
                             mov.Farmacia, mov.Medicamento);
                fin si

                Leer_Movimiento
            fin si
        fin si

    Fin Mientras

    Cerrar(MAE_REMEDIOS)
    Cerrar(MOVIMIENTOS)
    Cerrar(NUE_MAE)
    Cerrar(REM_VENC)

FinAccion


subaccion Leer_Maestro es
    Leer(MAE_REMEDIOS, mae)                 //Avz(Sec, v)
    Si FDA(MAE_REMEDIOS) entonces
        Clave := HV
    sino
        Clave := mae.Farmacia + mae.Medicamento
    fin si
fin subaccion


subaccion Leer_Movimiento es
    Leer(MOVIMIENTOS, mov)                  //Avz(Sec, v)
    Si FDA(MOVIMIENTOS) entonces
        Clavem := HV
    sino
        Clavem := mov.Farmacia + mov.Medicamento
    fin si
fin subaccion


subaccion Vencimiento_A_30_Dias es
    * Fecha_Actual es la fecha del sistema *
    alta.Fecha_Vencimiento := Fecha_Actual + 30 dias
fin subaccion
```

### Los tres errores clásicos en este tipo de ejercicio

1. **No avanzar el archivo correcto.** En `<` avanza sólo el maestro, en `>`
   sólo el movimiento, en `=` los dos. Si te equivocás, ciclo infinito.
2. **Perder el maestro en el caso de error.** Con `Clave = Clavem` y
   `Cod_Mov = 1` hay error, pero el registro del maestro **igual se graba**.
   Sólo la baja lo saca de la salida.
3. **Comparar campo por campo en vez de la clave completa.** La clave es
   `Farmacia + Medicamento`; armala una vez en la subacción de lectura y compará
   contra esa variable.

---

## Cómo pensar el recorrido

### El modelo mental: dos dedos sobre dos listas ordenadas

Poné un dedo en el primer registro del Maestro y otro dedo en el primer registro
de Movimientos. En cada vuelta mirás **sólo esos dos** y te preguntás una única
cosa: *¿cuál de las dos claves es más chica?*

**El dedo que apunta a la clave más chica es el que avanza.** Si empatan,
avanzan los dos.

Eso es todo. No hay más.

### Por qué esa regla es correcta

Porque los archivos están **ordenados ascendentemente**. Si la clave del maestro
es más chica que la del movimiento actual, entonces es más chica que la de
**todos los movimientos que faltan** — porque los que faltan son todavía más
grandes.

Conclusión: ese registro del maestro **nunca va a encontrar pareja**. Su destino
está decidido ahora mismo, no hace falta mirar nada más. Se graba y se avanza.

Ese razonamiento es el corazón del apareo, y es lo que te permite resolver todo
en **una sola pasada, sin volver atrás y sin leer un registro dos veces**.

### Las tres preguntas de cada vuelta

Cuando estés en el parcial, en cada vuelta del ciclo respondé en este orden:

1. **¿Quién es más chico?** → define el caso (`<`, `=`, `>`).
2. **¿Qué grabo?** → maestro tal cual, maestro modificado, alta nueva, vencido,
   o nada (baja) o sólo un mensaje (error).
3. **¿Quién avanza?** → el más chico, o los dos si empataron.

Si no contestás la 3, ciclo infinito. Si no contestás la 2, perdés registros.

### Prueba de escritorio

Datos de ejemplo (fijate que ambos archivos están ordenados por clave):

**MAE_REMEDIOS**

| Farmacia | Medicamento | Cant_Actual |
|---|---|---|
| F01 | AMOXI | 100 |
| F01 | IBU | 50 |
| F02 | ASPI | 200 |

**MOVIMIENTOS**

| Farmacia | Medicamento | Cod_Mov | Cant_Recibida |
|---|---|---|---|
| F01 | IBU | 3 | 20 |
| F01 | PARA | 1 | 80 |
| F02 | ASPI | 2 | — |

Recorrido, vuelta por vuelta:

| # | Clave (mae) | Clavem (mov) | Caso | Qué hace | Avanza |
|---|---|---|---|---|---|
| — | *lecturas anticipadas* | | | | |
| 1 | `F01AMOXI` | `F01IBU` | `<` | graba AMOXI(100) en NUE_MAE | Maestro |
| 2 | `F01IBU` | `F01IBU` | `=` Cod 3 | 50 + 20 = 70 → graba IBU(70) | Ambos |
| 3 | `F02ASPI` | `F01PARA` | `>` Cod 1 | alta → graba PARA(80) | Movimiento |
| 4 | `F02ASPI` | `F02ASPI` | `=` Cod 2 | baja → graba en REM_VENC, **nada** en NUE_MAE | Ambos |
| 5 | `HV` | `HV` | — | corta el ciclo | — |

Resultado:

**NUE_MAE** → AMOXI(100), IBU(70), PARA(80). *ASPI no está: se dio de baja.*
**REM_VENC** → ASPI, Cant_Vencida 200.

### Dos observaciones que valen oro

1. **La salida sale ordenada sola.** No hay que ordenarla después. Como siempre
   procesás primero la clave más chica, los registros se graban en orden
   ascendente por construcción. Mirá la vuelta 3: el alta de `PARA` se intercala
   en el lugar correcto sin que hagas nada especial.
2. **Cada registro se lee exactamente una vez.** Nunca retrocedés, nunca releés.
   Por eso funciona con archivos enormes que no entran en memoria: en cada
   momento sólo hay **un** registro de cada archivo cargado.

### Cómo verificar que tu algoritmo está bien

Contá los registros. Al terminar:

```
registros leídos del maestro = grabados en NUE_MAE + grabados en REM_VENC
                               (menos los que salieron por baja)
registros leídos de movimientos = altas + bajas + modificaciones + errores
```

Si un movimiento no cayó en ninguna de esas cuatro categorías, te falta una rama
del `Si`. Si un registro del maestro desapareció sin ser baja, tenés un `Esc`
faltante.


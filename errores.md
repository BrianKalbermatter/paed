# Errores de PAED

Cuando un programa no anda, PAED dice qué pasó, en qué línea, y **con qué
código**:

```
Hola.paed:6: error XL-01: la subaccion 'devolverCuadro' no tiene bloque PROCESO
             ─────┬─────
                  └── esto se busca acá
```

El **mensaje** explica qué pasó en *ese* programa. El **código** dice de qué
clase de error se trata. Sirven para cosas distintas: el mensaje se lee en el
momento, el código se busca cuando el mensaje no alcanzó — y buscar por mensaje
no funciona, porque los mensajes llevan el nombre de tu variable adentro.

Cada código tiene su explicación en [`xasolError/`](xasolError/), en un archivo
que se llama igual: `XL-01` → `xasolError/XLerror-01.md`.

## El índice

| Código | Qué clase de error es | Cuándo |
|---|---|---|
| [XL-01](xasolError/XLerror-01.md) | Falta un bloque de la subacción | al parsear |
| [XL-02](xasolError/XLerror-02.md) | Declaración de subacción mal formada | al parsear |
| [XL-03](xasolError/XLerror-03.md) | Nombre que no está en el `AMBIENTE` | al parsear |
| [XL-04](xasolError/XLerror-04.md) | Bloque sin cerrar, o cierre que no corresponde | al parsear |
| [XL-05](xasolError/XLerror-05.md) | Falta la condición de un bloque de control | al parsear |
| [XL-06](xasolError/XLerror-06.md) | `PARA` mal escrito | al parsear |
| [XL-07](xasolError/XLerror-07.md) | Declaración de variable mal formada | al parsear |
| [XL-08](xasolError/XLerror-08.md) | Procedimiento o función que no existe | al parsear |
| [XL-09](xasolError/XLerror-09.md) | Cantidad de argumentos incorrecta | al parsear |
| [XL-10](xasolError/XLerror-10.md) | Archivos: modo de apertura y organización | al parsear |
| [XL-11](xasolError/XLerror-11.md) | `REGISTRO` mal declarado | al parsear |
| [XL-12](xasolError/XLerror-12.md) | Asignación mal formada | al parsear |
| [XL-13](xasolError/XLerror-13.md) | Se pasó un límite del lenguaje | al parsear |
| [XL-14](xasolError/XLerror-14.md) | El programa no termina | al ejecutar |
| [XL-15](xasolError/XLerror-15.md) | Se acabaron los datos de entrada | al ejecutar |
| [XL-16](xasolError/XLerror-16.md) | El dato no corresponde al tipo declarado | al ejecutar |
| [XL-17](xasolError/XLerror-17.md) | La estructura del programa está fuera de orden | al parsear |
| [XL-18](xasolError/XLerror-18.md) | Se usa una variable antes de darle valor | al ejecutar |
| [XL-19](xasolError/XLerror-19.md) | CONJUNTO mal declarado, o usado sin declarar | al parsear y al ejecutar |

## Un error sin código

Pasa, y no es una falla: significa que ese error **todavía no está catalogado**.
Sale igual, con su mensaje completo y su línea, nada más que sin el `XL-NN`.

Los códigos son un catálogo que **crece con lo que vamos encontrando**. Cuando
un error se repite lo suficiente como para merecer una explicación, se lo
agrega.

## Cómo se agrega uno

Dos pasos, y ninguno toca el parser:

1. Una fila en `lang/src/errores.c`, con los pedazos de texto que reconocen al
   mensaje. Los pedazos son la parte **fija** — la que no cambia de un programa
   a otro. En `la subaccion 'devolverCuadro' no tiene bloque PROCESO`, lo que
   identifica al error es `no tiene bloque PROCESO`; el nombre de la subacción
   es del programa.
2. Su archivo en `xasolError/XLerror-NN.md`, siguiendo la forma de los que ya
   están: qué dice, por qué pasa, cómo se arregla.

El orden de la tabla no importa: gana el pedazo **más largo** que matchea, o sea
el más específico. Un mensaje como `falta FIN_REGISTRO: el registro 'r' quedó
abierto` matchea el `quedó abierto` de XL-04 y el `falta FIN_REGISTRO` de XL-11;
los dos son ciertos, pero el segundo dice más, y la regla del pedazo más largo
lo elige sola.

## Por dónde no se busca

Los errores **no** están en `data/sintaxis.json`, a diferencia de casi todo lo
demás del lenguaje. El motivo: los códigos no son una regla de PAED, son un
contrato entre los mensajes y esta documentación. Cambiarlos no cambia qué
programas son válidos.

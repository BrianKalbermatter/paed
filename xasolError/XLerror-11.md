# XL-11 — REGISTRO mal declarado

## Qué dice

```
error XL-11: falta FIN_REGISTRO: el registro 'alumno' de la linea 21 quedo abierto y aca ya empieza el PROCESO
error XL-11: el registro 'fecha' no tiene ningun campo
error XL-11: el registro 'remedio' no tiene un campo 'precio'
error XL-11: un campo de registro no puede ser un archivo: 'arch'
error XL-11: el registro 'alumno' ya se declaro en la linea 12
```

## Por qué pasa

Un `REGISTRO` agrupa campos bajo un nombre, y se declara con `=`, no con `:`:

```paed
remedio = REGISTRO
    farmacia: AN(4);
    medicamento: AN(20);
    cant: ENTERO;
FIN_REGISTRO
```

Los errores más comunes: olvidarse el `FIN_REGISTRO`, dejarlo vacío, o nombrar
en una cláusula `ORDENADO POR` un campo que el registro no tiene.

## Registro y variable son dos cosas

El `REGISTRO` es el **tipo**. Las variables de ese tipo se declaran aparte:

```paed
AMBIENTE
    remedio = REGISTRO          ← el tipo
        farmacia: AN(4);
    FIN_REGISTRO

    reg_mae: remedio;           ← una variable de ese tipo
    mae: ARCHIVO DE remedio;    ← un archivo de ese tipo
```

## Un campo no puede ser un archivo

Un archivo vive en disco, un campo vive adentro de un registro que se lee y se
escribe entero. Meter uno adentro del otro no tiene sentido, y PAED lo rechaza
en la declaración en vez de dejarlo fallar al ejecutar.

## Ver también

- [XL-04](XLerror-04.md) — otros bloques que quedan abiertos
- [XL-10](XLerror-10.md) — la cláusula que valida sus campos contra este registro

---

Volver al [índice de errores](../errores.md).

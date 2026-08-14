# Librería `escena` — v1.0

**Esto NO es parte del lenguaje PAED.** Es una librería de VimMon que agrega
procedimientos propios, igual que `stdlib/matematica.sh` agrega funciones al
intérprete bash. El pseudocódigo AED de la cátedra no conoce ninguno de estos
nombres.

Definición formal: [`../data/escena.json`](../data/escena.json).
El parser la carga **además** de `sintaxis.json`. Si el archivo no está, PAED
sigue andando como AED puro.

## Cómo se engancha (el código también lo cumple)

Hasta el 2026-08-11 esta separación existía **solo en el papel**: `CUBO`,
`MOVER`, `GIRAR` y otros nueve estaban escritos adentro de `interpreter.c`, y
`interp_exec` recibía el `SceneState`. El `.json` decía "esto no es el lenguaje"
y el `.c` los tenía adentro.

Ahora la escena vive en `plugins/ide/escena.{c,h}` — del lado de VimMon, no del
lenguaje — y se **registra**:

```c
escena_init(&scene);
escena_registrar(&scene);   // anota los 12 procedimientos apuntando a esta escena
interp_exec(&prog);         // el intérprete ya no sabe qué es una escena
```

Por debajo, cada uno se anota con `paed_register_proc(nombre, fn, ud)`. Es la
misma idea del bus de plugins (`bus_register`) y del puerto de entrada de `LEER`
(`interp_set_entrada`): el núcleo no conoce a sus extensiones, las extensiones
se anotan.

**Dos reglas que salen de ahí:**

1. **Los procedimientos del lenguaje ganan.** El registro se consulta *después*
   de `LEER`/`ESCRIBIR`, así que registrar un `LEER` propio no puede tapar al de
   AED. El host extiende el lenguaje, no lo redefine.
2. **Registrar justo antes de ejecutar.** El registro es uno solo para todo el
   proceso y guarda un *puntero* al estado: si dos partes de VimMon manejan
   escenas distintas, el último que anota es el que recibe los cuerpos.

Correrlo sin registrar nada es legítimo y es lo que hace `paedrun`:

```
$ build/paedrun solo_escena.paed
el lenguaje si anda
solo_escena.paed:6: error: 'CUBO' lo reconoce el parser pero no lo implementa
nadie: el host no registro ese procedimiento
```

---

## Convención propia: argumentos con nombre

AED llama posicional (`AVZ(sec, ventana)`). Esta librería usa argumentos con
nombre:

```
PROCEDIMIENTO(clave = valor, clave = valor);
```

Es una convención **de la librería**, no una regla del lenguaje. La razón: una
entidad de escena se referencia siempre igual, la cree o la modifique, y con
diez parámetros opcionales el orden posicional es inmanejable.

Los vectores van entre paréntesis: `posicion = (0,2,5)`. Los paréntesis le
permiten al parser distinguir la coma que separa las componentes del vector de
la coma que separa los argumentos.

## Crear entidades

```
CUBO  (nombre = <id>, posicion = (x,y,z), color = #hex, tamano = (x,y,z));
ESFERA(nombre = <id>, posicion = (x,y,z), color = #hex, radio  = n);
PLANO (nombre = <id>, posicion = (x,y,z), color = #hex, tamano = (x,y,z));
LUZ   (nombre = <id>, posicion = (x,y,z), tipo = puntual|dir, intensidad = 0.0-1.0);
```

## Modificar

```
MOVER  (nombre = <id>, posicion = (x,y,z));
ROTAR  (nombre = <id>, eje = x|y|z, angulo = grados);
ESCALAR(nombre = <id>, factor = n);
COLOR  (nombre = <id>, color = #hex);
```

## Comportamientos

```
GIRAR  (nombre = <id>, eje = x|y|z, velocidad = n);
OSCILAR(nombre = <id>, amplitud = n, frecuencia = n);
```

## Global

```
CAMARA(posicion = (x,y,z), mirar = (x,y,z));
FONDO (color = #hex);
```

## Ejemplo

```paed
ACCION escena ES
    AMBIENTE
        cubo1: CUBO;
    PROCESO
        FONDO (color = #1a1a2e);
        CAMARA(posicion = (0,2,5), mirar = (0,0,0));
        CUBO  (nombre = cubo1, posicion = (0,0,0), color = #ff0000, tamano = (1,1,1));
FIN_ACCION
```

`CUBO` como tipo en `AMBIENTE` es también de la librería: el parser guarda el
tipo como texto sin validarlo, así que no choca con los tipos de AED.

## Reglas para la IA

La IA genera SOLO instrucciones, sin el envoltorio `ACCION`/`PROCESO`/
`FIN_ACCION`. El plugin `ide` las inserta antes de `FIN_ACCION`.

- Si la entidad ya existe → `MOVER`, `COLOR`, `ESCALAR`, `ROTAR`.
- Si es nueva → el procedimiento de creación completo.
- Nunca repetir lo que ya está en la escena.

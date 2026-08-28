# Diccionario del motor — librería `mundo`
## Esto es del proyecto de Shulker_dev fuera de la catedra
> **Es para fines de divertirse creando cosas nuevas.**

```bash
make mundo
./build/mundo LaberintoMinotauro/laberinto.paed    # parado en la raíz de VimMon
```

---

## `USAR` — pedir la librería

```paed
USAR mundo;

ACCION laberinto ES
AMBIENTE
    ...
PROCESO
    ...
FIN_ACCION
```

# El esqueleto de todo juego

```paed
USAR mundo;
// Lo que todo juego debe tener siempre 
ACCION mi_juego ES
AMBIENTE
    ang, px, pz: REAL;
PROCESO
    INICIAR(titulo = 'Mi juego', ancho = 960, alto = 540);
    CAPTURAR_MOUSE(activar = 1);
    
    // Que es activar = 1; donde definis activar?
    ang := 90;  px := 0;  pz := 0;

    MIENTRAS SALIR() = 0 HACER
        // 1. LEER LA ENTRADA
        ang := ang + MOUSE_X() * 0.2;
        SI TECLA('W') ENTONCES
            pz := pz + 0.1;
        FIN_SI

        // 2. ACTUALIZAR EL MUNDO
        //    (mover enemigos, chequear choques...)

        // 3. DIBUJAR
        FRAME_INICIO(camx = px, camy = 0.5, camz = pz, angulo = ang);
            CUBO(x = 3, z = 5, color = 'gris');
            BILLBOARD(x = 8, z = 2, alto = 2, color = 'rojo');
        FRAME_FIN();
    FIN_MIENTRAS
FIN_ACCION
```

Los tres pasos —**leer, actualizar, dibujar**— son el game loop de cualquier juego que exista. Pong y Elden Ring tienen esta misma forma.

---

## FUNCIONES — preguntan algo

Devuelven un valor, así que van **adentro de una expresión**:
`SI TECLA('W') ENTONCES`, `t := TICKS();`

### `SALIR()` → `0` o `1`
`1` si cerraron la ventana.
> Es como el si presionas q y salis de la ventana sino es 0 no presionaste nada

**Llamala UNA vez por frame, siempre.** Por debajo vacía la cola de eventos del
sistema, y ahí es donde vive el movimiento del mouse. Si no la llamás, el mouse
se siente trabado y no vas a entender por qué.

```paed
MIENTRAS SALIR() = 0 HACER
```

### `TECLA(letra)` → `0` o `1`
`1` mientras la tecla esté apretada. Entre comillas.

Acepta: `'W'` `'A'` `'S'` `'D'` `' '` (espacio). Cualquier otra da error.

```paed
SI TECLA('A') ENTONCES
    ang := ang - 2;
FIN_SI
```

### `MOUSE_X()` → número
Cuánto se movió el mouse en horizontal **desde el cuadro anterior**.
Negativo = izquierda, positivo = derecha, `0` = quieto.

No es "dónde está el puntero": es **cuánto se movió**. Para mirar alrededor es
lo único que sirve.

```paed
ang := ang + MOUSE_X() * 0.2;    // 0.2 = sensibilidad
```

### `MOUSE_Y()` → número
Igual, en vertical. **Positivo es hacia ABAJO**, porque la pantalla cuenta desde
arriba.

### `CLIC(boton)` → `0` o `1`
Acepta `'izq'`, `'der'`, `'medio'`.

```paed
SI CLIC('izq') ENTONCES
```

### `TICKS()` → número
Milisegundos desde que arrancó el programa.

Sirve para que el juego vaya **igual de rápido en cualquier máquina**: en vez de
mover "3 por vuelta" (que en una PC rápida vuela), movés "3 por segundo" y
calculás cuánto pasó.

```paed
ahora := TICKS();
SI ahora - ultimo_paso > 500 ENTONCES     // medio segundo
```

---

## PROCEDIMIENTOS — hacen algo

No devuelven nada. Van **solos en su línea**, terminados en `;`.
Los argumentos llevan **nombre**: `CUBO(x = 3, z = 5)`.

### `INICIAR(titulo, ancho, alto)`
Abre la ventana. **Va una sola vez, antes que todo lo demás.**

| Parámetro | Por defecto | |
|---|---|---|
| `titulo` | `'PAED'` | Entre comillas |
| `ancho` | `960` | Mínimo 64 |
| `alto` | `540` | Mínimo 64 |

Por dentro dibuja a **la mitad** de ese tamaño y después lo estira sin suavizar.
Eso es lo que da el pixel art parejo, y hace que la placa pinte 4 veces menos
píxeles.

### `CAPTURAR_MOUSE(activar)`
`1` encierra el puntero en la ventana y lo esconde — lo que hace todo shooter.
`0` lo suelta.

### `FRAME_INICIO(camx, camy, camz, angulo, cielo)`
Empieza un cuadro. Todo lo que dibujes después va adentro de él.

| Parámetro | Por defecto | |
|---|---|---|
| `camx` `camy` `camz` | `0`, `0.5`, `0` | Dónde está la cámara |
| `angulo` | `0` | Hacia dónde mira, **en grados** |
| `cielo` | azul oscuro | Color de fondo |

`camy = 0.5` es la altura de los ojos si una celda mide 1.

El ángulo va en **grados** (0, 90, 180...) porque es como pensás con el mouse.
La cuenta a radianes la hace el motor.

### `CUBO(x, y, z, color)`
Un cubo de **una celda**. Es la pared del laberinto.

| Parámetro | Por defecto | |
|---|---|---|
| `x` `z` | *obligatorios* | Posición en el piso |
| `y` | `0` | Altura |
| `color` | `'blanco'` | Ver la tabla de abajo |

### `BILLBOARD(x, y, z, ancho, alto, color)`
Un sprite que **siempre encara a la cámara**: gires como gires, lo ves de frente.
Los enemigos, los ítems, todo lo que "vive".

| Parámetro | Por defecto | |
|---|---|---|
| `x` `z` | *obligatorios* | Posición |
| `y` | `0.5` | Altura del centro |
| `ancho` `alto` | `1` | Tamaño |
| `color` | `'blanco'` | |

**No lleva rotación, y esa es toda su gracia.** Así dibujaban los enemigos DOOM
y Duke Nukem 3D.

### `FRAME_FIN()`
Cierra el cuadro y lo muestra. Sin argumentos.

Si te lo olvidás, no ves nada.

---

## Los colores

`'gris'` `'rojo'` `'verde'` `'azul'` `'amarillo'` `'negro'` `'blanco'`

Con comillas. Cualquier otro nombre da error con la lista completa.

Tienen nombre y no número hexadecimal a propósito: estás aprendiendo a
programar, no a leer `0xFF8C8C96`.

---

## Los valores pueden ser CUENTAS

Todos los números aceptan variables y expresiones, no solo números escritos:

```paed
CUBO(x = col * 2, z = fila * 2, color = 'gris');
FRAME_INICIO(camx = px + 0.5, camy = 0.5, camz = pz, angulo = ang);
```

Se resuelven cuando el programa corre, con las variables ya cargadas.

---

## Errores que vas a ver

| Mensaje | Qué pasó |
|---|---|
| `procedimiento desconocido 'CUBO'` | Falta `USAR mundo;` arriba de todo |
| `falta INICIAR antes de dibujar` | Dibujaste antes de abrir la ventana |
| `INICIAR ya se llamo: va una sola vez` | Está adentro del `MIENTRAS` |
| `no pude abrir la ventana` | Falta un driver Vulkan |
| `TECLA no conoce 'X'` | Solo W, A, S, D y espacio |
| `color desconocido` | Mirá la lista de arriba |
| `no encontre la libreria 'mundo'` | No estás parado en la raíz de VimMon |

---

## Dónde vive cada cosa

| | |
|---|---|
| `plugins/mundo/pl_mundo.c` | Los once verbos, en C |
| `plugins/mundo/host_mundo.c` | El host: hospeda el intérprete |
| `paed/data/mundo.json` | Declara los procedimientos al parser |
| `plugins/renderer3d/` | El motor que dibuja |

Las **funciones** no están en el `.json`: se resuelven cuando el programa corre.
Solo los **procedimientos** se declaran ahí.

PAED — el pseudocódigo AED de la cátedra, ejecutable.

## Instalar — un solo archivo, desde cualquier carpeta

```bash
curl -L https://github.com/BrianKalbermatter/paed/releases/latest/download/paed -o paed
chmod +x paed
./paed install
```

Eso es todo. El binario **lleva la definición del lenguaje adentro**, así que
anda solo, sin nada al lado. `paed install` se copia a sí mismo a `/usr/local`
(o a `~/.local` si no puede escribir ahí), escribe su `sintaxis.json` y
**se agrega solo al PATH**: detecta tu shell y escribe la línea en `.bashrc`,
`.zshrc` o `config.fish` según corresponda. Abrís una terminal nueva y el
comando `paed` ya existe.

Para elegir el destino: `./paed install /opt/paed`.

También está el paquete completo, que además trae el README y la licencia:

```bash
curl -L https://github.com/BrianKalbermatter/paed/releases/latest/download/paed-linux-x86_64.tar.gz | tar xz
cd paed-*-linux-x86_64 && ./bin/paed install
```

## Windows

```
paed.exe programa.paed
```

Un solo archivo, sin instalador: el `.exe` lleva la definición del lenguaje
adentro. Bajalo del listado de abajo y ponelo donde quieras. Si querés
escribir `paed` sin la ruta, `paed.exe install` lo copia a tu carpeta de
usuario y te dice cómo agregarlo al PATH. En Windows la línea del PATH la
tenés que correr vos: ahí el PATH vive en el registro, no en un archivo.

## Usar

```bash
paed tu_programa.paed
```

Los datos de `LEER` entran por teclado, o por una tubería:

```bash
printf '10\n20\n' | paed suma.paed
```

Otros comandos:

```bash
paed --version      # que version es
paed --help         # todo lo que sabe hacer
paed uninstall      # se borra a si mismo de donde este instalado
```

`uninstall` sin argumentos deduce de dónde sacarse: el binario que corre **es**
el instalado. Borra solo lo que puso `install` — si en el directorio de datos
hay librerías de otro, las deja y avisa.

## Qué corre hoy

`ACCION` / `AMBIENTE` / `PROCESO`, `SI` / `MIENTRAS` / `PARA` / `REPETIR` /
`SEGUN`, `FUNCION` y `PROCEDIMIENTO`, registros y registros anidados,
arreglos con límites chequeados, conjuntos y rangos, expresiones con la tabla
de prioridad de la cátedra, y `LEER` / `ESCRIBIR` de consola.

**Archivos en disco**: `ARCHIVO DE`, `ABRIR E/ S/ E/S`, `CREAR`, `LEER`,
`ESCRIBIR` / `GRABAR`, `CERRAR`, `FDA` / `NFDA`, `ORDENADO POR`,
`INDEXADO POR` y `HV` — alcanza para corte de control, mezcla y actualización
secuencial. Cada archivo es un `.csv` con encabezado, que se puede abrir y
mirar.

**Secuencias**: `SECUENCIA DE`, `ARR` / `AVZ` / `NFDS` / `FDS`, `VENTANA`.

**Todavía no**: matrices (`ARREGLO[1..3, 1..3]`), punteros y listas.

## Novedades de esta versión

- `install` se agrega solo al PATH, con la sintaxis de tu shell.
- `ABRIR S/` crea el archivo de salida, como escribe la cátedra. Antes exigía
  que existiera, que es justo lo que no pasa con un archivo generado.
- Un arreglo llamado `V` o `F` se puede indexar. `V` es también el literal
  verdadero, y se resolvía antes de mirar el corchete — justo el nombre que usa
  la cátedra para sus vectores.
- El resaltado de Helix reconoce `ACCION 223 ES` y `ACCION 2.2.4 ES`.

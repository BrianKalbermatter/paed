PAED — el pseudocódigo AED de la cátedra, ejecutable.

## Instalar — un solo archivo, desde cualquier carpeta

```bash
curl -L https://github.com/BrianKalbermatter/paed/releases/latest/download/paed -o paed
chmod +x paed
./paed install
```

Eso es todo. El binario **lleva la definición del lenguaje adentro**, así que
anda solo, sin nada al lado, y `paed install` se copia a sí mismo a
`/usr/local` (o a `~/.local` si no puede escribir ahí) y escribe su
`sintaxis.json`.

Para elegir el destino: `./paed install /opt/paed`.

También está el paquete completo, que además trae el README y la licencia:

```bash
curl -L https://github.com/BrianKalbermatter/paed/releases/latest/download/paed-linux-x86_64.tar.gz | tar xz
cd paed-*-linux-x86_64 && ./bin/paed install
```

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
```

## Qué corre hoy

`ACCION` / `AMBIENTE` / `PROCESO`, `SI` / `MIENTRAS` / `PARA`, arreglos con
límites chequeados, registros, expresiones con la tabla de prioridad de la
cátedra, y `LEER` / `ESCRIBIR` de consola.

**Todavía no**: archivos en disco, `SECUENCIA`, `HV`, `SEGUN`, `REPETIR`.

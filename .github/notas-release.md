PAED — el pseudocódigo AED de la cátedra, ejecutable.

## Instalar

```bash
curl -L https://github.com/BrianKalbermatter/paed/releases/latest/download/paed-linux-x86_64.tar.gz | tar xz
cd paed-*-linux-x86_64 && ./instalar.sh
```

No hace falta compilador ni clonar el repo: el binario viene armado. Son 41 KB.

También anda sin instalarlo, directamente desde la carpeta descomprimida:

```bash
./paed-*-linux-x86_64/bin/paed tu_programa.paed
```

## Usar

```bash
paed tu_programa.paed
```

Los datos de `LEER` entran por teclado, o por una tubería:

```bash
printf '10\n20\n' | paed suma.paed
```

## Qué corre hoy

`ACCION` / `AMBIENTE` / `PROCESO`, `SI` / `MIENTRAS` / `PARA`, arreglos con
límites chequeados, registros, expresiones con la tabla de prioridad de la
cátedra, y `LEER` / `ESCRIBIR` de consola.

**Todavía no**: archivos en disco, `SECUENCIA`, `HV`, `SEGUN`, `REPETIR`.

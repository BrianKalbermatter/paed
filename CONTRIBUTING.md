# Cómo aportar a PAED

## La regla corta

Nadie escribe directo en la rama principal (`feat/lsp-y-modulos`). Todo entra
por Pull Request, y lo aprueba Brian. Pero **crear ramas y probar es libre**:
rompé lo que quieras en la tuya.

## Si no tenés permiso de escritura (lo normal)

El repo es público, así que no necesitás que nadie te agregue.

1. **Fork** en GitHub, con el botón de arriba a la derecha.
2. Cloná tu fork y hacé tu rama:

   ```bash
   git clone https://github.com/TU-USUARIO/paed.git
   cd paed
   git checkout -b fix/lo-que-arreglas
   ```

3. Compilá y probá:

   ```bash
   make
   make test
   ```

4. Commiteá, pusheá a **tu** fork y abrí el Pull Request contra
   `feat/lsp-y-modulos`.

## Si tenés permiso de escritura

Igual, pero la rama la creás acá y no en un fork:

```bash
git clone -b feat/lsp-y-modulos https://github.com/BrianKalbermatter/paed.git
cd paed
git checkout -b fix/lo-que-arreglas
```

Pushear directo a `feat/lsp-y-modulos` está bloqueado. No es desconfianza: es
que un push directo se saltea los tests y la revisión.

## Antes de abrir el PR

- **`make test` tiene que pasar.** Son 51 tests más el tutorial. Si algo falla,
  no abras el PR todavía.
- Compilá sin warnings: el proyecto usa `-Wall -Wextra`.
- Un PR, un tema. Si arreglaste tres cosas distintas, son tres ramas y tres PR:
  así se puede revertir una sin tocar las otras.

## Los mensajes de commit

Conventional commits, en español y sin acentos:

```
fix(paed): ABRIR S/ crea el archivo de salida, como la catedra
feat(helix): el resaltado reconoce los conjuntos
docs(paed): E y S son el modo de ABRIR, no de un parametro
```

Los scopes que se usan: `paed` (el intérprete), `helix` (el resaltado),
`aprender` (el tutorial), `lang`, `kanban`.

**El cuerpo del commit importa más que el título.** Explicá *por qué* estaba
mal, no *qué* tocaste — el diff ya dice qué tocaste. Mirá `git log` para ver el
tono.

## Qué NO va en un PR

- Archivos generados: `build/` y `helix/src/` están en `.gitignore` a propósito.
- Cambios de formato mezclados con cambios de lógica.
- Sintaxis nueva del lenguaje sin fuente de la cátedra que la respalde. PAED
  implementa el pseudocódigo de AED, no un dialecto propio: si algo no está en
  el material de la cátedra ni en la wiki, se discute antes de escribirlo.

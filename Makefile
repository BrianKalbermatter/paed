# ═══════════════════════════════════════════════════════════════════════════
# PAED — el lenguaje
#
# Se instala y se usa SOLO, como cualquier interprete: no depende de VimMon, ni
# del editor, ni de ningun otro repo. Los que dependen de PAED son ellos.
#
#   make           libpaed.a + el binario `paed`
#   make test      corre todos los .paed de tests/
#   make install   deja `paed` y sus datos en el sistema
#
# Es C puro, sin SDL: compila en cualquier lado.
# ═══════════════════════════════════════════════════════════════════════════

CC       = clang
BUILDDIR = build

# El default es `all`, explicito. Sin esto make toma el PRIMER target del
# archivo, y alcanza con agregar una regla mas arriba para que `make` a secas
# deje de compilar sin avisar.
.DEFAULT_GOAL := all

# ── El lenguaje: libpaed.a + el binario `paed` ─────────────────────────────
#
# PREFIX es donde se instala. /usr/local es lo estandar para lo que uno compila
# a mano; con `make install PREFIX=$HOME/.local` no hace falta sudo.
PREFIX  ?= /usr/local
BINDIR   = $(PREFIX)/bin
DATADIR  = $(PREFIX)/share/paed

# La ruta de instalacion se COMPILA adentro del binario: asi `paed` encuentra
# sintaxis.json desde cualquier carpeta, sin variables de entorno. $(PAED_HOME)
# la sigue pisando para desarrollo.
# La version sale de un archivo y no del codigo: el tag de git, el `paed -v` y
# el nombre del paquete tienen que decir lo mismo, y con tres lugares distintos
# tarde o temprano no lo dicen.
VERSION := $(shell cat VERSION)

# -MMD -MP: el compilador ESCRIBE, al lado de cada .o, un .d que dice de que
# headers depende. Se incluyen mas abajo, y con eso make se entera de que tocar
# un .h obliga a recompilar los .c que lo usan.
#
# Sin esto el build se rompia CALLADO y de la peor manera: agregar un campo a
# PAEDInstr recompilaba parser.c pero no main.c, y quedaban dos objetos con dos
# tamaños distintos del mismo struct. El binario compilaba, linkeaba, y moria en
# cualquier programa con "*** stack smashing detected ***" — un mensaje que
# manda a buscar un desborde de buffer que no existe, mientras el error real es
# que main.c reserva el struct chico y parser.c escribe el grande.
LANG_CFLAGS = -Wall -Wextra -Ilang/include -Ilang/vendor/cjson \
              -DPAED_DATADIR='"$(DATADIR)"' -DPAED_VERSION='"$(VERSION)"' \
              -MMD -MP

# sintaxis.json va EMBEBIDO adentro del binario. Es lo que hace que `paed` sea
# un solo archivo que se puede bajar suelto y ya funcione: sin esto, el binario
# sin su .json al lado no sabe ni que es una palabra clave.
#
# El archivo del disco SIGUE GANANDO cuando existe (ver paed_datadir): asi se
# puede tocar la definicion del lenguaje sin recompilar, que es como venimos
# trabajando. Lo embebido es la ultima red.
LANG_GEN = lang/src/sintaxis_embebida.c

$(LANG_GEN): data/sintaxis.json
	@mkdir -p $(dir $@)
	@echo '// GENERADO por el Makefile desde data/sintaxis.json — NO EDITAR' > $@
	@echo 'const char PAED_SINTAXIS_EMBEBIDA[] =' >> $@
	@sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $< >> $@
	@echo ';' >> $@

# Los ejercicios del tutorial viajan adentro del binario por el mismo motivo
# que sintaxis.json: `paed` tiene que ser UN archivo que se baja suelto y ya
# trae todo. `paed aprender init` los desempaca en una carpeta de trabajo.
#
# La generacion vive en un script y no aca porque son N archivos con nombres
# variables, y eso en sed puro es ilegible.
APRENDER_GEN = lang/src/ejercicios_embebidos.c
APRENDER_SRC = $(sort $(wildcard aprender/[0-9]*.paed))

$(APRENDER_GEN): $(APRENDER_SRC) aprender/modulos.txt aprender/generar.sh
	@mkdir -p $(dir $@)
	@bash aprender/generar.sh > $@

LANG_SRC = lang/src/parser.c lang/src/expr.c lang/src/interpreter.c \
           lang/src/secuencia.c lang/src/archivo.c lang/src/aprender.c \
           lang/src/colores.c lang/src/asistente.c lang/src/datos.c \
           lang/src/errores.c \
           lang/src/plataforma.c $(LANG_GEN) $(APRENDER_GEN) \
           lang/vendor/cjson/cJSON.c
LANG_OBJ = $(LANG_SRC:%.c=$(BUILDDIR)/lang/%.o)
CLI_OBJ  = $(BUILDDIR)/lang/lang/cli/main.o
LSP_OBJ  = $(BUILDDIR)/lang/lang/lsp/main.o

# Los .d que dejo el compilador. Van con '-include' (con guion) y no con
# 'include': la primera vez todavia no existen, y sin el guion make aborta en
# vez de compilarlos.
-include $(LANG_OBJ:.o=.d) $(CLI_OBJ:.o=.d) $(LSP_OBJ:.o=.d)

LIB      = $(BUILDDIR)/libpaed.a
CLI      = $(BUILDDIR)/paed
LSP      = $(BUILDDIR)/paed-lsp

all: $(LIB) $(CLI) $(LSP)
lang: all   # nombre viejo, cuando el editor vivia en este mismo Makefile

$(LIB): $(LANG_OBJ)
	@mkdir -p $(dir $@)
	ar rcs $@ $(LANG_OBJ)

# El servidor de lenguaje. Misma libreria que el interprete: un error que ves
# subrayado en el editor es EXACTAMENTE el que vas a ver al correr el programa.
$(LSP): $(LSP_OBJ) $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $< $(LIB) -lm -o $@

$(CLI): $(CLI_OBJ) $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $< $(LIB) -lm -o $@

# Todo lo que se compila ADENTRO del binario y NO viene de un archivo .c va en
# este sello: la ruta de datos y la version. Make mira fechas de ARCHIVOS, y una
# flag de linea de comandos no es un archivo, asi que sin esto no se entera de
# que cambiaron y no recompila.
#
# El sello se reescribe SOLO si el contenido cambio (por eso el cmp): si se
# reescribiera siempre, su fecha cambiaria en cada `make` y recompilaria todo al
# pedo.
#
# Ya mordio dos veces:
#   - `make install PREFIX=~/.local` copiaba un binario que seguia buscando en
#     /usr/local/share/paed
#   - subir VERSION a 0.1.2 dejaba un `paed --version` que seguia diciendo 0.1.1
SELLO = $(BUILDDIR)/lang/.sello

$(SELLO): FORCE_SELLO
	@mkdir -p $(dir $@)
	@echo '$(DATADIR) $(VERSION)' | cmp -s - $@ || echo '$(DATADIR) $(VERSION)' > $@

FORCE_SELLO:

$(BUILDDIR)/lang/%.o: %.c $(SELLO)
	@mkdir -p $(dir $@)
	$(CC) $(LANG_CFLAGS) -c $< -o $@

# ── Windows (cross-compile con mingw-w64) ──────────────────────────────────
#
#   make windows    ->  build/windows/paed.exe
#
# Va a un BUILDDIR aparte a proposito: los .o de Linux y los de Windows tienen
# el mismo nombre y distinto formato, asi que compartir carpeta significa que un
# `make windows` te deja el build de Linux roto — y el error aparece despues, al
# linkear, culpando a otra cosa.
#
# El .exe no necesita nada al lado: la definicion del lenguaje viaja adentro.
# Por eso alcanza con pasarle el .exe a alguien, sin instalador ni carpetas.
WIN_CC      = x86_64-w64-mingw32-gcc
WIN_BUILD   = $(BUILDDIR)/windows
WIN_CFLAGS  = -Wall -Wextra -Ilang/include -Ilang/vendor/cjson \
              -DPAED_DATADIR='"C:\\\\paed"' -DPAED_VERSION='"$(VERSION)"' \
              -MMD -MP
WIN_OBJ     = $(LANG_SRC:%.c=$(WIN_BUILD)/%.o) $(WIN_BUILD)/lang/cli/main.o
WIN_TARGET  = $(WIN_BUILD)/paed.exe

# Mismo motivo que en el build de Linux: sin las dependencias de headers, tocar
# un .h deja objetos con dos layouts del mismo struct.
-include $(WIN_OBJ:.o=.d)

windows: $(WIN_TARGET)

$(WIN_TARGET): $(WIN_OBJ)
	@mkdir -p $(dir $@)
	$(WIN_CC) $(WIN_OBJ) -o $@
	@echo
	@echo "listo: $@"

$(WIN_BUILD)/%.o: %.c $(SELLO)
	@mkdir -p $(dir $@)
	$(WIN_CC) $(WIN_CFLAGS) -c $< -o $@

# Corre toda la bateria de programas PAED contra la salida que cada uno declara.
test: $(CLI)
	@bash tests/correr.sh
	@echo
	@bash aprender/verificar.sh

# El tutorial solo, cuando estas tocando los ejercicios y no el lenguaje.
test-aprender: $(CLI)
	@bash aprender/verificar.sh

# ── Instalacion ────────────────────────────────────────────────────────────
#
# Dos cosas van al sistema: el binario y los DATOS. Sin los datos, `paed` no
# sabe que es una palabra clave — sintaxis.json no es un extra, es el lenguaje.
install: all
	install -d $(BINDIR) $(DATADIR)
	install -m 755 $(CLI) $(BINDIR)/paed
	install -m 644 data/sintaxis.json $(DATADIR)/
	@echo
	@echo "PAED instalado:"
	@echo "  binario  $(BINDIR)/paed"
	@echo "  datos    $(DATADIR)/"
	@echo
	@echo "Probalo:  paed <archivo.paed>"

uninstall:
	rm -f  $(BINDIR)/paed
	rm -rf $(DATADIR)

# `clean-lang` es el nombre que usa VimMon desde su Makefile. Se mantiene como
# alias de `clean` para no romperlo: existia cuando este archivo tambien
# compilaba el editor y habia que distinguir que se borraba.
clean clean-lang:
	rm -rf $(BUILDDIR)

.PHONY: all lang windows test test-aprender install uninstall clean clean-lang FORCE_SELLO

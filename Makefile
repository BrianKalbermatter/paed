# ═══════════════════════════════════════════════════════════════════════════
# PAED — el lenguaje
#
# Se instala y se usa SOLO, como cualquier interprete: no depende de VimMon, ni
# del editor, ni de ningun otro repo. Los que dependen de PAED son ellos.
#
#   make           libpaed.a + el binario `paed`
#   make test      corre todos los .paed de Frankly/tests
#   make install   deja `paed` y sus datos en el sistema
#
# Es C puro, sin SDL: compila en cualquier lado.
# ═══════════════════════════════════════════════════════════════════════════

CC       = clang
BUILDDIR = build

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
LANG_CFLAGS = -Wall -Wextra -Ilang/include -Ilang/vendor/cjson \
              -DPAED_DATADIR='"$(DATADIR)"'

LANG_SRC = lang/src/parser.c lang/src/expr.c lang/src/interpreter.c \
           lang/vendor/cjson/cJSON.c
LANG_OBJ = $(LANG_SRC:%.c=$(BUILDDIR)/lang/%.o)

LIB      = $(BUILDDIR)/libpaed.a
CLI      = $(BUILDDIR)/paed

all: $(LIB) $(CLI)
lang: all   # nombre viejo, cuando el editor vivia en este mismo Makefile

$(LIB): $(LANG_OBJ)
	@mkdir -p $(dir $@)
	ar rcs $@ $(LANG_OBJ)

$(CLI): $(BUILDDIR)/lang/lang/cli/main.o $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $< $(LIB) -lm -o $@

# La ruta de datos se compila ADENTRO del binario, asi que cambiar PREFIX obliga
# a recompilar. Make no puede darse cuenta solo: mira fechas de ARCHIVOS, y una
# flag de linea de comandos no es un archivo. Se deja la ruta escrita en un
# sello y los objetos dependen de el.
#
# El sello se reescribe SOLO si el valor cambio (por eso el cmp): si se
# reescribiera siempre, su fecha cambiaria en cada `make` y recompilaria todo al
# pedo.
#
# Sin esto: `make install PREFIX=~/.local` copiaba un binario que seguia
# buscando en /usr/local/share/paed y fallaba con "no encuentro sintaxis.json".
SELLO = $(BUILDDIR)/lang/.datadir

$(SELLO): FORCE_SELLO
	@mkdir -p $(dir $@)
	@echo '$(DATADIR)' | cmp -s - $@ || echo '$(DATADIR)' > $@

FORCE_SELLO:

$(BUILDDIR)/lang/%.o: %.c $(SELLO)
	@mkdir -p $(dir $@)
	$(CC) $(LANG_CFLAGS) -c $< -o $@

# Corre toda la bateria de programas PAED contra la salida que cada uno declara.
test: $(CLI)
	@bash Frankly/tests/correr.sh

# ── Instalacion ────────────────────────────────────────────────────────────
#
# Dos cosas van al sistema: el binario y los DATOS. Sin los datos, `paed` no
# sabe que es una palabra clave — sintaxis.json no es un extra, es el lenguaje.
install: all
	install -d $(BINDIR) $(DATADIR)
	install -m 755 $(CLI) $(BINDIR)/paed
	install -m 644 Frankly/data/sintaxis.json $(DATADIR)/
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

.PHONY: all lang test install uninstall clean clean-lang FORCE_SELLO

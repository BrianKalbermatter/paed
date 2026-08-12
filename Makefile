# ═══════════════════════════════════════════════════════════════════════════
# PAED — el lenguaje y su editor
#
# Este proyecto se instala y se usa SOLO, como cualquier lenguaje: no depende
# de VimMon ni de ningun otro repo. VimMon lo tiene adentro como submodulo y lo
# consume como libreria, que es una relacion de una sola direccion.
#
# Tres cosas se construyen aca:
#
#   make lang      libpaed.a + el binario `paed`   (el LENGUAJE, sin SDL)
#   make           el editor PseudoGames (`aed`)   (necesita SDL2)
#   make install   deja `paed` y sus datos en el sistema
#
# `lang` no necesita SDL: es C puro y se compila en cualquier lado.
# ═══════════════════════════════════════════════════════════════════════════

# `make` a secas sigue compilando el EDITOR, como siempre. Sin esto el default
# pasaria a ser el primer target del archivo, que ahora es `lang`.
.DEFAULT_GOAL := all

# ── Linux (clang) ──────────────────────────────────────────────────────────
CC     = clang
CFLAGS = -Wall -Wextra -I src
LDFLAGS = -lSDL2 -lSDL2_ttf -lSDL2_mixer -lSDL2_image -lm -lutil

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

lang: $(LIB) $(CLI)

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
install: lang
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

# Solo lo del lenguaje: el editor tiene su propio `clean`, que borra build/
# entero. Existe para que VimMon pueda limpiar lo que el uso sin llevarse
# puesto lo que no es suyo.
clean-lang:
	rm -rf $(BUILDDIR)/lang $(LIB) $(CLI)

.PHONY: lang test install uninstall clean-lang FORCE_SELLO

SRC = src/main.c src/ui.c src/niveles.c src/progreso.c src/cJSON.c \
      src/screenDOC.c src/screenMenu.c src/screenEditorLvl.c \
      src/screenLvLs.c src/screenEditorFree.c src/editorText.c \
      src/screenSoluction.c src/screenPomodoro.c src/pomodoro_bg.c \
      src/screenConfig.c src/screenFeedback.c src/screenPJ.c \
      src/screenVerificador.c src/audio.c src/config.c \
      src/screenTutorial.c src/shell.c src/editorBim.c \
      src/screenCEditor.c

OBJ     = $(addprefix $(BUILDDIR)/, $(notdir $(SRC:.c=.o)))
TARGET  = aed

all: $(BUILDDIR) $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# ── Windows cross-compile (mingw-w64) ──────────────────────────────────────
WIN_CC     = x86_64-w64-mingw32-gcc
WIN_LIBS   = win-libs
WIN_CFLAGS = -Wall -Wextra -I src -I $(WIN_LIBS)/include -DSIN_AUDIO
WIN_LDFLAGS = -L $(WIN_LIBS)/lib \
              -lmingw32 -lSDL2main \
              -lSDL2 -lSDL2_ttf -lm \
              -mwindows \
              -lole32 -loleaut32 -limm32 -lwinmm -lversion \
              -lsetupapi -lgdi32 -lcomdlg32 \
              -static-libgcc -static-libstdc++

WIN_OBJ    = $(addprefix $(BUILDDIR)/, $(notdir $(SRC:.c=.win.o)))
WIN_RC_OBJ = $(BUILDDIR)/app.win.o
WIN_TARGET = PseudoGames.exe

windows: $(BUILDDIR) $(WIN_TARGET)

$(WIN_RC_OBJ): src/app.rc assets/Icono/icon.ico | $(BUILDDIR)
	x86_64-w64-mingw32-windres src/app.rc -o $@

$(WIN_TARGET): $(WIN_OBJ) $(WIN_RC_OBJ)
	$(WIN_CC) $(WIN_OBJ) $(WIN_RC_OBJ) -o $@ $(WIN_LDFLAGS)

$(BUILDDIR)/%.win.o: src/%.c | $(BUILDDIR)
	$(WIN_CC) $(WIN_CFLAGS) -c $< -o $@

# ── Limpieza ────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILDDIR) $(TARGET) $(WIN_TARGET)

.PHONY: all windows clean

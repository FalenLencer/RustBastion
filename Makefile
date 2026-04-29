# ═══════════════════════════════════════════════════════════════
#  RUST BASTION — Makefile
#  Cibles : linux | win | package_win | zip_win | clean
# ═══════════════════════════════════════════════════════════════

# ── Sources ─────────────────────────────────────────────────────
SRC = Src/main.c       Src/game_state.c  Src/map_gen.c    \
      Src/pathfinding.c Src/renderer.c   Src/theme.c      \
      Src/enemy.c       Src/wave.c       Src/tower.c      \
      Src/unit.c        Src/meta.c       Src/ui.c      	  \
	  Src/save.c        Src/menu.c      \

# ── Dossiers ────────────────────────────────────────────────────
ASSETS_DIR   = assets
RELEASE_DIR  = release
WIN_DIR      = $(RELEASE_DIR)/win/rustbastion
LINUX_DIR    = $(RELEASE_DIR)/linux/rustbastion
BUILD_DIR    = build
OBJ_DIR      = $(BUILD_DIR)/obj

# ── ZIP ─────────────────────────────────────────────────────────
ZIP_WIN      = $(RELEASE_DIR)/rustbastion_win.zip
ZIP_LINUX    = $(RELEASE_DIR)/rustbastion_linux.tar.gz

# ════════════════════════════════════════════════════════════════
#  LINUX
# ════════════════════════════════════════════════════════════════
CC_LINUX      = gcc
CFLAGS_LINUX  = -Wall -Wextra -O2 -Isrc
LDFLAGS_LINUX = -lraylib -lGL -lm -lpthread -ldl -lrt \
                -lX11 -lXi -lXcursor -lXrandr -lXinerama
OUT_LINUX     = $(BUILD_DIR)/rustbastion

# ════════════════════════════════════════════════════════════════
#  WINDOWS (cross-compilation depuis Linux)
# ════════════════════════════════════════════════════════════════
CC_WIN          = x86_64-w64-mingw32-gcc
WINDRES         = x86_64-w64-mingw32-windres
RAYLIB_WIN_PATH = ./raylib_win

CFLAGS_WIN  = -Wall -Wextra -O2                          \
              -Isrc                                       \
              -I$(RAYLIB_WIN_PATH)/include                \
              -mwindows
LDFLAGS_WIN = -L$(RAYLIB_WIN_PATH)/lib                   \
              -lraylib -lopengl32 -lgdi32                 \
              -lwinmm -lshell32 -lpthread -lm             \
              -static-libgcc -static-libstdc++

OUT_WIN     = $(BUILD_DIR)/rustbastion.exe

# ── Icône Windows ───────────────────────────────────────────────
ICON_RC  = assets/win_ico/icon.rc
ICON_OBJ = $(OBJ_DIR)/icon.o

# ════════════════════════════════════════════════════════════════
#  CIBLES PRINCIPALES
# ════════════════════════════════════════════════════════════════
.PHONY: all linux win package_win package_linux \
        zip_win zip_linux clean help

all: linux

# ── Linux ────────────────────────────────────────────────────────
linux: $(OUT_LINUX)

$(OUT_LINUX): $(SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC_LINUX) $(CFLAGS_LINUX) $(SRC) -o $(OUT_LINUX) $(LDFLAGS_LINUX)
	@echo "✓ Linux build : $(OUT_LINUX)"

# ── Windows — icône ──────────────────────────────────────────────
$(ICON_OBJ): $(ICON_RC)
	@mkdir -p $(OBJ_DIR)
	$(WINDRES) -i $(ICON_RC) -o $(ICON_OBJ)

# ── Windows — exécutable ─────────────────────────────────────────
win: $(OUT_WIN)

$(OUT_WIN): $(SRC) $(ICON_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CC_WIN) $(CFLAGS_WIN) $(SRC) $(ICON_OBJ) -o $(OUT_WIN) $(LDFLAGS_WIN)
	@echo "✓ Windows build : $(OUT_WIN)"

# ════════════════════════════════════════════════════════════════
#  PACKAGE LINUX — structure prête à distribuer
# ════════════════════════════════════════════════════════════════
package_linux: linux
	@echo "── Package Linux ──────────────────────────────"
	@mkdir -p $(LINUX_DIR)/assets/textures
	@mkdir -p $(LINUX_DIR)/assets/sounds
	@mkdir -p $(LINUX_DIR)/assets/fonts
	@mkdir -p $(LINUX_DIR)/saves
	@mkdir -p $(LINUX_DIR)/config

	@# Exécutable
	cp $(OUT_LINUX) $(LINUX_DIR)/rustbastion
	chmod +x $(LINUX_DIR)/rustbastion

	@# Assets
	@if [ -d "$(ASSETS_DIR)" ]; then \
		cp -r $(ASSETS_DIR)/textures $(LINUX_DIR)/assets/ 2>/dev/null || true; \
		cp -r $(ASSETS_DIR)/sounds   $(LINUX_DIR)/assets/ 2>/dev/null || true; \
		cp -r $(ASSETS_DIR)/fonts    $(LINUX_DIR)/assets/ 2>/dev/null || true; \
	fi

	@# Config par défaut
	@echo '{ "volume": 100, "fullscreen": false, "monitor": 0 }' \
		> $(LINUX_DIR)/config/settings.json

	@# Script de lancement
	@echo '#!/bin/bash'                              > $(LINUX_DIR)/launch.sh
	@echo 'cd "$$(dirname "$$0")"'                 >> $(LINUX_DIR)/launch.sh
	@echo './rustbastion'                           >> $(LINUX_DIR)/launch.sh
	@chmod +x $(LINUX_DIR)/launch.sh

	@# README
	@echo "RUST BASTION"                            > $(LINUX_DIR)/README.txt
	@echo "============"                           >> $(LINUX_DIR)/README.txt
	@echo ""                                       >> $(LINUX_DIR)/README.txt
	@echo "Lancement : ./rustbastion"              >> $(LINUX_DIR)/README.txt
	@echo "       ou : ./launch.sh"                >> $(LINUX_DIR)/README.txt
	@echo ""                                       >> $(LINUX_DIR)/README.txt
	@echo "Sauvegardes : saves/rustbastion.sav"    >> $(LINUX_DIR)/README.txt

	@echo "✓ Package Linux : $(LINUX_DIR)/"

# ════════════════════════════════════════════════════════════════
#  PACKAGE WINDOWS — structure prête à distribuer / Steam
# ════════════════════════════════════════════════════════════════
package_win: win
	@echo "── Package Windows ────────────────────────────"
	@mkdir -p $(WIN_DIR)/assets/textures
	@mkdir -p $(WIN_DIR)/assets/sounds
	@mkdir -p $(WIN_DIR)/assets/fonts
	@mkdir -p $(WIN_DIR)/saves
	@mkdir -p $(WIN_DIR)/config

	@# Exécutable
	cp $(OUT_WIN) $(WIN_DIR)/rustbastion.exe

	@# Assets
	@if [ -d "$(ASSETS_DIR)" ]; then \
		cp -r $(ASSETS_DIR)/textures $(WIN_DIR)/assets/ 2>/dev/null || true; \
		cp -r $(ASSETS_DIR)/sounds   $(WIN_DIR)/assets/ 2>/dev/null || true; \
		cp -r $(ASSETS_DIR)/fonts    $(WIN_DIR)/assets/ 2>/dev/null || true; \
	fi

	@# Config par défaut
	@echo '{ "volume": 100, "fullscreen": false, "monitor": 0 }' \
		> $(WIN_DIR)/config/settings.json

	@# README
	@echo "RUST BASTION"                                    > $(WIN_DIR)/README.txt
	@echo "============"                                   >> $(WIN_DIR)/README.txt
	@echo ""                                               >> $(WIN_DIR)/README.txt
	@echo "Double-cliquez sur rustbastion.exe pour jouer." >> $(WIN_DIR)/README.txt
	@echo ""                                               >> $(WIN_DIR)/README.txt
	@echo "Sauvegardes : saves\rustbastion.sav"            >> $(WIN_DIR)/README.txt

	@echo "✓ Package Windows : $(WIN_DIR)/"

# ════════════════════════════════════════════════════════════════
#  ZIP / ARCHIVE
# ════════════════════════════════════════════════════════════════
zip_win: package_win
	@mkdir -p $(RELEASE_DIR)
	cd $(RELEASE_DIR)/win && zip -r ../../$(ZIP_WIN) rustbastion
	@echo "✓ ZIP Windows : $(ZIP_WIN)"

zip_linux: package_linux
	@mkdir -p $(RELEASE_DIR)
	cd $(RELEASE_DIR)/linux && tar -czf ../../$(ZIP_LINUX) rustbastion
	@echo "✓ Archive Linux : $(ZIP_LINUX)"

# ── Les deux plateformes d'un coup ───────────────────────────────
release: zip_win zip_linux
	@echo ""
	@echo "══════════════════════════════════"
	@echo "  RELEASE COMPLÈTE"
	@echo "  Windows : $(ZIP_WIN)"
	@echo "  Linux   : $(ZIP_LINUX)"
	@echo "══════════════════════════════════"

# ════════════════════════════════════════════════════════════════
#  NETTOYAGE
# ════════════════════════════════════════════════════════════════
clean:
	@rm -rf $(BUILD_DIR)
	@rm -rf $(RELEASE_DIR)
	@echo "✓ Nettoyage terminé"

# ════════════════════════════════════════════════════════════════
#  AIDE
# ════════════════════════════════════════════════════════════════
help:
	@echo ""
	@echo "  make linux         — compile pour Linux"
	@echo "  make win           — compile pour Windows (cross-compile)"
	@echo "  make package_linux — prépare le dossier de distribution Linux"
	@echo "  make package_win   — prépare le dossier de distribution Windows"
	@echo "  make zip_linux     — package Linux + archive .tar.gz"
	@echo "  make zip_win       — package Windows + archive .zip"
	@echo "  make release       — les deux plateformes en une commande"
	@echo "  make clean         — supprime tous les fichiers générés"
	@echo ""
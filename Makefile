# ═══════════════════════════════════════════════════════════════
#  RUST BASTION — Makefile
#  Cibles : linux | win | package_win | package_linux | zip | clean
# ═══════════════════════════════════════════════════════════════

# ── Sources ─────────────────────────────────────────────────────
SRC = Src/main.c                                                              \
      Src/engine/audio.c       Src/engine/canvas.c    Src/engine/window.c   \
      Src/engine/assets.c      Src/engine/paths.c                            \
      Src/game/game_state.c    Src/game/game_init.c   Src/game/save.c       \
      Src/game/meta.c          Src/game/app.c         Src/game/runperks.c   \
      Src/game/app_tutorial.c  Src/game/app_mp.c                            \
      Src/game/campaign_data.c                                              \
      Src/map/map_gen.c        Src/map/pathfinding.c  Src/map/theme.c       \
      Src/combat/enemy.c       Src/combat/wave.c      Src/combat/tower.c    \
      Src/combat/unit.c        Src/combat/projectile.c Src/combat/material.c \
      Src/combat/fx.c                                                        \
      Src/ui/renderer.c        Src/ui/ui_utils.c      Src/ui/tile_art.c     \
      Src/ui/render3d.c        Src/ui/render3d_units.c Src/ui/render3d_enemies.c \
      Src/ui/perk_art.c                                                     \
      Src/ui/hud.c             Src/ui/hud_input.c     Src/ui/hud_render.c   \
      Src/ui/menu.c            Src/ui/menu_screens.c  Src/ui/menu_campaign.c \
      Src/ui/menu_anim.c                                                     \
      Src/ui/menu_options.c    Src/ui/menu_upgrades.c Src/ui/menu_bestiary.c \
      Src/ui/menu_custom_config.c                                             \
      Src/ui/menu_mp.c                                                       \
      Src/ui/interlude.c                                                     \
      Src/net/net_transport.c  Src/net/net_session.c   Src/net/net_upnp.c     \
      Src/net/net_relay.c                                                     \

# ── Dossiers ────────────────────────────────────────────────────
ASSETS_DIR   = assets
RELEASE_DIR  = release
WIN_DIR      = $(RELEASE_DIR)/win/rustbastion
LINUX_DIR    = $(RELEASE_DIR)/linux/rustbastion
BUILD_DIR    = build
OBJ_DIR      = $(BUILD_DIR)/obj

# ── ZIP ─────────────────────────────────────────────────────────
ZIP_WIN      = $(RELEASE_DIR)/rustbastion_win.tar.gz
ZIP_LINUX    = $(RELEASE_DIR)/rustbastion_linux.tar.gz

# ════════════════════════════════════════════════════════════════
#  LINUX
# ════════════════════════════════════════════════════════════════
CC_LINUX      = gcc
CFLAGS_LINUX  = -Wall -Wextra -O2 -Isrc -DGL_DISABLE_VSYNC
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
              -lws2_32                                    \
              -static-libgcc -static-libstdc++

OUT_WIN     = $(BUILD_DIR)/rustbastion.exe

# ── Icône Windows ───────────────────────────────────────────────
ICON_RC  = assets/win_ico/icon.rc
ICON_OBJ = $(OBJ_DIR)/icon.o

# ════════════════════════════════════════════════════════════════
#  CIBLES PRINCIPALES
# ════════════════════════════════════════════════════════════════
.PHONY: all linux win package package_win package_linux \
        zip zip_win zip_linux clean help

all: linux

# ── Les deux plateformes d'un coup ───────────────────────────────
package: package_win package_linux
	@echo "✓ Packages créés pour Windows et Linux"

zip: zip_win zip_linux
	@echo "✓ Archives ZIP créées pour Windows et Linux"

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
		mkdir -p $(LINUX_DIR)/assets/3d/3D_Tours $(LINUX_DIR)/assets/3d/3D_Troupes $(LINUX_DIR)/assets/3d/3D_enemies; \
		cp $(ASSETS_DIR)/3d/*.glb              $(LINUX_DIR)/assets/3d/           2>/dev/null || true; \
		cp $(ASSETS_DIR)/3d/3D_Tours/*.glb     $(LINUX_DIR)/assets/3d/3D_Tours/   2>/dev/null || true; \
		cp $(ASSETS_DIR)/3d/3D_Troupes/*.glb   $(LINUX_DIR)/assets/3d/3D_Troupes/ 2>/dev/null || true; \
		cp $(ASSETS_DIR)/3d/3D_enemies/*.glb   $(LINUX_DIR)/assets/3d/3D_enemies/ 2>/dev/null || true; \
	fi

	@# Script de lancement (gère audio WSLg + répertoire de travail)
	@echo '#!/bin/bash'                                                        > $(LINUX_DIR)/launch.sh
	@echo 'cd "$$(dirname "$$0")"'                                            >> $(LINUX_DIR)/launch.sh
	@echo '# Audio WSLg (Windows Subsystem for Linux)'                        >> $(LINUX_DIR)/launch.sh
	@echo 'if [ -S /mnt/wslg/runtime-dir/pulse/native ]; then'               >> $(LINUX_DIR)/launch.sh
	@echo '  export PULSE_SERVER=unix:/mnt/wslg/runtime-dir/pulse/native'     >> $(LINUX_DIR)/launch.sh
	@echo 'fi'                                                                >> $(LINUX_DIR)/launch.sh
	@echo 'exec ./rustbastion "$$@"'                                          >> $(LINUX_DIR)/launch.sh
	@chmod +x $(LINUX_DIR)/launch.sh

	@# README
	@echo "RUST BASTION"                                     > $(LINUX_DIR)/README.txt
	@echo "============"                                    >> $(LINUX_DIR)/README.txt
	@echo ""                                                >> $(LINUX_DIR)/README.txt
	@echo "Lancement : ./launch.sh  (recommande)"          >> $(LINUX_DIR)/README.txt
	@echo "       ou : ./rustbastion"                       >> $(LINUX_DIR)/README.txt
	@echo ""                                                >> $(LINUX_DIR)/README.txt
	@echo "Sauvegardes  : saves/rustbastion_slot*.sav"      >> $(LINUX_DIR)/README.txt
	@echo "Meta-progres : saves/rustbastion_meta.sav"       >> $(LINUX_DIR)/README.txt
	@echo "Parametres   : config/settings.bin"              >> $(LINUX_DIR)/README.txt

	@echo "✓ Package Linux : $(LINUX_DIR)/"

# ════════════════════════════════════════════════════════════════
#  PACKAGE WINDOWS — structure prête à distribuer / Steam
# ════════════════════════════════════════════════════════════════
package_win: win
	@echo "── Package Windows ────────────────────────────"
	@mkdir -p $(WIN_DIR)/assets/textures/Menue
	@mkdir -p $(WIN_DIR)/assets/sounds
	@mkdir -p $(WIN_DIR)/assets/fonts
	@mkdir -p $(WIN_DIR)/assets/win_ico
	@mkdir -p $(WIN_DIR)/saves
	@mkdir -p $(WIN_DIR)/config

	@# Exécutable
	cp $(OUT_WIN) $(WIN_DIR)/rustbastion.exe

	@# Assets complets
	@if [ -d "$(ASSETS_DIR)" ]; then \
		cp -r $(ASSETS_DIR)/textures $(WIN_DIR)/assets/ 2>/dev/null || true; \
		cp -r $(ASSETS_DIR)/sounds   $(WIN_DIR)/assets/ 2>/dev/null || true; \
		cp -r $(ASSETS_DIR)/fonts    $(WIN_DIR)/assets/ 2>/dev/null || true; \
		mkdir -p $(WIN_DIR)/assets/3d/3D_Tours $(WIN_DIR)/assets/3d/3D_Troupes $(WIN_DIR)/assets/3d/3D_enemies; \
		cp $(ASSETS_DIR)/3d/*.glb              $(WIN_DIR)/assets/3d/           2>/dev/null || true; \
		cp $(ASSETS_DIR)/3d/3D_Tours/*.glb     $(WIN_DIR)/assets/3d/3D_Tours/   2>/dev/null || true; \
		cp $(ASSETS_DIR)/3d/3D_Troupes/*.glb   $(WIN_DIR)/assets/3d/3D_Troupes/ 2>/dev/null || true; \
		cp $(ASSETS_DIR)/3d/3D_enemies/*.glb   $(WIN_DIR)/assets/3d/3D_enemies/ 2>/dev/null || true; \
		cp -r $(ASSETS_DIR)/win_ico  $(WIN_DIR)/assets/ 2>/dev/null || true; \
		cp $(ASSETS_DIR)/icon.png    $(WIN_DIR)/assets/ 2>/dev/null || true; \
	fi

	@# README
	@echo "RUST BASTION"                                         > $(WIN_DIR)/README.txt
	@echo "============"                                        >> $(WIN_DIR)/README.txt
	@echo ""                                                    >> $(WIN_DIR)/README.txt
	@echo "Double-cliquez sur rustbastion.exe pour jouer."      >> $(WIN_DIR)/README.txt
	@echo ""                                                    >> $(WIN_DIR)/README.txt
	@echo "Sauvegardes  : saves\rustbastion_slot*.sav"          >> $(WIN_DIR)/README.txt
	@echo "Meta-progres : saves\rustbastion_meta.sav"           >> $(WIN_DIR)/README.txt
	@echo "Parametres   : config\settings.bin"                  >> $(WIN_DIR)/README.txt

	@echo "✓ Package Windows : $(WIN_DIR)/"

# ════════════════════════════════════════════════════════════════
#  ZIP / ARCHIVE
# ════════════════════════════════════════════════════════════════
zip_win: package_win
	@mkdir -p $(RELEASE_DIR)
	cd $(RELEASE_DIR)/win && tar -czf ../../$(ZIP_WIN) rustbastion
	@echo "✓ Archive Windows : $(ZIP_WIN)"

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
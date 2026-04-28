CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt \
          -lX11 -lXi -lXcursor -lXrandr -lXinerama
SRC = main.c game_state.c map_gen.c pathfinding.c \
      renderer.c theme.c enemy.c wave.c tower.c unit.c meta.c ui.c save.c menu.c
OUT = rustbastion

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(OUT)
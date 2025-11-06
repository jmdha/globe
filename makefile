all:
	gcc -Ithird_party -std=c17 -O3 -ggdb main.c io.c -lm -lX11 -lXrandr

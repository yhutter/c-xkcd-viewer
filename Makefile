build:
	clang -Wall -Wextra -std=c99 -O3 `pkg-config sdl3 sdl3-ttf --cflags --libs` src/*.c -o xkcd_viewer
run:
	./xkcd_viewer

build:
	clang -Wall -Wextra -std=c99 `pkg-config sdl3 --cflags --libs` src/*.c -o xkcd_viewer
run:
	./xkcd_viewer

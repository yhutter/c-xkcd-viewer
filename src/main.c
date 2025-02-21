#include <SDL3/SDL.h>
#include <stdbool.h>

#define FPS 30
#define FRAME_TARGET_TIME (1000.0f / FPS)
SDL_Renderer* renderer = NULL;
SDL_Window* window = NULL;
int window_width = 1280;
int window_height = 720;
bool running = false;
Uint64 start_time;
Uint64 current_time;


bool initialize() {
    if (!SDL_CreateWindowAndRenderer("XKCD Viewer", window_width, window_height, 0, &window, &renderer)) {
        SDL_Log("Could not create window and renderer: '%s'\n", SDL_GetError());
        return false;
    }

    // Enable VSync
    SDL_SetRenderVSync(renderer, 1);


    start_time = SDL_GetTicks();
    return true;
}

void process(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.key == SDLK_ESCAPE) {
                    running = false;
                    break;
                }
        }
    }
}

void update(void) {
    current_time = SDL_GetTicks();
    Uint64 delta_time = current_time - start_time;
    start_time = current_time;

    Uint64 time_to_wait = FRAME_TARGET_TIME - delta_time;
    if (time_to_wait > 0 && time_to_wait <= FRAME_TARGET_TIME) {
        SDL_Delay(time_to_wait);
    }
}

void render(void) {
    SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xff);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

void destroy(void) {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(void) {
    running = initialize();

    while (running) {
        process();
        update();
        render();
    }
    destroy();
    return 0;
}

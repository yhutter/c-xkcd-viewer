#include <SDL3/SDL.h>
#include <stdbool.h>
#include <math.h>

#define FPS 60
#define FRAME_TARGET_TIME (1000.0f / FPS)
#define FRAME_TARGET_TIME_SECONDS (FRAME_TARGET_TIME * 0.001f)

#define INPUT_OVERLAY_ANIMATION_DURATION 0.6f

typedef enum {
    ease_out_expo
} animation_kind;

typedef struct {
    float start;
    float now;
    float value;
    float end;
    float duration;
    float progress;
    bool done;
    bool reverse;
    animation_kind kind;
} animation_t;


typedef struct {
    animation_t open_animation;
    animation_t close_animation;
    animation_t current_animation;
    bool open;
} input_overlay_t;

animation_t create_animation(float duration, animation_kind kind, bool reverse) {
    // Convert to seconds
    float start = SDL_GetTicks() * 0.001f;
    animation_t result = {
        .start = start, 
        .now = 0.0f,
        .duration = duration,
        .end = start + duration,
        .progress = 0.0f,
        .value = 0.0f,
        .done = false,
        .reverse = reverse,
        .kind = kind
    };
    return result;
}

input_overlay_t create_input_overlay(void) {
    animation_t open_animation = create_animation(INPUT_OVERLAY_ANIMATION_DURATION, ease_out_expo, false);
    animation_t close_animation = create_animation(INPUT_OVERLAY_ANIMATION_DURATION, ease_out_expo, true);
    input_overlay_t result = {
        .open_animation = open_animation,
        .close_animation = close_animation,
        .current_animation = open_animation,
        .open = true
    };
    return result;
}

SDL_Renderer* renderer = NULL;
SDL_Window* window = NULL;
int window_width = 1280;
int window_height = 720;
bool running = false;

Uint64 start_time;
float seconds_passed = 0.0f;
input_overlay_t input_overlay;

static inline float lerp(float min, float max, float t) {
    float range = max - min;
    return min + (t * range);
}

static inline float remap(float value, float in_min, float in_max, float out_min, float out_max) {
    float in_range = in_max - in_min;
    float out_range = out_max - out_min;
    float percentage = (value - in_min) / in_range;
    return out_min + (percentage * out_range);
}

static inline float max(float first, float second) {
    return first > second ? first : second;
}

void update_animation(animation_t* animation) {
    animation->now += FRAME_TARGET_TIME_SECONDS;
    // Check if animation is done
    if (animation->progress > 1.0f) {
        animation->done = true;
    }
    // Keep updating the animation
    else {
        animation->progress = animation->now / animation->end;
        float t = lerp(0.0f, 1.0f, animation->progress);
        switch (animation->kind) {
            case ease_out_expo:
                animation->value = 1.0f - pow(2.0f, (-10.0f * t));
                break;
        }
        if (animation->reverse) {
            animation->value = remap(animation->value, 0.0f, 1.0f, 1.0f, 0.0f);
        }
    }
}


bool initialize() {
    if (!SDL_CreateWindowAndRenderer("XKCD Viewer", window_width, window_height, 0, &window, &renderer)) {
        SDL_Log("Could not create window and renderer: '%s'\n", SDL_GetError());
        return false;
    }

    // Enable VSync
    SDL_SetRenderVSync(renderer, 1);
    start_time = SDL_GetTicks();
    input_overlay = create_input_overlay();
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
                    // Toggle overlay
                    input_overlay.open = !input_overlay.open;
                    // Swap animations
                    if (input_overlay.open) {
                        input_overlay.current_animation = input_overlay.open_animation;
                    }
                    else {
                        input_overlay.current_animation = input_overlay.close_animation;
                    }
                    break;
                }
        }
    }
}

void update(void) {
    int time_to_wait = FRAME_TARGET_TIME - (SDL_GetTicks() - start_time);
    if (time_to_wait > 0 && time_to_wait <= FRAME_TARGET_TIME) {
        SDL_Delay(time_to_wait);
    }
    seconds_passed += FRAME_TARGET_TIME_SECONDS;
    update_animation(&input_overlay.current_animation);
    start_time = SDL_GetTicks();
}

void render_input_overlay(void) {
    SDL_SetRenderDrawColor(renderer, 0xff, 0x18, 0x18, 0xff);
    SDL_FRect input_overlay_rect = {
        .x = 0,
        .y = 0,
        .w = window_width, 
        .h = input_overlay.current_animation.value * (window_height * 0.3f), 
    };
    SDL_RenderFillRect(renderer, &input_overlay_rect);
}

void render(void) {
    SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xff);
    SDL_RenderClear(renderer);
    render_input_overlay();
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

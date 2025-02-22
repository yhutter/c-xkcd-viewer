#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define FPS 144
#define FRAME_TARGET_TIME (1000.0f / FPS)
#define FRAME_TARGET_TIME_SECONDS (FRAME_TARGET_TIME * 0.001f)

#define ANIMATION_DURATION 0.6f
#define MAX_NUM_XKCD 1024

typedef enum {
    ease_out_expo,
    ease_in_sine
} animation_kind;

typedef struct {
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
    animation_t animation;
    bool destroy;
    int index;
    SDL_FRect rect;
    float size_x;
    float size_y;
} xkcd_t;

SDL_Renderer* renderer = NULL;
SDL_Window* window = NULL;
int window_width = 1280;
int window_height = 720;
bool running = false;

float mouse_x = 0.0f;
float mouse_y = 0.0f;
bool mouse_down = false;

// Mouse position at the point of time the user has pressed the mouse button
float mouse_down_x = 0.0f; 
float mouse_down_y = 0.0f;

Uint64 start_time;
float seconds_passed = 0.0f;

xkcd_t xkcds[MAX_NUM_XKCD];
int num_xkcds = 0;

static inline float remap(float value, float in_min, float in_max, float out_min, float out_max) {
    float in_range = in_max - in_min;
    float out_range = out_max - out_min;
    float percentage = (value - in_min) / in_range;
    return out_min + (percentage * out_range);
}

static inline bool inside_rect(float x, float y, float x0, float y0, float w, float h) {
    if (x >= x0 && x <= (x0 + w) && y >= y0 && y <= (y0 + h)) {
        return true;
    }
    return false;
}

animation_t create_animation(float duration, animation_kind kind, bool reverse) {
    animation_t result = {
        .now = 0.0f,
        .duration = duration,
        .progress = 0.0f,
        .value = 0.0f,
        .done = false,
        .reverse = reverse,
        .kind = kind
    };
    return result;
}

xkcd_t create_xkcd(int index, float x, float y, float size_x, float size_y) {
    animation_t animation = create_animation(ANIMATION_DURATION, ease_out_expo, false);
    xkcd_t result = {
        .animation = animation,
        .destroy = false,
        .rect = {
            .x = x,
            .y = y,
            .w = 0,
            .h = 0, 
        },
        .size_x = size_x,
        .size_y = size_y,
        .index = index 
    };
    return result;
}

int xkcd_index_at_mouse(void) {
    for (int i = 0; i < num_xkcds; i++) {
        xkcd_t xkcd = xkcds[i];
        if (inside_rect(mouse_x, mouse_y, xkcd.rect.x, xkcd.rect.y, xkcd.rect.w, xkcd.rect.h)) {
            return xkcd.index;
        }
    }
    return -1;
}

void update_animation(animation_t* animation) {
    if (animation->done) {
        return;
    }
    animation->now += FRAME_TARGET_TIME_SECONDS;
    if (animation->progress > 1.0f) {
        animation->done = true;
        return;
    }
    // Keep updating the animation
    else {
        animation->progress = animation->now / animation->duration;
        switch (animation->kind) {
            case ease_out_expo:
                animation->value = 1.0f - pow(2.0f, (-10.0f * animation->progress));
                break;
            case ease_in_sine:
                animation->value = 1.0f - cosf((animation->progress * M_PI) / 2.0f);
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

    return true;
}

void process(void) {
    SDL_GetMouseState(&mouse_x, &mouse_y);
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                SDL_GetMouseState(&mouse_down_x, &mouse_down_y);
                mouse_down = true;
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                mouse_down = false;
                // Create new xkc
                if (num_xkcds < MAX_NUM_XKCD) {
                    float size_x = fabs(mouse_x - mouse_down_x);
                    float size_y = fabs(mouse_y - mouse_down_y);
                    float x = mouse_x - (size_x * 0.5f);
                    float y = mouse_y - (size_y * 0.5f);
                    xkcds[num_xkcds] = create_xkcd(num_xkcds, x, y, size_x, size_y);
                    num_xkcds++;
                }
                break;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.key == SDLK_D) {
                    // Delete all xkcds
                    animation_t animation = create_animation(ANIMATION_DURATION, ease_in_sine, true);
                    for (int i = 0; i < num_xkcds; i++) {
                        xkcds[i].animation = animation;
                        xkcds[i].destroy = true;
                    }
                    break;
                }
                if (e.key.key == SDLK_ESCAPE) {
                    running = false;
                    break;
                }
        }
    }
}

void update_xkcd(xkcd_t* xkcd) {
    update_animation(&xkcd->animation);
    float animation_value = xkcd->animation.value;
    xkcd->rect.w = animation_value * xkcd->size_x;
    xkcd->rect.h = animation_value * xkcd->size_y;
}

void update(void) {
    int time_to_wait = FRAME_TARGET_TIME - (SDL_GetTicks() - start_time);
    if (time_to_wait > 0 && time_to_wait <= FRAME_TARGET_TIME) {
        SDL_Delay(time_to_wait);
    }
    seconds_passed += FRAME_TARGET_TIME_SECONDS;
    for (int i = 0; i < num_xkcds; i++) {
        update_xkcd(&xkcds[i]);
    }
    start_time = SDL_GetTicks();
}

void render_xkcd(xkcd_t* xkcd) {
    SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xff);
    bool animation_done = xkcd->animation.done;
    bool draw_border = !xkcd->destroy || (xkcd->destroy && !animation_done);
    SDL_RenderFillRect(renderer, &xkcd->rect);
 
    // Draw border
    if (draw_border) {
        // Are we hovering over it?
        if (inside_rect(mouse_x, mouse_y, xkcd->rect.x, xkcd->rect.y, xkcd->rect.w, xkcd->rect.h)) {
            SDL_SetRenderDrawColor(renderer, 0x9e, 0x95, 0xc7, 0xff);
        }
        else {
            SDL_SetRenderDrawColor(renderer, 0xff, 0xdd, 0x33, 0xff);
        }
        SDL_RenderRect(renderer, &xkcd->rect);
    }
}

void render(void) {
    SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xff);
    SDL_RenderClear(renderer);
    for (int i = 0; i < num_xkcds; i++) {
        render_xkcd(&xkcds[i]);
    }
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

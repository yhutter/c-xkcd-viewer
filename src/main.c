#include <SDL3/SDL.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <curl/curl.h>

#define FPS 60
#define FRAME_TARGET_TIME (1000.0f / FPS)
#define FRAME_TARGET_TIME_SECONDS (FRAME_TARGET_TIME * 0.001f)

#define ANIMATION_DURATION 0.6f
#define MAX_NUM_XKCD 1024

#define FONT_PATH "./font/Alegreya-Regular.ttf"

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
    bool destroy; // Mark for destruction (however the animation might still be ongoing)
    bool destroyed; // Mark for destruction and animation has finished
    SDL_FRect rect;
    float size_x;
    float size_y;
    float font_size;
    int index;
    bool loading;
} xkcd_t;

typedef struct {
    int index;
    int xkcd_number;
} xkcd_request_t;

SDL_Renderer* renderer = NULL;
SDL_Window* window = NULL;
int window_width = 1280;
int window_height = 720;
bool running = false;

TTF_TextEngine* text_engine = NULL;

float mouse_x = 0.0f;
float mouse_y = 0.0f;
bool mouse_down = false;

// Mouse position at the point of time the user has pressed the mouse button
float mouse_down_x = 0.0f; 
float mouse_down_y = 0.0f;

Uint64 start_time;
float seconds_passed = 0.0f;

xkcd_t xkcds[MAX_NUM_XKCD];
xkcd_request_t xkcd_requests[MAX_NUM_XKCD];
int num_xkcds = 0;
SDL_FRect xkcd_indication_rect = {0};

static inline float remap(float value, float in_min, float in_max, float out_min, float out_max) {
    float in_range = in_max - in_min;
    float out_range = out_max - out_min;
    float percentage = (value - in_min) / in_range;
    return out_min + (percentage * out_range);
}

static inline bool inside_rect(float x, float y, SDL_FRect rect) {
    if (x >= rect.x && x <= (rect.x + rect.w) && y >= rect.y && y <= (rect.y + rect.h)) {
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


int make_xkcd_request(void* data) {
    CURL* curl;
    CURLcode res;
    curl = curl_easy_init();
    xkcd_request_t* request  = (xkcd_request_t*) data;
    char* request_url = NULL;
    SDL_asprintf(&request_url, "https://xkcd.com/%d/info.0.json", request->xkcd_number);
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, request_url);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            SDL_Log("ERROR in performing curl request for url %s", request_url);
        }
        curl_easy_cleanup(curl);
    }
    xkcds[request->index].loading = false;
    SDL_free(request_url);
    return 0;
}

xkcd_t create_xkcd(int index, float x, float y, float size_x, float size_y) {
    animation_t animation = create_animation(ANIMATION_DURATION, ease_out_expo, false);

    xkcd_t result = {
        .index = index,
        .loading = true,
        .animation = animation,
        .destroy = false,
        .destroyed = false,
        .rect = {
            .x = x,
            .y = y,
            .w = 0,
            .h = 0, 
        },
        .size_x = size_x,
        .size_y = size_y,
        .font_size = 0.0f
    };

    xkcd_requests[index] = (xkcd_request_t) {
        .index = index,
        .xkcd_number = 6
    };
    SDL_Log("Creating xkcd with index %d", index);
    SDL_Thread* thread = SDL_CreateThread(make_xkcd_request, "xkcd_request_thread", (void*) &xkcd_requests[index]);
    if (thread == NULL) {
        SDL_Log("Failed to create thread");
    }
    else {
        // Run in background, will automatically be cleaned up once done
        SDL_DetachThread(thread);
    }
    return result;
}

xkcd_t* xkcd_at_mouse(void) {
    for (int i = num_xkcds; i >= 0; i--) {
        xkcd_t* xkcd = &xkcds[i];
        if (inside_rect(mouse_x, mouse_y, xkcd->rect)) {
            return xkcd;
        }
    }
    return NULL;
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

SDL_FRect rect_from_mouse(void) {
    float w = mouse_x - mouse_down_x;
    float h = mouse_y - mouse_down_y;
    float x = mouse_down_x;
    float y = mouse_down_y;
    if (w < 0) {
        w = fabs(w);
        x = x - w;
    }
    if (h < 0) {
        h = fabs(h);
        y = y - h;
    }
    SDL_FRect result = {
        .x = x,
        .y = y,
        .w = w,
        .h = h 
    };
    return result;
}

bool rect_intersects(SDL_FRect a, SDL_FRect b) {
    // Check if any of the 4 corners intersect
    bool top_left = inside_rect(b.x, b.y, a);
    bool bottom_left = inside_rect(b.x, b.y + b.h, a);
    bool top_right = inside_rect(b.x + b.w, b.y, a);
    bool bottom_right = inside_rect(b.x + b.w, b.y + b.h, a);
    return top_left || bottom_left || top_right || bottom_right;
}


bool initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Could not initialize SDL: '%s'\n", SDL_GetError());
        return false;
    }
    if (!SDL_CreateWindowAndRenderer("XKCD Viewer", window_width, window_height, 0, &window, &renderer)) {
        SDL_Log("Could not create window and renderer: '%s'\n", SDL_GetError());
        return false;
    }

    if (!TTF_Init()) {
        SDL_Log("Could not initialize SDL TTF: '%s'\n", SDL_GetError());
        return false;
    }

    // Enable VSync
    SDL_SetRenderVSync(renderer, 1);

    text_engine = TTF_CreateRendererTextEngine(renderer);
    if (text_engine == NULL) {
        SDL_Log("Could not create text engine: '%s'\n", SDL_GetError());
        return false;
    }


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
                    SDL_FRect rect = rect_from_mouse();
                    xkcds[num_xkcds] = create_xkcd(num_xkcds, rect.x, rect.y, rect.w, rect.h);
                    num_xkcds++;
                }
                break;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.key == SDLK_D) {
                    // Mark xkcd for deletion and assign new "delete" animation
                    xkcd_t* xkcd_to_delete = xkcd_at_mouse();
                    if (xkcd_to_delete == NULL) {
                        return;
                    }
                    animation_t animation = create_animation(ANIMATION_DURATION, ease_in_sine, true);
                    xkcd_to_delete->animation = animation;
                    xkcd_to_delete->destroy = true;
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
    if (xkcd->destroyed) {
        return;
    }
    update_animation(&xkcd->animation);
    xkcd->destroyed = xkcd->destroy && xkcd->animation.done;
    float animation_value = xkcd->animation.value;
    xkcd->rect.w = animation_value * xkcd->size_x;
    xkcd->rect.h = animation_value * xkcd->size_y;
    xkcd->font_size = ceilf(xkcd->rect.h * 0.2);
}


void update(void) {
    int time_to_wait = FRAME_TARGET_TIME - (SDL_GetTicks() - start_time);
    if (time_to_wait > 0 && time_to_wait <= FRAME_TARGET_TIME) {
        SDL_Delay(time_to_wait);
    }
    seconds_passed += FRAME_TARGET_TIME_SECONDS;
    xkcd_indication_rect = rect_from_mouse();
    for (int i = 0; i < num_xkcds; i++) {
        update_xkcd(&xkcds[i]);
    }
    start_time = SDL_GetTicks();
}

void render_xkcd(xkcd_t* xkcd) {
    if (xkcd->destroyed) {
        return;
    }
    const char* message = xkcd->loading ? "Loading" : "Done!";
    TTF_Font* font = TTF_OpenFont(FONT_PATH, xkcd->font_size);
    TTF_Text* text = TTF_CreateText(text_engine, font, message, 0);
    SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xff);
    bool animation_done = xkcd->animation.done;
    bool draw_border = !xkcd->destroy || (xkcd->destroy && !animation_done);
    SDL_RenderFillRect(renderer, &xkcd->rect);
    bool is_hovering = inside_rect(mouse_x, mouse_y, xkcd->rect);
    
    // Render text
    SDL_SetRenderDrawColor(renderer, 0xe4, 0xe4, 0xef, 0xff);
    int text_w;
    int text_h;
    TTF_GetTextSize(text, &text_w, &text_h);
    TTF_DrawRendererText(text, xkcd->rect.x + (xkcd->rect.w - text_w) * 0.5, xkcd->rect.y + (xkcd->rect.h - text_h) * 0.5f);
 
    // Draw border
    if (draw_border) {
        if (is_hovering) {
            SDL_SetRenderDrawColor(renderer, 0x9e, 0x95, 0xc7, 0xff);
        }
        else if (xkcd->loading) {
            SDL_SetRenderDrawColor(renderer, 0xf4, 0x38, 0x41, 0xff);
        }
        else {
            SDL_SetRenderDrawColor(renderer, 0xff, 0xdd, 0x33, 0xff);
        }
        SDL_RenderRect(renderer, &xkcd->rect);
    }

    TTF_DestroyText(text);
    TTF_CloseFont(font);
}

void render(void) {
    SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xff);
    SDL_RenderClear(renderer);

    // Render indication
    if (mouse_down) {
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
        SDL_RenderRect(renderer, &xkcd_indication_rect);
    }

    for (int i = 0; i < num_xkcds; i++) {
        render_xkcd(&xkcds[i]);
    }
    SDL_RenderPresent(renderer);
}

void destroy(void) {
    TTF_DestroyRendererTextEngine(text_engine);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
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

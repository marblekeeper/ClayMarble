#include "input_handler.h"
#include <string.h>

#define MAX_KEYS 512
static int g_keyStates[MAX_KEYS] = {0};

void Input_Init(void) {
    memset(g_keyStates, 0, sizeof(g_keyStates));
}

void Input_ProcessEvent(SDL_Event* event) {
    if (event->type == SDL_KEYDOWN) {
        int sc = event->key.keysym.scancode;
        if (sc >= 0 && sc < MAX_KEYS) g_keyStates[sc] = 1;
    } else if (event->type == SDL_KEYUP) {
        int sc = event->key.keysym.scancode;
        if (sc >= 0 && sc < MAX_KEYS) g_keyStates[sc] = 0;
    }
}

static SDL_Scancode NameToScancode(const char* name) {
    if (strcmp(name, "up") == 0)     return SDL_SCANCODE_UP;
    if (strcmp(name, "down") == 0)   return SDL_SCANCODE_DOWN;
    if (strcmp(name, "left") == 0)   return SDL_SCANCODE_LEFT;
    if (strcmp(name, "right") == 0)  return SDL_SCANCODE_RIGHT;
    if (strcmp(name, "space") == 0)  return SDL_SCANCODE_SPACE;
    if (strcmp(name, "w") == 0)      return SDL_SCANCODE_W;
    if (strcmp(name, "a") == 0)      return SDL_SCANCODE_A;
    if (strcmp(name, "s") == 0)      return SDL_SCANCODE_S;
    if (strcmp(name, "d") == 0)      return SDL_SCANCODE_D;
    if (strcmp(name, "escape") == 0) return SDL_SCANCODE_ESCAPE;
    if (strcmp(name, "enter") == 0)  return SDL_SCANCODE_RETURN;
    if (strcmp(name, "lshift") == 0) return SDL_SCANCODE_LSHIFT;
    if (strcmp(name, "tab") == 0)    return SDL_SCANCODE_TAB;
    return SDL_SCANCODE_UNKNOWN;
}

int Input_GetKeyState(const char* keyName) {
    SDL_Scancode sc = NameToScancode(keyName);
    if (sc == SDL_SCANCODE_UNKNOWN || sc >= MAX_KEYS) return 0;
    return g_keyStates[sc];
}
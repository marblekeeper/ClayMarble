/* test_ui.c - EMSCRIPTEN COMPATIBLE VERSION */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#ifndef __EMSCRIPTEN__
#include <SDL2/SDL_syswm.h>
#endif
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Bridge Engine Imports --- */
extern int InitEngine(void* windowHandle, int width, int height);
extern void ShutdownEngine();
extern void ClearScreen(float r, float g, float b, float a);
extern void RenderUI(void* vertices, int vertexCount);
extern void BridgeSwapBuffers();
extern int CreateWhiteTexture();
extern int CreateTextureFromData(unsigned char* data, int w, int h);
extern int LoadTexture(const char* path, int* outW, int* outH);
extern void BindTexture(int id);
extern void SetProjectionMatrix(float* matrix);
extern void UpdateViewport(int width, int height);

/* --- Data Structures --- */
typedef struct { float x, y; float u, v; unsigned int color; } UIVertex;

typedef struct {
    float u0, v0, u1, v1;
    float width, height;
    float advance;
    float xoff, yoff;
} Glyph;

typedef struct {
    int textureId;
    int texWidth;
    int texHeight;
    Glyph glyphs[256];
    int loaded;
} Font;

/* Global State */
#define MAX_UI_VERTS 10000
UIVertex g_uiVerts[MAX_UI_VERTS];
int g_uiVertCount = 0;

int g_whiteTexId = 0;
int g_currentTexId = 0;
Font g_activeFont = {0};

/* Emscripten main loop state */
#ifdef __EMSCRIPTEN__
typedef struct {
    SDL_Window* window;
    lua_State* L;
    int winW;
    int winH;
    int running;
} EmscriptenLoopData;

EmscriptenLoopData g_loopData = {0};
#endif

/* --- Embedded Fallback Font Data --- */
const unsigned char FONT_DATA[] = {
    0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x5F,0x00,0x00, 0x00,0x07,0x00,0x07,0x00, 0x14,0x7F,0x14,0x7F,0x14,
    0x24,0x2A,0x7F,0x2A,0x12, 0x23,0x13,0x08,0x64,0x62, 0x36,0x49,0x55,0x22,0x50, 0x00,0x05,0x03,0x00,0x00,
    0x00,0x1C,0x22,0x41,0x00, 0x00,0x41,0x22,0x1C,0x00, 0x14,0x08,0x3E,0x08,0x14, 0x08,0x08,0x3E,0x08,0x08,
    0x00,0x50,0x30,0x00,0x00, 0x08,0x08,0x08,0x08,0x08, 0x00,0x60,0x60,0x00,0x00, 0x20,0x10,0x08,0x04,0x02,
    0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00, 0x42,0x61,0x51,0x49,0x46, 0x21,0x41,0x45,0x4B,0x31,
    0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39, 0x3C,0x4A,0x49,0x49,0x30, 0x01,0x71,0x09,0x05,0x03,
    0x36,0x49,0x49,0x49,0x36, 0x06,0x49,0x49,0x29,0x1E, 0x00,0x36,0x36,0x00,0x00, 0x00,0x56,0x36,0x00,0x00,
    0x08,0x14,0x22,0x41,0x00, 0x14,0x14,0x14,0x14,0x14, 0x00,0x41,0x22,0x14,0x08, 0x02,0x01,0x51,0x09,0x06,
    0x32,0x49,0x79,0x41,0x3E, 0x7E,0x11,0x11,0x11,0x7E, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
    0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01, 0x3E,0x41,0x49,0x49,0x7A,
    0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00, 0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41,
    0x7F,0x40,0x40,0x40,0x40, 0x7F,0x02,0x0C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
    0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46, 0x46,0x49,0x49,0x49,0x31,
    0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F, 0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F,
    0x63,0x14,0x08,0x14,0x63, 0x07,0x08,0x70,0x08,0x07, 0x61,0x51,0x49,0x45,0x43, 0x00,0x7F,0x41,0x41,0x00,
    0x02,0x04,0x08,0x10,0x20, 0x00,0x41,0x41,0x7F,0x00, 0x04,0x02,0x01,0x02,0x04, 0x40,0x40,0x40,0x40,0x40,
    0x00,0x01,0x02,0x04,0x00, 0x20,0x54,0x54,0x54,0x78, 0x7F,0x48,0x44,0x44,0x38, 0x38,0x44,0x44,0x44,0x20,
    0x38,0x44,0x44,0x48,0x7F, 0x38,0x54,0x54,0x54,0x18, 0x08,0x7E,0x09,0x01,0x02, 0x0C,0x52,0x52,0x52,0x3E,
    0x7F,0x08,0x04,0x04,0x78, 0x00,0x44,0x7D,0x40,0x00, 0x20,0x40,0x44,0x3D,0x00, 0x7F,0x10,0x28,0x44,0x00,
    0x00,0x41,0x7F,0x40,0x00, 0x7C,0x04,0x18,0x04,0x78, 0x7C,0x08,0x04,0x04,0x78, 0x38,0x44,0x44,0x44,0x38,
    0x7C,0x14,0x14,0x14,0x08, 0x08,0x14,0x14,0x18,0x7C, 0x7C,0x08,0x04,0x04,0x08, 0x48,0x54,0x54,0x54,0x20,
    0x04,0x3F,0x44,0x40,0x20, 0x3C,0x40,0x40,0x20,0x7C, 0x1C,0x20,0x40,0x20,0x1C, 0x3C,0x40,0x30,0x40,0x3C,
    0x44,0x28,0x10,0x28,0x44, 0x0C,0x50,0x50,0x50,0x3C, 0x44,0x64,0x54,0x4C,0x44, 0x00,0x08,0x36,0x41,0x00,
    0x00,0x00,0x7F,0x00,0x00, 0x00,0x41,0x36,0x08,0x00, 0x10,0x08,0x08,0x10,0x08, 0x7F,0x7F,0x7F,0x7F,0x7F
};

void FlushBatch() {
    if (g_uiVertCount > 0) {
        BindTexture(g_currentTexId);
        RenderUI(g_uiVerts, g_uiVertCount);
        g_uiVertCount = 0;
    }
}

void SetBatchTexture(int id) {
    if (g_currentTexId != id) {
        FlushBatch();
        g_currentTexId = id;
    }
}

void GenerateDebugFont() {
    printf("[System] Generating Procedural Debug Font...\n");
    int charW = 8, charH = 8, cols = 16, rows = 8;
    int texW = cols * charW, texH = rows * charH;
    int size = texW * texH * 4;
    unsigned char* data = (unsigned char*)malloc(size);
    memset(data, 0, size);
    
    for (int i = 0; i < 96; i++) {
        int ascii = i + 32, col = ascii % cols, row = ascii / cols;
        int startX = col * charW, startY = row * charH;
        const unsigned char* glyph = &FONT_DATA[i * 5];
        for (int x = 0; x < 5; x++) {
            unsigned char strip = glyph[x];
            for (int y = 0; y < 8; y++) {
                if ((strip >> y) & 1) {
                    int px = startX + x + 1, py = startY + y + 1;
                    int idx = (py * texW + px) * 4;
                    if (idx < size - 4) {
                        data[idx+0] = 255; data[idx+1] = 255; 
                        data[idx+2] = 255; data[idx+3] = 255;
                    }
                }
            }
        }
    }
    
    g_activeFont.textureId = CreateTextureFromData(data, texW, texH);
    g_activeFont.texWidth = texW;
    g_activeFont.texHeight = texH;
    free(data);
    
    for (int i = 0; i < 256; i++) {
        int col = i % cols, row = i / cols;
        g_activeFont.glyphs[i].u0 = (float)(col * charW) / texW;
        g_activeFont.glyphs[i].v0 = (float)(row * charH) / texH;
        g_activeFont.glyphs[i].u1 = (float)((col+1) * charW) / texW;
        g_activeFont.glyphs[i].v1 = (float)((row+1) * charH) / texH;
        g_activeFont.glyphs[i].width = (float)charW;
        g_activeFont.glyphs[i].height = (float)charH;
        g_activeFont.glyphs[i].advance = (float)charW;
        g_activeFont.glyphs[i].xoff = 0;
        g_activeFont.glyphs[i].yoff = 0;
    }
    g_activeFont.loaded = 1;
}

int GetValue(const char* line, const char* key) {
    char search[64];
    sprintf(search, "%s=", key);
    char* found = strstr(line, search);
    if (!found) return 0;
    return atoi(found + strlen(search));
}

void LoadFontFromFile(const char* name) {
    char path[256];
    sprintf(path, "Content/%s.fnt", name);
    FILE* file = fopen(path, "r");
    if (!file) { printf("[System] Font file not found: %s - Using debug font\n", path); GenerateDebugFont(); return; }
    printf("[System] Loading font from %s...\n", path);
    char line[512], texFilename[256] = {0};
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "page id=0")) {
            char* fileStart = strstr(line, "file=\"");
            if (fileStart) {
                fileStart += 6;
                char* fileEnd = strchr(fileStart, '"');
                if (fileEnd) {
                    int len = (int)(fileEnd - fileStart);
                    if (len < sizeof(texFilename)) { strncpy(texFilename, fileStart, len); texFilename[len] = '\0'; }
                }
            }
        }
        if (strstr(line, "char id=")) {
            int id = GetValue(line, "id");
            if (id >= 0 && id < 256) {
                int x = GetValue(line, "x"), y = GetValue(line, "y");
                int width = GetValue(line, "width"), height = GetValue(line, "height");
                int xoffset = GetValue(line, "xoffset"), yoffset = GetValue(line, "yoffset");
                int xadvance = GetValue(line, "xadvance");
                g_activeFont.glyphs[id].u0 = (float)x / g_activeFont.texWidth;
                g_activeFont.glyphs[id].v0 = (float)y / g_activeFont.texHeight;
                g_activeFont.glyphs[id].u1 = (float)(x + width) / g_activeFont.texWidth;
                g_activeFont.glyphs[id].v1 = (float)(y + height) / g_activeFont.texHeight;
                g_activeFont.glyphs[id].width = (float)width;
                g_activeFont.glyphs[id].height = (float)height;
                g_activeFont.glyphs[id].advance = (float)xadvance;
                g_activeFont.glyphs[id].xoff = (float)xoffset;
                g_activeFont.glyphs[id].yoff = (float)yoffset;
            }
        }
    }
    fclose(file);
    if (texFilename[0] != '\0') {
        char texPath[512];
        sprintf(texPath, "Content/%s", texFilename);
        int w, h;
        g_activeFont.textureId = LoadTexture(texPath, &w, &h);
        g_activeFont.texWidth = w;
        g_activeFont.texHeight = h;
        if (g_activeFont.textureId) { g_activeFont.loaded = 1; printf("[System] Font texture loaded: %s\n", texPath); }
        else { printf("[System] Font texture failed to load: %s\n", texPath); GenerateDebugFont(); }
    } else { printf("[System] No texture filename found\n"); GenerateDebugFont(); }
}

static int l_draw_rect(lua_State* L) {
    SetBatchTexture(g_whiteTexId);
    float x = (float)luaL_checknumber(L, 1), y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3), h = (float)luaL_checknumber(L, 4);
    int r = (int)luaL_checknumber(L, 5), g = (int)luaL_checknumber(L, 6);
    int b = (int)luaL_checknumber(L, 7), a = (int)luaL_checknumber(L, 8);
    unsigned int color = (a << 24) | (b << 16) | (g << 8) | r;
    if (g_uiVertCount + 6 >= MAX_UI_VERTS) FlushBatch();
    UIVertex* v = &g_uiVerts[g_uiVertCount];
    v[0] = (UIVertex){x, y, 0,0, color}; v[1] = (UIVertex){x+w, y, 1,0, color}; v[2] = (UIVertex){x, y+h, 0,1, color};
    v[3] = (UIVertex){x+w, y, 1,0, color}; v[4] = (UIVertex){x+w, y+h, 1,1, color}; v[5] = (UIVertex){x, y+h, 0,1, color};
    g_uiVertCount += 6;
    return 0;
}

static int l_draw_text(lua_State* L) {
    if (!g_activeFont.loaded) return 0;
    const char* text = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2), y = (float)luaL_checknumber(L, 3);
    int r = (int)luaL_checknumber(L, 4), g = (int)luaL_checknumber(L, 5);
    int b = (int)luaL_checknumber(L, 6), a = (int)luaL_checknumber(L, 7);
    unsigned int color = (a << 24) | (b << 16) | (g << 8) | r;
    SetBatchTexture(g_activeFont.textureId);
    float curX = x, curY = y;
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\n') { curX = x; curY += g_activeFont.glyphs[(int)'A'].height; continue; }
        Glyph* glyph = &g_activeFont.glyphs[c];
        if (g_uiVertCount + 6 >= MAX_UI_VERTS) FlushBatch();
        float gx = curX + glyph->xoff, gy = curY + glyph->yoff;
        float gw = glyph->width, gh = glyph->height;
        UIVertex* v = &g_uiVerts[g_uiVertCount];
        v[0] = (UIVertex){gx, gy, glyph->u0, glyph->v0, color};
        v[1] = (UIVertex){gx+gw, gy, glyph->u1, glyph->v0, color};
        v[2] = (UIVertex){gx, gy+gh, glyph->u0, glyph->v1, color};
        v[3] = (UIVertex){gx+gw, gy, glyph->u1, glyph->v0, color};
        v[4] = (UIVertex){gx+gw, gy+gh, glyph->u1, glyph->v1, color};
        v[5] = (UIVertex){gx, gy+gh, glyph->u0, glyph->v1, color};
        g_uiVertCount += 6;
        curX += glyph->advance;
    }
    return 0;
}

static int l_measure_text(lua_State* L) {
    if (!g_activeFont.loaded) { lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 2; }
    const char* text = luaL_checkstring(L, 1);
    float width = 0, height = 0;
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];
        Glyph* glyph = &g_activeFont.glyphs[c];
        width += glyph->advance;
        if (glyph->height > height) height = glyph->height;
    }
    lua_pushnumber(L, width);
    lua_pushnumber(L, height);
    return 2;
}

static int l_draw_border(lua_State* L) {
    SetBatchTexture(g_whiteTexId);
    float x = (float)luaL_checknumber(L, 1), y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3), h = (float)luaL_checknumber(L, 4);
    int r = (int)luaL_checknumber(L, 5), g = (int)luaL_checknumber(L, 6);
    int b = (int)luaL_checknumber(L, 7), a = (int)luaL_checknumber(L, 8);
    float thickness = (float)luaL_checknumber(L, 9);
    unsigned int color = (a << 24) | (b << 16) | (g << 8) | r;
    if (g_uiVertCount + 24 >= MAX_UI_VERTS) FlushBatch();
    UIVertex* v = &g_uiVerts[g_uiVertCount];
    v[0] = (UIVertex){x, y, 0,0, color}; v[1] = (UIVertex){x+w, y, 1,0, color}; v[2] = (UIVertex){x, y+thickness, 0,1, color};
    v[3] = (UIVertex){x+w, y, 1,0, color}; v[4] = (UIVertex){x+w, y+thickness, 1,1, color}; v[5] = (UIVertex){x, y+thickness, 0,1, color};
    v[6] = (UIVertex){x, y+h-thickness, 0,0, color}; v[7] = (UIVertex){x+w, y+h-thickness, 1,0, color}; v[8] = (UIVertex){x, y+h, 0,1, color};
    v[9] = (UIVertex){x+w, y+h-thickness, 1,0, color}; v[10] = (UIVertex){x+w, y+h, 1,1, color}; v[11] = (UIVertex){x, y+h, 0,1, color};
    v[12] = (UIVertex){x, y, 0,0, color}; v[13] = (UIVertex){x+thickness, y, 1,0, color}; v[14] = (UIVertex){x, y+h, 0,1, color};
    v[15] = (UIVertex){x+thickness, y, 1,0, color}; v[16] = (UIVertex){x+thickness, y+h, 1,1, color}; v[17] = (UIVertex){x, y+h, 0,1, color};
    v[18] = (UIVertex){x+w-thickness, y, 0,0, color}; v[19] = (UIVertex){x+w, y, 1,0, color}; v[20] = (UIVertex){x+w-thickness, y+h, 0,1, color};
    v[21] = (UIVertex){x+w, y, 1,0, color}; v[22] = (UIVertex){x+w, y+h, 1,1, color}; v[23] = (UIVertex){x+w-thickness, y+h, 0,1, color};
    g_uiVertCount += 24;
    return 0;
}

static int l_draw_texture(lua_State* L) {
    int texId = (int)luaL_checknumber(L, 1);
    float x = (float)luaL_checknumber(L, 2), y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_checknumber(L, 4), h = (float)luaL_checknumber(L, 5);
    unsigned int color = 0xFFFFFFFF; /* White */
    SetBatchTexture(texId);
    if (g_uiVertCount + 6 >= MAX_UI_VERTS) FlushBatch();
    UIVertex* v = &g_uiVerts[g_uiVertCount];
    v[0] = (UIVertex){x, y, 0,0, color}; v[1] = (UIVertex){x+w, y, 1,0, color}; v[2] = (UIVertex){x, y+h, 0,1, color};
    v[3] = (UIVertex){x+w, y, 1,0, color}; v[4] = (UIVertex){x+w, y+h, 1,1, color}; v[5] = (UIVertex){x, y+h, 0,1, color};
    g_uiVertCount += 6;
    return 0;
}

static int l_load_texture(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int w, h;
    int texId = LoadTexture(path, &w, &h);
    if (texId == 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushnumber(L, texId);
    lua_pushnumber(L, w);
    lua_pushnumber(L, h);
    return 3;
}

static int l_write_file(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    const char* data = luaL_checkstring(L, 2);
    FILE* file = fopen(path, "w");
    if (!file) { lua_pushboolean(L, 0); return 1; }
    fprintf(file, "%s", data);
    fclose(file);
    printf("[System] Wrote file: %s\n", path);
    lua_pushboolean(L, 1);
    return 1;
}

void UpdateProjection(int w, int h) {
    float L = 0, R = (float)w, T = 0, B = (float)h;
    float ortho[16] = {2.0f/(R-L), 0, 0, 0, 0, 2.0f/(T-B), 0, 0, 0, 0, -1, 0, -(R+L)/(R-L), -(T+B)/(T-B), 0, 1};
    SetProjectionMatrix(ortho);
    UpdateViewport(w, h);
}

#ifdef __EMSCRIPTEN__
/* Emscripten main loop function */
void emscripten_main_loop() {
    EmscriptenLoopData* data = &g_loopData;
    
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            data->running = 0;
            emscripten_cancel_main_loop();
            return;
        }
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
            data->winW = e.window.data1;
            data->winH = e.window.data2;
            UpdateProjection(data->winW, data->winH);
        }
        if (e.type == SDL_KEYDOWN) {
            const char* keyName = SDL_GetKeyName(e.key.keysym.sym);
            lua_getglobal(data->L, "HandleKeyPress");
            if (lua_isfunction(data->L, -1)) {
                char simpleName[32] = {0};
                if (strcmp(keyName, "Up") == 0) strcpy(simpleName, "up");
                else if (strcmp(keyName, "Down") == 0) strcpy(simpleName, "down");
                else if (strcmp(keyName, "Left") == 0) strcpy(simpleName, "left");
                else if (strcmp(keyName, "Right") == 0) strcpy(simpleName, "right");
                else if (strcmp(keyName, "Space") == 0) strcpy(simpleName, "space");
                else if (strlen(keyName) == 1) { simpleName[0] = keyName[0]; simpleName[1] = '\0'; }
                else strncpy(simpleName, keyName, sizeof(simpleName) - 1);
                lua_pushstring(data->L, simpleName);
                if (lua_pcall(data->L, 1, 0, 0) != LUA_OK) {
                    printf("HandleKeyPress Error: %s\n", lua_tostring(data->L, -1));
                    lua_pop(data->L, 1);
                }
            } else {
                lua_pop(data->L, 1);
            }
        }
    }
    
    int mouseX = 0, mouseY = 0;
    Uint32 buttons = SDL_GetMouseState(&mouseX, &mouseY);
    int mouseDown = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    
    lua_getglobal(data->L, "UpdateUI");
    lua_pushnumber(data->L, mouseX);
    lua_pushnumber(data->L, mouseY);
    lua_pushboolean(data->L, mouseDown);
    lua_pushnumber(data->L, data->winW);
    lua_pushnumber(data->L, data->winH);
    if (lua_pcall(data->L, 5, 0, 0) != LUA_OK) {
        printf("Lua Update Error: %s\n", lua_tostring(data->L, -1));
        lua_pop(data->L, 1);
    }
    
    ClearScreen(0.1f, 0.1f, 0.15f, 1.0f);
    g_uiVertCount = 0;
    
    lua_getglobal(data->L, "DrawUI");
    if (lua_pcall(data->L, 0, 0, 0) != LUA_OK) {
        printf("Lua Draw Error: %s\n", lua_tostring(data->L, -1));
        lua_pop(data->L, 1);
    }
    
    FlushBatch();
    BridgeSwapBuffers();
}
#endif

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;
    
    int winW = 1024, winH = 768;
    SDL_Window* window = SDL_CreateWindow("Project Bridge Lua UI", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        winW, winH, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    
    void* nativeWindow = NULL;
#ifndef __EMSCRIPTEN__
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    nativeWindow = (void*)wmInfo.info.win.window;
#endif
    
    InitEngine(nativeWindow, winW, winH);
    g_whiteTexId = CreateWhiteTexture();
    g_currentTexId = g_whiteTexId;
    LoadFontFromFile("custom");
    UpdateProjection(winW, winH);

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_newtable(L);
    lua_pushcfunction(L, l_draw_rect); lua_setfield(L, -2, "drawRect");
    lua_pushcfunction(L, l_draw_text); lua_setfield(L, -2, "drawText");
    lua_pushcfunction(L, l_measure_text); lua_setfield(L, -2, "measureText");
    lua_pushcfunction(L, l_draw_border); lua_setfield(L, -2, "drawBorder");
    lua_pushcfunction(L, l_write_file); lua_setfield(L, -2, "writeFile");
    lua_pushcfunction(L, l_load_texture); lua_setfield(L, -2, "loadTexture");
    lua_pushcfunction(L, l_draw_texture); lua_setfield(L, -2, "drawTexture");
    lua_setglobal(L, "bridge");

    const char* scriptName = (argc > 1) ? argv[1] : "demo";
    char scriptPath[256];
    sprintf(scriptPath, "%s.lua", scriptName);
    if (luaL_dofile(L, "framework.lua") != LUA_OK) {
        printf("Error loading framework: %s\n", lua_tostring(L, -1));
        return 1;
    }
    if (luaL_dofile(L, scriptPath) != LUA_OK) {
        printf("Error loading %s: %s\n", scriptPath, lua_tostring(L, -1));
        return 1;
    }
    printf("[System] Running: %s\n", scriptPath);

#ifdef __EMSCRIPTEN__
    /* Setup Emscripten loop data */
    g_loopData.window = window;
    g_loopData.L = L;
    g_loopData.winW = winW;
    g_loopData.winH = winH;
    g_loopData.running = 1;
    
    /* Start Emscripten main loop */
    emscripten_set_main_loop(emscripten_main_loop, 0, 1);
#else
    /* Traditional desktop main loop */
    int running = 1, mouseX = 0, mouseY = 0, mouseDown = 0;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                winW = e.window.data1; winH = e.window.data2;
                UpdateProjection(winW, winH);
            }
            if (e.type == SDL_KEYDOWN) {
                const char* keyName = SDL_GetKeyName(e.key.keysym.sym);
                lua_getglobal(L, "HandleKeyPress");
                if (lua_isfunction(L, -1)) {
                    char simpleName[32] = {0};
                    if (strcmp(keyName, "Up") == 0) strcpy(simpleName, "up");
                    else if (strcmp(keyName, "Down") == 0) strcpy(simpleName, "down");
                    else if (strcmp(keyName, "Left") == 0) strcpy(simpleName, "left");
                    else if (strcmp(keyName, "Right") == 0) strcpy(simpleName, "right");
                    else if (strcmp(keyName, "Space") == 0) strcpy(simpleName, "space");
                    else if (strlen(keyName) == 1) { simpleName[0] = keyName[0]; simpleName[1] = '\0'; }
                    else strncpy(simpleName, keyName, sizeof(simpleName) - 1);
                    lua_pushstring(L, simpleName);
                    if (lua_pcall(L, 1, 0, 0) != LUA_OK) { printf("HandleKeyPress Error: %s\n", lua_tostring(L, -1)); lua_pop(L, 1); }
                } else lua_pop(L, 1);
            }
        }
        Uint32 buttons = SDL_GetMouseState(&mouseX, &mouseY);
        mouseDown = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        lua_getglobal(L, "UpdateUI");
        lua_pushnumber(L, mouseX); lua_pushnumber(L, mouseY); lua_pushboolean(L, mouseDown);
        lua_pushnumber(L, winW); lua_pushnumber(L, winH);
        if (lua_pcall(L, 5, 0, 0) != LUA_OK) { printf("Lua Update Error: %s\n", lua_tostring(L, -1)); lua_pop(L, 1); }
        ClearScreen(0.1f, 0.1f, 0.15f, 1.0f);
        g_uiVertCount = 0;
        lua_getglobal(L, "DrawUI");
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) { printf("Lua Draw Error: %s\n", lua_tostring(L, -1)); lua_pop(L, 1); }
        FlushBatch();
        BridgeSwapBuffers();
        SDL_Delay(16);
    }
#endif

    ShutdownEngine();
    lua_close(L);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
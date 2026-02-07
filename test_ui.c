/* test_ui.c */
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
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
    Glyph glyphs[256]; /* ASCII 0-255 support */
    int loaded;
} Font;

/* Global State */
#define MAX_UI_VERTS 10000
UIVertex g_uiVerts[MAX_UI_VERTS];
int g_uiVertCount = 0;

int g_whiteTexId = 0;
int g_currentTexId = 0;
Font g_activeFont = {0};

/* --- Embedded Fallback Font Data --- */
const unsigned char FONT_DATA[] = {
    0x00,0x00,0x00,0x00,0x00, /* Space */
    0x00,0x00,0x5F,0x00,0x00, /* ! */
    0x00,0x07,0x00,0x07,0x00, /* " */
    0x14,0x7F,0x14,0x7F,0x14, /* # */
    0x24,0x2A,0x7F,0x2A,0x12, /* $ */
    0x23,0x13,0x08,0x64,0x62, /* % */
    0x36,0x49,0x55,0x22,0x50, /* & */
    0x00,0x05,0x03,0x00,0x00, /* ' */
    0x00,0x1C,0x22,0x41,0x00, /* ( */
    0x00,0x41,0x22,0x1C,0x00, /* ) */
    0x14,0x08,0x3E,0x08,0x14, /* * */
    0x08,0x08,0x3E,0x08,0x08, /* + */
    0x00,0x50,0x30,0x00,0x00, /* , */
    0x08,0x08,0x08,0x08,0x08, /* - */
    0x00,0x60,0x60,0x00,0x00, /* . */
    0x20,0x10,0x08,0x04,0x02, /* / */
    0x3E,0x51,0x49,0x45,0x3E, /* 0 */
    0x00,0x42,0x7F,0x40,0x00, /* 1 */
    0x42,0x61,0x51,0x49,0x46, /* 2 */
    0x21,0x41,0x45,0x4B,0x31, /* 3 */
    0x18,0x14,0x12,0x7F,0x10, /* 4 */
    0x27,0x45,0x45,0x45,0x39, /* 5 */
    0x3C,0x4A,0x49,0x49,0x30, /* 6 */
    0x01,0x71,0x09,0x05,0x03, /* 7 */
    0x36,0x49,0x49,0x49,0x36, /* 8 */
    0x06,0x49,0x49,0x29,0x1E, /* 9 */
    0x00,0x36,0x36,0x00,0x00, /* : */
    0x00,0x56,0x36,0x00,0x00, /* ; */
    0x08,0x14,0x22,0x41,0x00, /* < */
    0x14,0x14,0x14,0x14,0x14, /* = */
    0x00,0x41,0x22,0x14,0x08, /* > */
    0x02,0x01,0x51,0x09,0x06, /* ? */
    0x32,0x49,0x79,0x41,0x3E, /* @ */
    0x7E,0x11,0x11,0x11,0x7E, /* A */
    0x7F,0x49,0x49,0x49,0x36, /* B */
    0x3E,0x41,0x41,0x41,0x22, /* C */
    0x7F,0x41,0x41,0x22,0x1C, /* D */
    0x7F,0x49,0x49,0x49,0x41, /* E */
    0x7F,0x09,0x09,0x09,0x01, /* F */
    0x3E,0x41,0x49,0x49,0x7A, /* G */
    0x7F,0x08,0x08,0x08,0x7F, /* H */
    0x00,0x41,0x7F,0x41,0x00, /* I */
    0x20,0x40,0x41,0x3F,0x01, /* J */
    0x7F,0x08,0x14,0x22,0x41, /* K */
    0x7F,0x40,0x40,0x40,0x40, /* L */
    0x7F,0x02,0x0C,0x02,0x7F, /* M */
    0x7F,0x04,0x08,0x10,0x7F, /* N */
    0x3E,0x41,0x41,0x41,0x3E, /* O */
    0x7F,0x09,0x09,0x09,0x06, /* P */
    0x3E,0x41,0x51,0x21,0x5E, /* Q */
    0x7F,0x09,0x19,0x29,0x46, /* R */
    0x46,0x49,0x49,0x49,0x31, /* S */
    0x01,0x01,0x7F,0x01,0x01, /* T */
    0x3F,0x40,0x40,0x40,0x3F, /* U */
    0x1F,0x20,0x40,0x20,0x1F, /* V */
    0x3F,0x40,0x38,0x40,0x3F, /* W */
    0x63,0x14,0x08,0x14,0x63, /* X */
    0x07,0x08,0x70,0x08,0x07, /* Y */
    0x61,0x51,0x49,0x45,0x43, /* Z */
    0x00,0x7F,0x41,0x41,0x00, /* [ */
    0x02,0x04,0x08,0x10,0x20, /* \ */
    0x00,0x41,0x41,0x7F,0x00, /* ] */
    0x04,0x02,0x01,0x02,0x04, /* ^ */
    0x40,0x40,0x40,0x40,0x40, /* _ */
    0x00,0x01,0x02,0x04,0x00, /* ` */
    0x20,0x54,0x54,0x54,0x78, /* a */
    0x7F,0x48,0x44,0x44,0x38, /* b */
    0x38,0x44,0x44,0x44,0x20, /* c */
    0x38,0x44,0x44,0x48,0x7F, /* d */
    0x38,0x54,0x54,0x54,0x18, /* e */
    0x08,0x7E,0x09,0x01,0x02, /* f */
    0x0C,0x52,0x52,0x52,0x3E, /* g */
    0x7F,0x08,0x04,0x04,0x78, /* h */
    0x00,0x44,0x7D,0x40,0x00, /* i */
    0x20,0x40,0x44,0x3D,0x00, /* j */
    0x7F,0x10,0x28,0x44,0x00, /* k */
    0x00,0x41,0x7F,0x40,0x00, /* l */
    0x7C,0x04,0x18,0x04,0x78, /* m */
    0x7C,0x08,0x04,0x04,0x78, /* n */
    0x38,0x44,0x44,0x44,0x38, /* o */
    0x7C,0x14,0x14,0x14,0x08, /* p */
    0x08,0x14,0x14,0x18,0x7C, /* q */
    0x7C,0x08,0x04,0x04,0x08, /* r */
    0x48,0x54,0x54,0x54,0x20, /* s */
    0x04,0x3F,0x44,0x40,0x20, /* t */
    0x3C,0x40,0x40,0x20,0x7C, /* u */
    0x1C,0x20,0x40,0x20,0x1C, /* v */
    0x3C,0x40,0x30,0x40,0x3C, /* w */
    0x44,0x28,0x10,0x28,0x44, /* x */
    0x0C,0x50,0x50,0x50,0x3C, /* y */
    0x44,0x64,0x54,0x4C,0x44, /* z */
    0x00,0x08,0x36,0x41,0x00, /* { */
    0x00,0x00,0x7F,0x00,0x00, /* | */
    0x00,0x41,0x36,0x08,0x00, /* } */
    0x10,0x08,0x08,0x10,0x08, /* ~ */
    0x7F,0x7F,0x7F,0x7F,0x7F  /* DEL */
};

/* --- Batching System --- */

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

/* --- Font System --- */

void GenerateDebugFont() {
    printf("[System] Generating Procedural Debug Font...\n");
    int charW = 8;
    int charH = 8;
    int cols = 16;
    int rows = 8;
    int texW = cols * charW; /* 128 */
    int texH = rows * charH; /* 64 */
    
    int size = texW * texH * 4;
    unsigned char* data = (unsigned char*)malloc(size);
    memset(data, 0, size);
    
    for (int i = 0; i < 96; i++) {
        int ascii = i + 32;
        int col = ascii % cols;
        int row = ascii / cols;
        int startX = col * charW;
        int startY = row * charH;
        
        const unsigned char* glyph = &FONT_DATA[i * 5];
        
        for (int x = 0; x < 5; x++) {
            unsigned char strip = glyph[x];
            for (int y = 0; y < 8; y++) {
                if ((strip >> y) & 1) {
                    int px = startX + x + 1;
                    int py = startY + y + 1;
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
    
    /* Generate standard glyphs for this grid */
    for (int i = 0; i < 256; i++) {
        int col = i % cols;
        int row = i / cols;
        g_activeFont.glyphs[i].u0 = (float)(col * charW) / texW;
        g_activeFont.glyphs[i].v0 = (float)(row * charH) / texH;
        g_activeFont.glyphs[i].u1 = (float)((col+1) * charW) / texW;
        g_activeFont.glyphs[i].v1 = (float)((row+1) * charH) / texH;
        g_activeFont.glyphs[i].width = (float)charW;   /* Match texture size for pixel-perfect rendering */
        g_activeFont.glyphs[i].height = (float)charH;
        g_activeFont.glyphs[i].advance = (float)charW;
        g_activeFont.glyphs[i].xoff = 0;
        g_activeFont.glyphs[i].yoff = 0;
    }
    g_activeFont.loaded = 1;
}

/* Helper to parse key=value in BMFont files */
int GetValue(const char* line, const char* key) {
    char search[64];
    sprintf(search, "%s=", key);
    char* found = strstr(line, search);
    if (!found) return 0;
    return atoi(found + strlen(search));
}

void LoadFontFromFile(const char* name) {
    /* 1. Try to open .fnt file */
    char path[256];
    sprintf(path, "Content/%s.fnt", name);
    
    FILE* file = fopen(path, "r");
    if (!file) {
        printf("[System] Font file not found: %s - Using debug font\n", path);
        GenerateDebugFont();
        return;
    }
    
    printf("[System] Loading font from %s...\n", path);
    
    /* 2. Parse FNT */
    char line[512];
    char texFilename[256] = {0};
    
    while (fgets(line, sizeof(line), file)) {
        /* Parse 'page' line for texture */
        if (strstr(line, "page id=0")) {
            char* fileStart = strstr(line, "file=\"");
            if (fileStart) {
                fileStart += 6;
                char* fileEnd = strchr(fileStart, '"');
                if (fileEnd) {
                    int len = (int)(fileEnd - fileStart);
                    if (len < sizeof(texFilename)) {
                        strncpy(texFilename, fileStart, len);
                        texFilename[len] = '\0';
                    }
                }
            }
        }
        
        /* Parse 'char' lines for glyph metrics */
        if (strstr(line, "char id=")) {
            int id = GetValue(line, "id");
            if (id >= 0 && id < 256) {
                /* Store pixel values temporarily in the uv coords */
                float x = (float)GetValue(line, "x");
                float y = (float)GetValue(line, "y");
                float w = (float)GetValue(line, "width");
                float h = (float)GetValue(line, "height");
                
                g_activeFont.glyphs[id].u0 = x;
                g_activeFont.glyphs[id].v0 = y;
                g_activeFont.glyphs[id].u1 = w; /* Temp store width here */
                g_activeFont.glyphs[id].v1 = h; /* Temp store height here */
                g_activeFont.glyphs[id].width = w;
                g_activeFont.glyphs[id].height = h;
                g_activeFont.glyphs[id].advance = (float)GetValue(line, "xadvance");
                g_activeFont.glyphs[id].xoff = (float)GetValue(line, "xoffset");
                g_activeFont.glyphs[id].yoff = (float)GetValue(line, "yoffset");
            }
        }
    }
    fclose(file);
    
    /* 3. Load Texture */
    if (strlen(texFilename) > 0) {
        char texPath[256];
        /* Try same folder as FNT first (simple assumption for this demo) */
        /* If FNT was in Content/, try Content/PNG */
        if (strstr(path, "Content/")) {
            sprintf(texPath, "Content/%s", texFilename);
        } else {
            strcpy(texPath, texFilename);
        }
        
        int w, h;
        g_activeFont.textureId = LoadTexture(texPath, &w, &h);
        g_activeFont.texWidth = w;
        g_activeFont.texHeight = h;
        
        if (g_activeFont.textureId == 0) {
            printf("[Error] Failed to load font texture: %s\n", texPath);
            GenerateDebugFont();
            return;
        }
        
        /* 4. Normalize UVs */
        for (int i = 0; i < 256; i++) {
            /* Retrieve pixel values stored earlier */
            float x = g_activeFont.glyphs[i].u0;
            float y = g_activeFont.glyphs[i].v0;
            float w = g_activeFont.glyphs[i].u1; /* Width in px */
            float h = g_activeFont.glyphs[i].v1; /* Height in px */
            
            /* Add small epsilon to prevent texture bleeding (0.5 pixel inset) */
            float epsilon = 0.5f;
            
            /* Convert to 0.0-1.0 with padding */
            g_activeFont.glyphs[i].u0 = (x + epsilon) / g_activeFont.texWidth;
            g_activeFont.glyphs[i].v0 = (y + epsilon) / g_activeFont.texHeight;
            g_activeFont.glyphs[i].u1 = (x + w - epsilon) / g_activeFont.texWidth;
            g_activeFont.glyphs[i].v1 = (y + h - epsilon) / g_activeFont.texHeight;
            
            /* Restore dimensions */
            g_activeFont.glyphs[i].width = w;
            g_activeFont.glyphs[i].height = h;
        }
        g_activeFont.loaded = 1;
        printf("[System] Font Loaded Successfully.\n");
    } else {
        GenerateDebugFont();
    }
}

/* --- Lua API --- */

static int l_draw_rect(lua_State* L) {
    SetBatchTexture(g_whiteTexId);
    
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);
    
    int r = (int)luaL_checknumber(L, 5);
    int g = (int)luaL_checknumber(L, 6);
    int b = (int)luaL_checknumber(L, 7);
    int a = (int)luaL_checknumber(L, 8);
    
    unsigned int color = (a << 24) | (b << 16) | (g << 8) | r;

    if (g_uiVertCount + 6 >= MAX_UI_VERTS) FlushBatch();

    UIVertex* v = &g_uiVerts[g_uiVertCount];
    
    v[0] = (UIVertex){x, y,     0,0, color};
    v[1] = (UIVertex){x+w, y,   1,0, color};
    v[2] = (UIVertex){x, y+h,   0,1, color};
    v[3] = (UIVertex){x+w, y,   1,0, color};
    v[4] = (UIVertex){x+w, y+h, 1,1, color};
    v[5] = (UIVertex){x, y+h,   0,1, color};
    
    g_uiVertCount += 6;
    return 0;
}

static int l_draw_text(lua_State* L) {
    if (!g_activeFont.loaded) return 0;
    SetBatchTexture(g_activeFont.textureId);
    
    float startX = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    const char* text = luaL_checkstring(L, 3);
    
    int r = (int)luaL_checknumber(L, 4);
    int g = (int)luaL_checknumber(L, 5);
    int b = (int)luaL_checknumber(L, 6);
    unsigned int color = (255 << 24) | (b << 16) | (g << 8) | r;
    
    float curX = startX;
    
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];
        
        Glyph* glyph = &g_activeFont.glyphs[c];
        
        /* Skip if invalid or empty (space usually has width but no texture, that's fine) */
        
        if (g_uiVertCount + 6 >= MAX_UI_VERTS) FlushBatch();
        
        float gx = curX + glyph->xoff;
        float gy = y + glyph->yoff;
        float gw = glyph->width;
        float gh = glyph->height;
        
        UIVertex* v = &g_uiVerts[g_uiVertCount];
        
        v[0] = (UIVertex){gx, gy,       glyph->u0, glyph->v0, color};
        v[1] = (UIVertex){gx+gw, gy,    glyph->u1, glyph->v0, color};
        v[2] = (UIVertex){gx, gy+gh,    glyph->u0, glyph->v1, color};
        v[3] = (UIVertex){gx+gw, gy,    glyph->u1, glyph->v0, color};
        v[4] = (UIVertex){gx+gw, gy+gh, glyph->u1, glyph->v1, color};
        v[5] = (UIVertex){gx, gy+gh,    glyph->u0, glyph->v1, color};
        
        g_uiVertCount += 6;
        curX += glyph->advance;
    }
    
    return 0;
}

static int l_measure_text(lua_State* L) {
    if (!g_activeFont.loaded) {
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        return 2;
    }
    
    const char* text = luaL_checkstring(L, 1);
    
    float width = 0;
    float height = 0;
    
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];
        Glyph* glyph = &g_activeFont.glyphs[c];
        width += glyph->advance;
        
        if (glyph->height > height) {
            height = glyph->height;
        }
    }
    
    lua_pushnumber(L, width);
    lua_pushnumber(L, height);
    return 2;
}

static int l_draw_border(lua_State* L) {
    SetBatchTexture(g_whiteTexId);
    
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);
    
    int r = (int)luaL_checknumber(L, 5);
    int g = (int)luaL_checknumber(L, 6);
    int b = (int)luaL_checknumber(L, 7);
    int a = (int)luaL_checknumber(L, 8);
    float thickness = (float)luaL_checknumber(L, 9);
    
    unsigned int color = (a << 24) | (b << 16) | (g << 8) | r;
    
    if (g_uiVertCount + 24 >= MAX_UI_VERTS) FlushBatch();
    
    UIVertex* v = &g_uiVerts[g_uiVertCount];
    
    /* Top border */
    v[0] = (UIVertex){x, y, 0,0, color};
    v[1] = (UIVertex){x+w, y, 1,0, color};
    v[2] = (UIVertex){x, y+thickness, 0,1, color};
    v[3] = (UIVertex){x+w, y, 1,0, color};
    v[4] = (UIVertex){x+w, y+thickness, 1,1, color};
    v[5] = (UIVertex){x, y+thickness, 0,1, color};
    
    /* Bottom border */
    v[6] = (UIVertex){x, y+h-thickness, 0,0, color};
    v[7] = (UIVertex){x+w, y+h-thickness, 1,0, color};
    v[8] = (UIVertex){x, y+h, 0,1, color};
    v[9] = (UIVertex){x+w, y+h-thickness, 1,0, color};
    v[10] = (UIVertex){x+w, y+h, 1,1, color};
    v[11] = (UIVertex){x, y+h, 0,1, color};
    
    /* Left border */
    v[12] = (UIVertex){x, y, 0,0, color};
    v[13] = (UIVertex){x+thickness, y, 1,0, color};
    v[14] = (UIVertex){x, y+h, 0,1, color};
    v[15] = (UIVertex){x+thickness, y, 1,0, color};
    v[16] = (UIVertex){x+thickness, y+h, 1,1, color};
    v[17] = (UIVertex){x, y+h, 0,1, color};
    
    /* Right border */
    v[18] = (UIVertex){x+w-thickness, y, 0,0, color};
    v[19] = (UIVertex){x+w, y, 1,0, color};
    v[20] = (UIVertex){x+w-thickness, y+h, 0,1, color};
    v[21] = (UIVertex){x+w, y, 1,0, color};
    v[22] = (UIVertex){x+w, y+h, 1,1, color};
    v[23] = (UIVertex){x+w-thickness, y+h, 0,1, color};
    
    g_uiVertCount += 24;
    return 0;
}

/* --- Main Runtime --- */

void UpdateProjection(int w, int h) {
    float L = 0, R = (float)w, T = 0, B = (float)h;
    float ortho[16] = {
        2.0f/(R-L),    0,             0, 0,
        0,             2.0f/(T-B),    0, 0,
        0,             0,            -1, 0,
        -(R+L)/(R-L), -(T+B)/(T-B),   0, 1
    };
    SetProjectionMatrix(ortho);
    UpdateViewport(w, h);
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    int winW = 800;
    int winH = 600;

    SDL_Window* window = SDL_CreateWindow("Project Bridge Lua UI", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        winW, winH, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    void* nativeWindow = (void*)wmInfo.info.win.window;

    InitEngine(nativeWindow, winW, winH);
    
    /* Initialize Resources */
    g_whiteTexId = CreateWhiteTexture();
    g_currentTexId = g_whiteTexId;
    
    /* Try to load default font, fallback to embedded if missing */
    LoadFontFromFile("default");
    
    UpdateProjection(winW, winH);

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    /* Register Lua bridge functions */
    lua_newtable(L);
    lua_pushcfunction(L, l_draw_rect); 
    lua_setfield(L, -2, "drawRect");
    lua_pushcfunction(L, l_draw_text); 
    lua_setfield(L, -2, "drawText");
    lua_pushcfunction(L, l_measure_text); 
    lua_setfield(L, -2, "measureText");
    lua_pushcfunction(L, l_draw_border); 
    lua_setfield(L, -2, "drawBorder");
    lua_setglobal(L, "bridge");

    if (luaL_dofile(L, "framework.lua") != LUA_OK) {
        printf("Error loading framework: %s\n", lua_tostring(L, -1));
        return 1;
    }
    if (luaL_dofile(L, "demo.lua") != LUA_OK) {
        printf("Error loading demo: %s\n", lua_tostring(L, -1));
        return 1;
    }

    int running = 1;
    int mouseX = 0, mouseY = 0;
    int mouseDown = 0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                winW = e.window.data1;
                winH = e.window.data2;
                UpdateProjection(winW, winH);
            }
        }

        Uint32 buttons = SDL_GetMouseState(&mouseX, &mouseY);
        mouseDown = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;

        lua_getglobal(L, "UpdateUI");
        lua_pushnumber(L, mouseX);
        lua_pushnumber(L, mouseY);
        lua_pushboolean(L, mouseDown);
        lua_pushnumber(L, winW);
        lua_pushnumber(L, winH);
        if (lua_pcall(L, 5, 0, 0) != LUA_OK) {
            printf("Lua Update Error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }

        ClearScreen(0.1f, 0.1f, 0.15f, 1.0f);
        
        g_uiVertCount = 0;
        
        lua_getglobal(L, "DrawUI");
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            printf("Lua Draw Error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }

        FlushBatch();
        BridgeSwapBuffers();
        SDL_Delay(16);
    }

    ShutdownEngine();
    lua_close(L);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
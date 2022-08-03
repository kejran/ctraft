#pragma once

#include "3ds.h"
#include "citro2d.h"

#include <unordered_map>
#include <vector>
#include <array>
#include <cctype>
#include <string_view>

struct PixelGlyph {
    s16 x;
    s16 y;
    s8 width;
    s8 height;
    s8 offsetX;
    s8 offsetY;
    s8 advance;
};

struct PixelFont {
    std::unordered_map<u32, PixelGlyph> glyphs;

    C3D_Tex *texture;

    u16 width, height;

    float invU, invV;

    void init(C3D_Tex &tex, FILE* file) {
        invU = 1.0f / tex.width;
        invV = 1.0f / tex.height;

        texture = &tex;

        std::array<char, 256> buffer;

        char *data;
        
        auto skipspaces = [&data]() {
            while (*data == ' ' || *data == '\t') ++data;
        };
        
        auto skipNospaces = [&data]() {
            while (*data != ' ' && *data != '\t' && *data != 0) ++data;
        };

        auto parseInt = [&](int &i, char const *name, int len) -> bool {
            if (!strncmp(name, data, len)) {
                data += len;
                i = strtol(data, nullptr, 10);
                skipNospaces();
                return true;
            }
            return false;
        };
        
        while (fgets(buffer.data(), 256, file)) {
        
            data = buffer.data();

            if (!strncmp("common ", data, 7)) {

                data += 6; 

                int height = 0;

                while (*data != 0) {
                    skipspaces();
                    if (parseInt(height, "lineHeight=", 11))
                        continue;

                    skipNospaces();                
                }
                // todo use height for multiline?
                continue;
            }


            if (!strncmp("chars ", data, 6)) {
                data += 5;

                int count = 0;

                while (*data != 0) {
                    skipspaces();
                    if (parseInt(count, "count=", 6))
                        continue;

                    skipNospaces();                
                }

                if (count)
                    glyphs.reserve(count);
                
                continue;
            }

            if (!strncmp("char ", data, 5)) {
                data += 4; 

                int id = 0;
                int x = 0, y = 0, width = 0, height = 0;
                int xoffset = 0, yoffset = 0, xadvance = 0; 

                while (*data != 0) {
                    skipspaces();

                    if (parseInt(id, "id=", 3)) continue;

                    if (parseInt(x, "x=", 2)) continue;
                    if (parseInt(y, "y=", 2)) continue;

                    if (parseInt(width, "width=", 6)) continue;
                    if (parseInt(height, "height=", 7)) continue;

                    if (parseInt(xoffset, "xoffset=", 8)) continue;
                    if (parseInt(yoffset, "yoffset=", 8)) continue;

                    if (parseInt(xadvance, "xadvance=", 9)) continue;

                    skipNospaces();
                }

                auto &g = glyphs[id];
                g.x = x;
                g.y = y;
                g.width = width;
                g.height = height;
                g.offsetX = xoffset;
                g.offsetY = yoffset;
                g.advance = xadvance;

                continue;
            }
        }
    }
};

struct PixelQuad { 
    // store floats to directly load them into regs  
    // hardp conversion on arm11 is probably too slow for small memory gains  
    struct {
        float x, y, w, h;
    } pos;
    struct { 
        float u, v; 
    } texStart; 
    struct { 
        float u, v; 
    } texEnd;
};

struct PixelText { // todo: a separate builder structure?

    std::vector<PixelQuad> quads;
    C2D_ImageTint globalTint;
    int advance;
    float px, py;
    int scale;
    PixelFont *font;

    void init(PixelFont &font, int scale, float x, float y) {
        this->font = &font;
        setColor(0xff'ff'ff'ff);
        px = x; py = y;
        this->scale = scale;
        quads.clear();
        advance = 0;
    }

    void setColor(u32 color) {
        C2D_Tint tint { color, 0.0f };
        for (int i = 0; i < 4; ++i)
            globalTint.corners[i] = tint;
    }

    void addGlyph(PixelGlyph const &g) {

        // skip whitespace quads
        if (g.width && g.height) {

            auto &q = quads.emplace_back();
            q.pos.w = scale * g.width;
            q.pos.h = scale * g.height;
            q.pos.x = advance + px + scale * g.offsetX;
            q.pos.y = py + scale * g.offsetY;

            q.texStart.u = g.x * font->invU;
            q.texStart.v = 1-g.y * font->invV;
            
            q.texEnd.u = (g.x + g.width) * font->invU;
            q.texEnd.v = 1-(g.y + g.height) * font->invV;
        }

        advance += scale * g.advance;
    }

    void addCodepoint(u32 c) {
        auto it = font->glyphs.find(c);
        if (it != font->glyphs.end())
            addGlyph(it->second);
    }

    // todo: have a 2-pass bake so we can get width before positioning
    void addString(char const *str) {
        u32 code;
        u8 *p = (u8*)str;
        
        while (true) {
            auto status = decode_utf8(&code, p);
            if (status == -1) break; // invalid data, ignore for now
            if (status == 0) break; // end of string
            if (code == 0) break;
            p += status;
            addCodepoint(code);
        }
    }

    void addString(std::string_view view) {
        u32 code;
        u8 *p = (u8 *)view.begin();
        u8 *end = (u8 *)view.end();
        int i = 0;
        while (p != end) {
            auto status = decode_utf8(&code, p);
            if (status == -1) break; // invalid data, ignore for now
            if (status == 0) break; // end of string
            p += status;
            addCodepoint(code);
        }
    }

    void draw() {
        // C2D_SetTintMode(C2D_TintMult);
        Tex3DS_SubTexture subtex { font->texture->width, font->texture->height };
        C2D_Image image { font->texture, &subtex };
        C2D_DrawParams params { { 0, 0, 1, 1 }, { 0, 0 }, 0, 0 };

        for (auto &q: quads) { // todo: maybe alias and memcpy it?
            params.pos.x = q.pos.x;
            params.pos.y = q.pos.y;
            params.pos.w = q.pos.w;
            params.pos.h = q.pos.h;
            
            subtex.left = q.texStart.u;
            subtex.top = q.texStart.v;
            subtex.right = q.texEnd.u;
            subtex.bottom = q.texEnd.v;

            C2D_DrawImage(image, &params, &globalTint);
        }
    }

    int width() const {
        return advance;
    }
};

#include "localization.h"
#include "brender/brender.h"
#include "displays.h"
#include "flicplay.h"
#include "globvars.h"
#include "grafdata.h"
#include "graphics.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "localization_catalog.h"

static int language;

void DRLanguageInit(const char* code) {
    language = code && !strcmp(code, "es") ? 1 : code && !strcmp(code, "it") ? 2 : code && !strcmp(code, "fr") ? 3 : 0;
}
int DRLanguageActive(void) { return language != 0; }

char* DRLocalize(const char* english) {
    int low = 0, high = sizeof(kLocalizedStrings) / sizeof(kLocalizedStrings[0]) - 1;
    if (!language || !english) return (char*)english;
    while (low <= high) {
        int middle = low + (high - low) / 2;
        int comparison = strcmp(english, kLocalizedStrings[middle][0]);
        if (comparison == 0) return (char*)kLocalizedStrings[middle][language];
        if (comparison < 0) high = middle - 1; else low = middle + 1;
    }
    return (char*)english;
}
char* DRLocalizeMisc(int index, char* original) {
    char key[32]; char* result;
    if (!language) return original;
    snprintf(key, sizeof(key), "@misc:%d", index);
    result = DRLocalize(key);
    return result == key ? DRLocalize(original) : result;
}

// Decode valid Latin-1 UTF-8 and common punctuation. Negative results retain
// the engine's single-byte font-change commands (F8, F9, FA, ...).
int DRLanguageNext(const unsigned char** text) {
    const unsigned char* p = *text;
    int result = *p;
    if ((p[0] == 0xc2 || p[0] == 0xc3) && p[1] >= 0x80 && p[1] <= 0xbf) {
        result = ((p[0] & 31) << 6) | (p[1] & 63); *text += 2;
    } else if (p[0] == 0xe2 && p[1] == 0x80 && p[2] >= 0x80 && p[2] <= 0xbf) {
        result = 0x2000 | (p[2] & 63); *text += 3;
    } else {
        ++*text;
        if (result >= 224) result = -result;
    }
    return result;
}
int DRLanguageHasUtf8(const char* text) {
    const unsigned char* p = (const unsigned char*)text;
    if (!language || !p) return 0;
    while (*p) {
        if ((p[0] == 0xc2 || p[0] == 0xc3) && p[1] >= 0x80 && p[1] <= 0xbf) return 1;
        if (p[0] == 0xe2 && p[1] == 0x80 && p[2] >= 0x80 && p[2] <= 0xbf) return 1;
        ++p;
    }
    return 0;
}
int DRLanguageBase(int c) {
    if (c >= 0xc0 && c <= 0xc5) return 'A';
    if (c >= 0xe0 && c <= 0xe5) return 'a';
    if (c >= 0xc8 && c <= 0xcb) return 'E';
    if (c >= 0xe8 && c <= 0xeb) return 'e';
    if (c >= 0xcc && c <= 0xcf) return 'I';
    if (c >= 0xec && c <= 0xef) return 'i';
    if (c >= 0xd2 && c <= 0xd6) return 'O';
    if (c >= 0xf2 && c <= 0xf6) return 'o';
    if (c >= 0xd9 && c <= 0xdc) return 'U';
    if (c >= 0xf9 && c <= 0xfc) return 'u';
    switch (c) {
    case 0xc7:return 'C'; case 0xe7:return 'c';
    case 0xd1:return 'N'; case 0xf1:return 'n';
    case 0xdd:return 'Y'; case 0xfd:case 0xff:return 'y';
    case 0xa1:return '!'; case 0xbf:return '?';
    case 0x2018:case 0x2019:return '\'';
    case 0x201c:case 0x201d:return '"';
    case 0x2013:case 0x2014:return '-';
    case 0x2026:return '.';
    default:return c >= 32 && c <= 126 ? c : '?';
    }
}

static int glyph(tDR_font* font, int c) {
    int base = DRLanguageBase(c), index = base - font->offset;
    if (index < 0 || index >= font->num_entries || index >= 224) {
        base = toupper((unsigned char)base); index = base - font->offset;
    }
    if (index < 0 || index >= font->num_entries || index >= 224) index = '?' - font->offset;
    return index >= 0 && index < font->num_entries && index < 224 ? index : -1;
}
static tDR_font* change_font(tDR_font* font, int code, int clever) {
    int index = 256 + code;
    return clever && index >= 0 && index < 21 && gFonts[index].images ? &gFonts[index] : font;
}
int DRLanguageTextWidth(tDR_font* font, const char* text, int clever) {
    const unsigned char* p = (const unsigned char*)text;
    int width = 0, last_spacing = 0;
    while (*p) {
        int c = DRLanguageNext(&p);
        if (c < 0) { font = change_font(font, c, clever); continue; }
        int index = glyph(font, c);
        if (index >= 0) { width += font->width_table[index] + font->spacing; last_spacing = font->spacing; }
    }
    return width - last_spacing;
}

static void pixel(br_pixelmap* pm, int x, int y, int colour) {
    if (pm && pm->pixels && pm->type == BR_PMT_INDEX_8 && x >= 0 && y >= 0 && x < pm->width && y < pm->height)
        ((unsigned char*)pm->pixels)[(pm->base_y + y) * pm->row_bytes + pm->base_x + x] = colour;
}
static void accent(br_pixelmap* pm, int x, int y, int w, int h, int c, int colour) {
    int lower = c >= 0xc0 && c <= 0xdf ? c + 32 : c;
    int centre = x + w / 2;
    // Compose diacritics over the original glyph, preserving its palette/style.
    if (lower == 0xe7) { pixel(pm,centre,y+h-2,colour);pixel(pm,centre-1,y+h-1,colour);return; }
    if (lower == 0xe1 || lower == 0xe9 || lower == 0xed || lower == 0xf3 || lower == 0xfa || lower == 0xfd) {
        pixel(pm,centre+1,y,colour);pixel(pm,centre,y+1,colour);
    } else if (lower == 0xe0 || lower == 0xe8 || lower == 0xec || lower == 0xf2 || lower == 0xf9) {
        pixel(pm,centre-1,y,colour);pixel(pm,centre,y+1,colour);
    } else if (lower == 0xe2 || lower == 0xea || lower == 0xee || lower == 0xf4 || lower == 0xfb) {
        pixel(pm,centre,y,colour);pixel(pm,centre-1,y+1,colour);pixel(pm,centre+1,y+1,colour);
    } else if (lower == 0xe4 || lower == 0xeb || lower == 0xef || lower == 0xf6 || lower == 0xfc || lower == 0xff) {
        pixel(pm,centre-1,y,colour);pixel(pm,centre+1,y,colour);
    } else if (lower == 0xf1 || lower == 0xe3 || lower == 0xf5) {
        pixel(pm,centre-2,y+1,colour);pixel(pm,centre-1,y,colour);pixel(pm,centre,y,colour);pixel(pm,centre+1,y+1,colour);
    }
}
void DRLanguageDrawText(br_pixelmap* target, int x, int y, tDR_font* font, const char* text, int right, int clever) {
    const unsigned char* p = (const unsigned char*)text;
    int original_height = font->height;
    while (*p) {
        int c = DRLanguageNext(&p);
        if (c < 0) { font = change_font(font, c, clever); continue; }
        int index = glyph(font, c);
        if (index < 0 || !font->images || !font->images->pixels) continue;
        int w = font->width_table[index], top = y - (font->height - original_height) / 2;
        if (x + w > right) break;
        char base[2] = {(char)(index + font->offset), 0};
        DRPixelmapText(target, x, top, font, base, right);
        int colour = 1;
        for (int row = 0, found = 0; row < font->height && !found; ++row)
            for (int column = 0; column < w; ++column) {
                unsigned char v = ((unsigned char*)font->images->pixels)[(font->images->base_y + index * font->height + row) * font->images->row_bytes + font->images->base_x + column];
                if (v) { colour = v; found = 1; break; }
            }
        accent(target, x, top, w, font->height, c, colour);
        x += w + font->spacing;
    }
}

// Replace only recognised button captions, using their actual hit rectangles.
// Unknown artwork remains original; no user data or FLIC files are rewritten.
static const char* button_key(int flic) {
    switch (flic) {
    case 14:return "Continue";case 25:return "Recover car";case 27:return "Abandon race";
    case 29:case 339:return "Options";case 23:case 333:return "Load game";
    case 21:return "Save game";case 17:return "End game";case 19:case 331:return "Quit";
    case 335:return "New game";case 337:return "Network game";
    case 116:return "Easy";case 119:return "Normal";case 121:return "Hard";
    case 172:case 154:case 323:return "Done";
    case 173:case 45:case 89:return "Cancel";
    case 174:return "Restore defaults";case 175:return "More options";
    case 144:return "Sound";case 145:return "Controls";case 146:return "Graphics";
    case 195:return "Change race";case 200:return "Parts shop";
    case 201:return "Start race";case 202:return "Change car";
    case 88:return "Done";case 44:return "OK";
    case 324:case 326:return "Back";
    default:return NULL;
    }
}
static int br_width(br_pixelmap* pm, const char* text) {
    int width = 0; const unsigned char* p = (const unsigned char*)text;
    while (*p) { int c = DRLanguageNext(&p); if (c < 0) continue; char base[2] = {(char)DRLanguageBase(c),0}; width += BrPixelmapTextWidth(pm,gFont_7,base)+1; }
    return width ? width-1 : 0;
}
void DRLanguageInterface(tInterface_spec* spec, int selection) {
    if (!language || !gBack_screen || !gBack_screen->pixels || !gFont_7) return;
    for (int i = 0; i < spec->number_of_button_flics; ++i) {
        int flic = spec->flicker_on_flics[i].flic_index;
        const char* key = spec->pushed_flics ? button_key(spec->pushed_flics[i].flic_index) : NULL;
        if (!key) key = button_key(flic);
        if (!key) continue;
        for (int a = 0; a < spec->number_of_mouse_areas; ++a) {
            tMouse_area* area = &spec->mouse_areas[a];
            if (area->new_choice != i && !(flic == 337 && a == i)) continue;
            int l=area->left[gGraf_data_index],r=area->right[gGraf_data_index];
            int t=area->top[gGraf_data_index],b=area->bottom[gGraf_data_index];
            if (l<0 || t<0 || r>=gBack_screen->width || b>=gBack_screen->height || r<=l || b<=t) continue;
            const char* text = DRLocalize(key);
            int width = br_width(gBack_screen,text), height = gFont_7->glyph_y;
            int x = l+(r-l-width)/2, y = t+(b-t-height)/2;
            if (width>r-l-2 || height>b-t) continue;
            BrPixelmapRectangleFill(gBack_screen,l,t,r-l+1,b-t+1,0);
            int colour = i == selection ? 2 : 5;
            const unsigned char* p = (const unsigned char*)text;
            while (*p) {
                int c=DRLanguageNext(&p);if(c<0)continue;
                char base[2]={(char)DRLanguageBase(c),0};
                int w=BrPixelmapTextWidth(gBack_screen,gFont_7,base);
                BrPixelmapText(gBack_screen,x,y,colour,gFont_7,base);
                accent(gBack_screen,x,y,w,height,c,colour);x+=w+1;
            }
        }
    }
}

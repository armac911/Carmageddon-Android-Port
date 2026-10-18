#ifndef DR_SCREEN_LAYOUT_H
#define DR_SCREEN_LAYOUT_H

/* Pixel viewport and its inverse pointer transform share one layout. */
typedef struct {
    int window_w, window_h;
    int drawable_w, drawable_h;
    int game_w, game_h;
    int x, y, w, h;
} DRScreenLayout;

static inline int DRScreenLayoutInit(DRScreenLayout* s, int ww, int wh,
    int dw, int dh, int gw, int gh, int stretch) {
    if (ww <= 0 || wh <= 0 || dw <= 0 || dh <= 0 || gw <= 0 || gh <= 0)
        return 0;
    s->window_w = ww; s->window_h = wh;
    s->drawable_w = dw; s->drawable_h = dh;
    s->game_w = gw; s->game_h = gh;
    s->w = dw; s->h = dh;
    if (!stretch) {
        if ((long long)dw * gh > (long long)dh * gw)
            s->w = ((long long)dh * gw + gh / 2) / gh;
        else
            s->h = ((long long)dw * gh + gw / 2) / gw;
    }
    s->x = (dw - s->w) / 2;
    s->y = (dh - s->h) / 2;
    return s->w > 0 && s->h > 0;
}

static inline int DRScreenMapPointer(const DRScreenLayout* s, int wx, int wy,
    int doubled_menu, int* gx, int* gy) {
    if (s->w <= 0 || s->h <= 0) { *gx = *gy = 0; return 0; }
    double dx = (double)wx * s->drawable_w / s->window_w;
    double dy = (double)wy * s->drawable_h / s->window_h;
    double x = (dx - s->x) * s->game_w / s->w;
    double y = (dy - s->y) * s->game_h / s->h;
    int inside = x >= 0 && x < s->game_w && y >= 0 && y < s->game_h;
    int max_x = s->game_w - 1, max_y = s->game_h - 1;
    if (doubled_menu) {
        /* PDScreenBufferSwap centers a doubled 320x200 menu in the
         * selected canvas. Undo that exact blit, including its margins. */
        int left = (s->game_w - 640) / 2;
        int top = (s->game_h - 400) / 2;
        inside = inside && x >= left && x < left + 640 && y >= top && y < top + 400;
        x = (x - left) / 2;
        y = (y - top) / 2;
        max_x = 319; max_y = 199;
    }
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > max_x) x = max_x;
    if (y > max_y) y = max_y;
    *gx = (int)x; *gy = (int)y;
    return inside;
}

#endif

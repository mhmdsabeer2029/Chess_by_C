/*  Chess Pro -- chess.com-style local chess + built-in engine + Coach
 *
 *  Headline features for this revision:
 *    - Resizable 1280x720 window (recomputes geometry every frame)
 *    - No fixed "X to move / 1. e4" side panel -- replaced by a Coach chat
 *      panel (the user explicitly asked for the move list to be removed)
 *    - Native OS Save As / Open file dialogs (file_dialog.c)
 *    - Drag-and-drop or click-to-move
 *    - Right-click to mark / unmark a square (red border)
 *    - Smooth animated piece moves
 *    - Capture particle burst + checkmate confetti
 *    - 4 board themes (cycle via the Theme pill at the top)
 *    - 7 game modes (cycle via the Mode pill at the top): local vs local,
 *      and you-vs-CPU at Easy/Medium/Hard playing either colour
 *    - Hint button (asks the engine for the best move)
 *    - Copy/Paste FEN from the system clipboard
 *    - Sound toggle
 *    - Coach: chat with a real LLM (Groq, llama-3.1-8b-instant by default)
 *      with a local rule-based fallback
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

#include "board.h"
#include "engine.h"
#include "coach.h"
#include "file_dialog.h"

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

/* ==================================================================== */
/* Themes                                                                */
/* ==================================================================== */
typedef struct {
    const char *name;
    SDL_Color light, dark, bg, panel, panel2, button, button_hv, button_red,
              text, dim, lastmove, selected, check, dot, marker, accent;
} Theme;

static const Theme THEMES[] = {
    {"Green",
     {238,238,210,255},{118,150, 86,255},
     { 49, 46, 43,255},{ 38, 36, 33,255},{ 60, 57, 54,255},
     { 90,130, 60,255},{110,160, 80,255},{175, 75, 75,255},
     {235,235,235,255},{180,180,180,255},
     {247,247,105,180},{186,202, 68,255},{235, 97, 80,220},
     { 40, 40, 40,110},{200, 50, 50,200},{ 90,130, 60,255}},
    {"Brown",
     {240,217,181,255},{181,136, 99,255},
     { 49, 36, 33,255},{ 38, 30, 28,255},{ 60, 45, 41,255},
     {140, 90, 50,255},{170,115, 65,255},{175, 75, 75,255},
     {235,225,210,255},{180,170,160,255},
     {247,210, 90,180},{220,180,100,255},{235, 97, 80,220},
     { 40, 40, 40,110},{200, 50, 50,200},{140, 90, 50,255}},
    {"Blue",
     {220,232,242,255},{ 80,124,166,255},
     { 30, 38, 56,255},{ 24, 30, 46,255},{ 40, 50, 70,255},
     { 70,110,160,255},{ 95,140,195,255},{175, 75, 75,255},
     {235,238,245,255},{180,185,200,255},
     {180,220,255,180},{120,180,235,255},{235, 97, 80,220},
     { 40, 40, 40,110},{200, 50, 50,200},{ 70,110,160,255}},
    {"Dark",
     {180,180,180,255},{ 90, 90, 90,255},
     { 18, 18, 22,255},{ 24, 24, 28,255},{ 36, 36, 42,255},
     { 95, 95,110,255},{120,120,140,255},{175, 75, 75,255},
     {235,235,235,255},{170,170,180,255},
     {235,220,120,170},{235,210, 90,255},{235, 97, 80,220},
     { 40, 40, 40,110},{200, 50, 50,200},{ 95, 95,110,255}},
};
#define NTHEMES ((int)(sizeof(THEMES)/sizeof(THEMES[0])))

/* ==================================================================== */
/* Game modes                                                            */
/* ==================================================================== */
typedef enum {
    MODE_LOCAL = 0,
    MODE_EASY_W, MODE_MED_W, MODE_HARD_W,   /* you play White, CPU plays Black */
    MODE_EASY_B, MODE_MED_B, MODE_HARD_B,   /* you play Black, CPU plays White */
    MODE_COUNT
} GameMode;
static const char *MODE_LABELS[MODE_COUNT] = {
    "Local 2P",
    "vs CPU (Easy, you White)",
    "vs CPU (Medium, you White)",
    "vs CPU (Hard, you White)",
    "vs CPU (Easy, you Black)",
    "vs CPU (Medium, you Black)",
    "vs CPU (Hard, you Black)",
};
static const char *MODE_SHORT[MODE_COUNT] = {
    "Local", "CPU Easy / W", "CPU Med / W", "CPU Hard / W",
    "CPU Easy / B", "CPU Med / B", "CPU Hard / B",
};
static int mode_has_cpu(GameMode m) { return m != MODE_LOCAL; }
static EngineLevel mode_level(GameMode m) {
    switch (m) {
        case MODE_EASY_W: case MODE_EASY_B: return LEVEL_EASY;
        case MODE_MED_W:  case MODE_MED_B:  return LEVEL_MEDIUM;
        case MODE_HARD_W: case MODE_HARD_B: return LEVEL_HARD;
        default: return LEVEL_EASY;
    }
}
static Color mode_human_side(GameMode m) {
    switch (m) {
        case MODE_EASY_W: case MODE_MED_W: case MODE_HARD_W: return WHITE;
        case MODE_EASY_B: case MODE_MED_B: case MODE_HARD_B: return BLACK;
        default: return WHITE;
    }
}

/* ==================================================================== */
/* Layout (recomputed on every resize)                                   */
/* ==================================================================== */
typedef struct {
    int win_w, win_h;
    int top_bar_h, bottom_bar_h;
    int board_x, board_y, square_size, board_size;
    int chat_x, chat_y, chat_w, chat_h;
    int chat_input_y, chat_input_h;
} Layout;

/* `chat_open` controls whether the Coach panel is shown. When closed, the
 * board grows to fill the freed horizontal space. A toggle button is rendered
 * along the right edge in both states. */
static int g_toggle_btn_w = 22;

static Layout compute_layout(int w, int h, int chat_open) {
    Layout L;
    L.win_w = w; L.win_h = h;
    L.top_bar_h    = 44;
    L.bottom_bar_h = 56;
    int side_pad   = 16;

    int chat_w_target = 0;
    if (chat_open) {
        chat_w_target = 360;
        if (w < 1000) chat_w_target = 280;
        if (w < 800)  chat_w_target = 240;
    }

    int usable_h = h - L.top_bar_h - L.bottom_bar_h - 2 * side_pad - 56; /* captured strips */
    int max_board_h = usable_h;
    int max_board_w = chat_open
                      ? (w - 3 * side_pad - chat_w_target - g_toggle_btn_w - 4)
                      : (w - 2 * side_pad - g_toggle_btn_w - 4);
    int board_size = max_board_h < max_board_w ? max_board_h : max_board_w;
    if (board_size < 320) board_size = 320;
    L.square_size = board_size / 8;
    L.board_size  = L.square_size * 8;
    /* Center the board in the area available to it. */
    int board_area_w = chat_open
                       ? (w - chat_w_target - g_toggle_btn_w - 4 - 2 * side_pad)
                       : (w - g_toggle_btn_w - 4 - 2 * side_pad);
    L.board_x = side_pad + (board_area_w - L.board_size) / 2;
    if (L.board_x < side_pad) L.board_x = side_pad;
    L.board_y = L.top_bar_h + side_pad + 28; /* captured strip on top */

    if (chat_open) {
        L.chat_x = w - side_pad - chat_w_target;
        L.chat_w = chat_w_target;
    } else {
        L.chat_x = w; L.chat_w = 0;
    }
    L.chat_y = L.top_bar_h + side_pad;
    L.chat_h = h - L.top_bar_h - L.bottom_bar_h - 2 * side_pad;
    L.chat_input_h = 36;
    L.chat_input_y = L.chat_y + L.chat_h - L.chat_input_h;
    return L;
}

/* ==================================================================== */
/* Render helpers                                                        */
/* ==================================================================== */
static void set_color(SDL_Renderer *ren, SDL_Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
}
static void fill_rect_c(SDL_Renderer *ren, SDL_Rect r, SDL_Color c) {
    set_color(ren, c);
    SDL_RenderFillRect(ren, &r);
}
static void draw_rect_c(SDL_Renderer *ren, SDL_Rect r, SDL_Color c) {
    set_color(ren, c);
    SDL_RenderDrawRect(ren, &r);
}

static void fill_round_rect(SDL_Renderer *ren, SDL_Rect r, int radius, SDL_Color c) {
    if (radius * 2 > r.w) radius = r.w / 2;
    if (radius * 2 > r.h) radius = r.h / 2;
    SDL_Rect mid = {r.x, r.y + radius, r.w, r.h - 2*radius};
    SDL_Rect left = {r.x, r.y + radius, radius, r.h - 2*radius};
    SDL_Rect right = {r.x + r.w - radius, r.y + radius, radius, r.h - 2*radius};
    (void)left; (void)right;
    fill_rect_c(ren, mid, c);
    SDL_Rect top = {r.x + radius, r.y, r.w - 2*radius, radius};
    SDL_Rect bot = {r.x + radius, r.y + r.h - radius, r.w - 2*radius, radius};
    fill_rect_c(ren, top, c);
    fill_rect_c(ren, bot, c);
    /* round corners */
    set_color(ren, c);
    for (int dy = 0; dy <= radius; dy++) {
        for (int dx = 0; dx <= radius; dx++) {
            int d = dx*dx + dy*dy;
            if (d <= radius * radius) {
                SDL_RenderDrawPoint(ren, r.x + radius - dx, r.y + radius - dy);
                SDL_RenderDrawPoint(ren, r.x + r.w - radius - 1 + dx, r.y + radius - dy);
                SDL_RenderDrawPoint(ren, r.x + radius - dx, r.y + r.h - radius - 1 + dy);
                SDL_RenderDrawPoint(ren, r.x + r.w - radius - 1 + dx, r.y + r.h - radius - 1 + dy);
            }
        }
    }
}

static void draw_text(SDL_Renderer *ren, TTF_Font *font, const char *t,
                      SDL_Color c, int x, int y) {
    if (!t || !*t) return;
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, t, c);
    if (!s) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, s);
    SDL_Rect d = {x, y, s->w, s->h};
    SDL_RenderCopy(ren, tex, NULL, &d);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(tex);
}
static void draw_text_centered(SDL_Renderer *ren, TTF_Font *font, const char *t,
                               SDL_Color c, SDL_Rect r) {
    if (!t || !*t) return;
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, t, c);
    if (!s) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, s);
    SDL_Rect d = {r.x + (r.w - s->w)/2, r.y + (r.h - s->h)/2, s->w, s->h};
    SDL_RenderCopy(ren, tex, NULL, &d);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(tex);
}

static void fill_circle_c(SDL_Renderer *ren, int cx, int cy, int r, SDL_Color c) {
    set_color(ren, c);
    for (int dy = -r; dy <= r; dy++) {
        int dxmax = (int)sqrtf((float)(r*r - dy*dy));
        SDL_RenderDrawLine(ren, cx - dxmax, cy + dy, cx + dxmax, cy + dy);
    }
}
static void draw_ring(SDL_Renderer *ren, int cx, int cy, int r, int thickness, SDL_Color c) {
    set_color(ren, c);
    int inner = r - thickness;
    for (int dy = -r; dy <= r; dy++) {
        int outer_dx = (int)sqrtf((float)(r*r - dy*dy));
        int inner_dx = 0;
        int inner_sq = inner*inner - dy*dy;
        if (inner_sq > 0) inner_dx = (int)sqrtf((float)inner_sq);
        if (inner_dx <= 0) {
            SDL_RenderDrawLine(ren, cx - outer_dx, cy + dy, cx + outer_dx, cy + dy);
        } else {
            SDL_RenderDrawLine(ren, cx - outer_dx, cy + dy, cx - inner_dx, cy + dy);
            SDL_RenderDrawLine(ren, cx + inner_dx, cy + dy, cx + outer_dx, cy + dy);
        }
    }
}

/* Thick line from (x1,y1) to (x2,y2). */
static void thick_line(SDL_Renderer *ren, int x1, int y1, int x2, int y2,
                       int thickness, SDL_Color c) {
    set_color(ren, c);
    int dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf((float)(dx*dx + dy*dy));
    if (len < 1) return;
    float nx = -dy / len, ny = dx / len;
    for (int t = -thickness/2; t <= thickness/2; t++) {
        SDL_RenderDrawLine(ren,
                           x1 + (int)(t*nx), y1 + (int)(t*ny),
                           x2 + (int)(t*nx), y2 + (int)(t*ny));
    }
}

/* Arrow from (x1,y1) to (x2,y2). */
static void draw_arrow(SDL_Renderer *ren, int x1, int y1, int x2, int y2,
                       SDL_Color c, int thickness) {
    thick_line(ren, x1, y1, x2, y2, thickness, c);
    float dx = (float)(x2 - x1), dy = (float)(y2 - y1);
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 1) return;
    float ux = dx / len, uy = dy / len;
    int head = thickness * 3;
    float a1x = -ux * head - uy * head * 0.6f;
    float a1y = -uy * head + ux * head * 0.6f;
    float a2x = -ux * head + uy * head * 0.6f;
    float a2y = -uy * head - ux * head * 0.6f;
    thick_line(ren, x2, y2, x2 + (int)a1x, y2 + (int)a1y, thickness, c);
    thick_line(ren, x2, y2, x2 + (int)a2x, y2 + (int)a2y, thickness, c);
}

/* ==================================================================== */
/* Square <-> screen translation                                         */
/* ==================================================================== */
static SDL_Rect square_screen(Layout *L, int row, int col, int flipped) {
    int dr = flipped ? (7 - row) : row;
    int dc = flipped ? (7 - col) : col;
    SDL_Rect r = {L->board_x + dc * L->square_size,
                  L->board_y + dr * L->square_size,
                  L->square_size, L->square_size};
    return r;
}
static int screen_to_sq(Layout *L, int x, int y, int flipped, int *row, int *col) {
    if (x < L->board_x || x >= L->board_x + L->board_size) return 0;
    if (y < L->board_y || y >= L->board_y + L->board_size) return 0;
    int dc = (x - L->board_x) / L->square_size;
    int dr = (y - L->board_y) / L->square_size;
    *col = flipped ? (7 - dc) : dc;
    *row = flipped ? (7 - dr) : dr;
    return 1;
}

/* ==================================================================== */
/* Move log -- SAN diff between two boards                               */
/* ==================================================================== */
typedef struct { char text[16]; } MoveSAN;

static const char *piece_letter(Type t) {
    switch (t) {
        case KNIGHT: return "N"; case BISHOP: return "B";
        case ROOK:   return "R"; case QUEEN:  return "Q";
        case KING:   return "K"; default:     return "";
    }
}
static void square_name(int row, int col, char *out) {
    out[0] = (char)('a' + col);
    out[1] = (char)('8' - row);
    out[2] = '\0';
}

static void compute_san(Board *prev, Board *curr, MoveSAN *out) {
    int from_r=-1,from_c=-1,to_r=-1,to_c=-1;
    int second_from_r=-1;
    Type moved_piece = PAWN; Color moved_color = WHITE;
    int captured=0, promotion=0; Type promoted_to=PAWN;
    int en_passant=0;
    for (int r=0;r<8;r++) for (int c=0;c<8;c++) {
        Piece pp=prev->board_places[r][c], pc=curr->board_places[r][c];
        if (pp.in_game && !pc.in_game) {
            if (from_r<0) { from_r=r; from_c=c; moved_piece=pp.piece_type; moved_color=pp.color; }
            else { second_from_r=r; }
        } else if (!pp.in_game && pc.in_game) {
            if (to_r<0) { to_r=r; to_c=c; }
        } else if (pp.in_game && pc.in_game) {
            if (pp.piece_type!=pc.piece_type || pp.color!=pc.color) {
                if (pp.color!=pc.color) { to_r=r; to_c=c; captured=1; }
                else if (pp.piece_type==PAWN && pc.piece_type!=PAWN) {
                    promotion=1; promoted_to=pc.piece_type;
                }
            }
        }
    }
    if (from_r>=0 && to_r>=0) {
        Piece origin = prev->board_places[from_r][from_c];
        Piece arrived= curr->board_places[to_r][to_c];
        moved_piece=origin.piece_type; moved_color=origin.color;
        if (origin.piece_type==PAWN && arrived.in_game && arrived.piece_type!=PAWN) {
            promotion=1; promoted_to=arrived.piece_type;
        }
        if (origin.piece_type==PAWN && from_c!=to_c &&
            !prev->board_places[to_r][to_c].in_game) {
            en_passant=1; captured=1;
        }
        if (prev->board_places[to_r][to_c].in_game &&
            prev->board_places[to_r][to_c].color!=origin.color) {
            captured=1;
        }
    }
    int is_castle=0,castle_side=0;
    if (moved_piece==KING && from_r>=0 && to_r>=0 && from_r==to_r && second_from_r>=0) {
        int d = to_c - from_c;
        if (d==2) {is_castle=1;castle_side=1;}
        if (d==-2){is_castle=1;castle_side=2;}
    }
    char buf[32];
    if (is_castle) strcpy(buf, castle_side==1?"O-O":"O-O-O");
    else {
        char dest[3]={'-','-','\0'};
        if (to_r>=0) square_name(to_r,to_c,dest);
        const char *plet=piece_letter(moved_piece);
        char src_file[2]={'\0','\0'};
        if (moved_piece==PAWN && captured) src_file[0]=(char)('a' + (from_c>=0?from_c:0));
        char promo[8]="";
        if (promotion) snprintf(promo,sizeof(promo),"=%s",piece_letter(promoted_to));
        snprintf(buf,sizeof(buf),"%s%s%s%s%s",plet,src_file,captured?"x":"",dest,promo);
    }
    (void)en_passant;
    /* Check / mate suffix. */
    Color opp = (moved_color==WHITE)?BLACK:WHITE;
    int in_check = curr->players[opp].is_in_check;
    int mate = 0;
    if (in_check) {
        int legal=0;
        for (int r=0;r<8 && !legal;r++) for (int c=0;c<8 && !legal;c++) {
            Piece p=curr->board_places[r][c];
            if (!p.in_game || p.color!=opp) continue;
            MoveList ml = get_possible_moves(curr,r,c);
            for (int k=0;k<ml.count && !legal;k++) {
                int tr=ml.moves[k].row, tc=ml.moves[k].col;
                if (tr<0||tr>=8||tc<0||tc>=8) continue;
                if (!is_it_llegal_move(r,c,tr,tc,curr)) { legal=1; break; }
            }
        }
        if (!legal) mate=1;
    }
    size_t bl=strlen(buf);
    if (mate)     { if (bl+1<sizeof(buf)) { buf[bl]='#'; buf[bl+1]='\0'; } }
    else if (in_check) { if (bl+1<sizeof(buf)) { buf[bl]='+'; buf[bl+1]='\0'; } }
    snprintf(out->text,sizeof(out->text),"%s",buf);
}

/* ==================================================================== */
/* Particles (capture burst) + Confetti (checkmate)                      */
/* ==================================================================== */
typedef struct { float x,y,vx,vy; Uint32 born; int alive; SDL_Color c; } Particle;
#define MAX_PARTICLES 120
static Particle parts[MAX_PARTICLES];
static void spawn_particles(int cx, int cy, SDL_Color c) {
    int n=18;
    for (int i=0;i<MAX_PARTICLES && n>0;i++) if (!parts[i].alive) {
        Particle *p=&parts[i];
        float ang = (float)(rand()%628) / 100.0f;
        float spd = 0.06f + (rand()%50)/600.0f;
        p->x=(float)cx; p->y=(float)cy;
        p->vx=cosf(ang)*spd; p->vy=sinf(ang)*spd - 0.04f;
        p->born=SDL_GetTicks(); p->alive=1; p->c=c;
        n--;
    }
}
static void render_particles(SDL_Renderer *ren) {
    Uint32 now = SDL_GetTicks();
    for (int i=0;i<MAX_PARTICLES;i++) if (parts[i].alive) {
        Uint32 age = now - parts[i].born;
        if (age > 700) { parts[i].alive=0; continue; }
        parts[i].x += parts[i].vx * 16;
        parts[i].y += parts[i].vy * 16;
        parts[i].vy += 0.005f;
        int a = 255 - (int)(age * 255 / 700);
        if (a<0) a=0;
        SDL_Color c = parts[i].c; c.a=(Uint8)a;
        fill_circle_c(ren, (int)parts[i].x, (int)parts[i].y, 4, c);
    }
}

typedef struct { float x,y,vx,vy,rot,vrot; Uint32 born; int alive; SDL_Color c; } Confetto;
#define MAX_CONFETTI 140
static Confetto conf[MAX_CONFETTI];
static int confetti_active = 0;
static void start_confetti(int x_left, int y_top, int w) {
    SDL_Color palette[6] = {
        {255, 80, 80,255},{255,180, 60,255},{ 90,200, 90,255},
        { 60,180,255,255},{220,120,255,255},{255,240,120,255},
    };
    confetti_active = SDL_GetTicks();
    int n=0;
    for (int i=0;i<MAX_CONFETTI;i++) {
        conf[i].alive=1;
        conf[i].x = (float)(x_left + rand()%w);
        conf[i].y = (float)(y_top + (rand()%30) - 60);
        conf[i].vx= ((rand()%200)-100) / 80.0f;
        conf[i].vy= 0.5f + (rand()%80)/200.0f;
        conf[i].rot=(float)(rand()%360);
        conf[i].vrot=((rand()%20)-10)*0.4f;
        conf[i].born=SDL_GetTicks();
        conf[i].c = palette[rand()%6];
        n++;
    }
}
static void render_confetti(SDL_Renderer *ren) {
    if (!confetti_active) return;
    Uint32 now = SDL_GetTicks();
    int any_alive = 0;
    for (int i=0;i<MAX_CONFETTI;i++) if (conf[i].alive) {
        Uint32 age = now - conf[i].born;
        if (age > 6000) { conf[i].alive=0; continue; }
        any_alive=1;
        conf[i].x += conf[i].vx * 2.5f;
        conf[i].y += conf[i].vy * 2.5f;
        conf[i].vy += 0.04f;
        conf[i].rot += conf[i].vrot;
        SDL_Rect r = {(int)conf[i].x - 4, (int)conf[i].y - 4, 8, 12};
        fill_rect_c(ren, r, conf[i].c);
    }
    if (!any_alive) confetti_active = 0;
}

/* ==================================================================== */
/* AI worker thread                                                      */
/* ==================================================================== */
typedef struct {
    Board pos;
    Color side;
    EngineLevel level;
    EngineMove result;
    volatile int done;   /* 1 = result ready */
    volatile int busy;   /* 1 = thread running */
} AIJob;
static AIJob g_ai;

#ifdef _WIN32
static unsigned __stdcall ai_worker(void *arg) {
    (void)arg;
    g_ai.result = engine_best_move(&g_ai.pos, g_ai.side, g_ai.level);
    g_ai.done = 1;
    g_ai.busy = 0;
    return 0;
}
#else
static void *ai_worker(void *arg) {
    (void)arg;
    g_ai.result = engine_best_move(&g_ai.pos, g_ai.side, g_ai.level);
    g_ai.done = 1;
    g_ai.busy = 0;
    return NULL;
}
#endif

static void ai_kick(Board *pos, Color side, EngineLevel level) {
    if (g_ai.busy) return;
    g_ai.pos = *pos;
    g_ai.side = side;
    g_ai.level = level;
    g_ai.done = 0;
    g_ai.busy = 1;
#ifdef _WIN32
    HANDLE h = (HANDLE)_beginthreadex(NULL, 0, ai_worker, NULL, 0, NULL);
    if (h) CloseHandle(h);
#else
    pthread_t th;
    if (pthread_create(&th, NULL, ai_worker, NULL) == 0) pthread_detach(th);
    else { g_ai.busy = 0; }
#endif
}

/* ==================================================================== */
/* Captured pieces / material count                                      */
/* ==================================================================== */
static int piece_value_for_material(Type t) {
    switch (t) { case PAWN:return 1; case KNIGHT:case BISHOP:return 3;
        case ROOK:return 5; case QUEEN:return 9; default:return 0; }
}

/* Render the captured pieces strip for one side. */
static void render_captured(SDL_Renderer *ren, TTF_Font *fnt,
                            Board *b, Color side_we_show_captures_from,
                            SDL_Texture *piece_tex[2][6],
                            int x, int y, int w, int h, SDL_Color text_col) {
    /* Count captures by `side_we_show_captures_from`: pieces of opposite color. */
    int counts[6] = {0};
    int my_val = 0, opp_val = 0;
    Color me = side_we_show_captures_from;
    Color opp = (me == WHITE) ? BLACK : WHITE;
    for (int r=0;r<8;r++) for (int c=0;c<8;c++) {
        Piece p=b->board_places[r][c];
        if (!p.in_game) continue;
        int v = piece_value_for_material(p.piece_type);
        if (p.color == me) my_val += v;
        else                opp_val+= v;
    }
    /* Count by inspecting players[me].captured_piece[]. */
    for (int i=0;i<b->players[me].total_captured && i<16;i++) {
        Type t = b->players[me].captured_piece[i].piece_type;
        if (t >= 0 && t < 6) counts[t]++;
    }
    int icon = h - 6;
    int cx = x;
    for (int t = PAWN; t <= QUEEN; t++) {
        for (int k = 0; k < counts[t]; k++) {
            SDL_Rect d = {cx, y + (h - icon)/2, icon, icon};
            SDL_RenderCopy(ren, piece_tex[opp][t], NULL, &d);
            cx += icon * 3 / 5;
        }
        if (counts[t] > 0) cx += 4;
    }
    int adv = my_val - opp_val;
    if (adv > 0) {
        char buf[16]; snprintf(buf, sizeof(buf), "+%d", adv);
        draw_text(ren, fnt, buf, text_col, cx + 6, y + 4);
    }
    (void)w;
}

/* ==================================================================== */
/* Promotion popup                                                       */
/* ==================================================================== */
typedef struct { SDL_Rect rect; Type t; } PromOpt;
static void compute_prom_rects(Layout *L, int row, int col, Color color,
                               int flipped, PromOpt opts[4]) {
    SDL_Rect base = square_screen(L, row, col, flipped);
    int dir = (color == WHITE) ? +1 : -1;
    if (flipped) dir = -dir;
    Type list[4] = {QUEEN, KNIGHT, ROOK, BISHOP};
    for (int i = 0; i < 4; i++) {
        opts[i].rect = (SDL_Rect){base.x, base.y + dir * i * L->square_size,
                                  L->square_size, L->square_size};
        opts[i].t = list[i];
    }
}

/* ==================================================================== */
/* main                                                                  */
/* ==================================================================== */
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    srand((unsigned)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "Chess Pro -- by Mohamed & Fares",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit(); return 1;
    }
    SDL_SetWindowMinimumSize(win, 800, 560);

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        SDL_DestroyWindow(win);
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_Quit(); return 1;
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    if (TTF_Init() == -1) {
        fprintf(stderr, "TTF_Init Error: %s\n", TTF_GetError());
        return 1;
    }
    TTF_Font *f_title = TTF_OpenFont("assets/arial.ttf", 24);
    TTF_Font *f_ui    = TTF_OpenFont("assets/arial.ttf", 16);
    TTF_Font *f_small = TTF_OpenFont("assets/arial.ttf", 13);
    if (!f_title || !f_ui || !f_small) {
        fprintf(stderr, "TTF_OpenFont Error: %s\n", TTF_GetError());
        return 1;
    }

    int audio_ready = 0;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0 &&
        Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) >= 0) {
        Mix_AllocateChannels(16);
        audio_ready = 1;
    } else {
        fprintf(stderr, "Audio disabled (%s). Game continues without sound.\n",
                Mix_GetError());
    }
    Mix_Chunk *sound[10] = {0};
    if (audio_ready) {
        sound[0]=Mix_LoadWAV("./assets/move-self.wav");
        sound[1]=Mix_LoadWAV("./assets/capture.wav");
        sound[2]=Mix_LoadWAV("./assets/move-check.wav");
        sound[3]=Mix_LoadWAV("./assets/castle.wav");
        sound[4]=Mix_LoadWAV("./assets/promote.wav");
        sound[5]=Mix_LoadWAV("./assets/game-start.wav");
        sound[6]=Mix_LoadWAV("./assets/game-end.wav");
        sound[7]=Mix_LoadWAV("./assets/illegal.wav");
    }

    SDL_Texture *piece_tex[2][6] = {{0}};
    piece_tex[WHITE][PAWN]   = IMG_LoadTexture(ren, "./assets/pawn_white.png");
    piece_tex[WHITE][ROOK]   = IMG_LoadTexture(ren, "./assets/rook_white.png");
    piece_tex[WHITE][KNIGHT] = IMG_LoadTexture(ren, "./assets/knight_white.png");
    piece_tex[WHITE][BISHOP] = IMG_LoadTexture(ren, "./assets/bishop_white.png");
    piece_tex[WHITE][QUEEN]  = IMG_LoadTexture(ren, "./assets/queen_white.png");
    piece_tex[WHITE][KING]   = IMG_LoadTexture(ren, "./assets/king_white.png");
    piece_tex[BLACK][PAWN]   = IMG_LoadTexture(ren, "./assets/pawn_black.png");
    piece_tex[BLACK][ROOK]   = IMG_LoadTexture(ren, "./assets/rook_black.png");
    piece_tex[BLACK][KNIGHT] = IMG_LoadTexture(ren, "./assets/knight_black.png");
    piece_tex[BLACK][BISHOP] = IMG_LoadTexture(ren, "./assets/bishop_black.png");
    piece_tex[BLACK][QUEEN]  = IMG_LoadTexture(ren, "./assets/queen_black.png");
    piece_tex[BLACK][KING]   = IMG_LoadTexture(ren, "./assets/king_black.png");

    Board *board = (Board*)calloc(MAX_BOARDS, sizeof(Board));
    if (!board) { fprintf(stderr, "calloc(board) failed\n"); return 1; }
    MoveSAN *log = (MoveSAN*)calloc(MAX_BOARDS, sizeof(MoveSAN));
    if (!log) { free(board); return 1; }

    init_board(&board[0]);

    int move_count = 0, latest_move = 0;
    int game_over = 0;
    int played_start_sound = 0;
    int status_idx = 0;
    int board_flipped = 0;
    int sound_on = 1;
    int theme_id = 0;
    int chat_open = 1;       /* Coach panel visible by default */
    GameMode mode = MODE_LOCAL;

    /* Selection / interaction */
    int selected = 0, sel_r = 0, sel_c = 0;
    MoveList sel_moves = {.count = 0};

    /* Right-click marks */
    int marked[8][8] = {{0}};

    /* Drag state */
    int dragging = 0;
    int drag_from_r = -1, drag_from_c = -1;
    int drag_mx = 0, drag_my = 0;
    int drag_started_at_x = 0, drag_started_at_y = 0;

    /* Promotion */
    int prom_active = 0;
    int prom_r = -1, prom_c = -1;
    Color prom_color = WHITE;

    /* Smooth animation */
    int anim_active = 0;
    Uint32 anim_start = 0;
    int anim_from_r=0, anim_from_c=0, anim_to_r=0, anim_to_c=0;
    Type anim_type = PAWN;
    Color anim_color = WHITE;

    /* Hint arrow */
    int hint_active = 0;
    int hint_from_r=0, hint_from_c=0, hint_to_r=0, hint_to_c=0;
    Uint32 hint_until = 0;

    /* Coach */
    CoachState coach;
    coach_init(&coach);

    /* Chat input */
    char chat_buf[200] = "";
    int chat_focus = 0;
    SDL_StartTextInput();

    /* Toolbar layout (computed each frame from layout) */
    typedef struct { const char *label; int id; SDL_Color c; SDL_Rect r; } Btn;

    static const char *status_strings[] = {
        "",                                 /* 0 */
        "White wins by checkmate",          /* 1 */
        "Black wins by checkmate",          /* 2 */
        "Draw by stalemate",                /* 3 */
        "Draw by insufficient material",    /* 4 */
        "Draw by threefold repetition",     /* 5 */
        "Draw by 50-move rule",             /* 6 */
        "Game in progress",                 /* 7 */
        "",                                 /* 8 */
        "",                                 /* 9 */
        "",                                 /* 10 */
        "",                                 /* 11 */
        "",                                 /* 12 */
        "Choose a promotion piece",         /* 13 */
        "",                                 /* 14 */
        "",                                 /* 15 */
        "Black wins by resignation",        /* 16 */
        "White wins by resignation",        /* 17 */
        "Draw by agreement",                /* 18 */
    };

    enum {
        BTN_NEW=1, BTN_OPEN, BTN_SAVE, BTN_UNDO, BTN_REDO,
        BTN_HINT, BTN_FLIP, BTN_FENCOPY, BTN_FENPASTE,
        BTN_DRAW, BTN_RESIGN
    };

    SDL_Event ev;
    int running = 1;

    while (running) {
        int ww, wh;
        SDL_GetWindowSize(win, &ww, &wh);
        Layout L = compute_layout(ww, wh, chat_open);

        const Theme *T = &THEMES[theme_id];

        /* Build pill rects on the top bar */
        SDL_Rect pill_mode  = {L.win_w - 16 - 220, 8, 220, 28};
        SDL_Rect pill_theme = {pill_mode.x - 16 - 110, 8, 110, 28};
        SDL_Rect pill_sound = {pill_theme.x - 12 - 78, 8, 78, 28};

        /* Build toolbar buttons */
        Btn buttons[] = {
            {"New",        BTN_NEW,       T->button,     {0}},
            {"Open...",    BTN_OPEN,      T->button,     {0}},
            {"Save As...", BTN_SAVE,      T->button,     {0}},
            {"Undo",       BTN_UNDO,      T->button,     {0}},
            {"Redo",       BTN_REDO,      T->button,     {0}},
            {"Hint",       BTN_HINT,      T->accent,     {0}},
            {"Flip",       BTN_FLIP,      T->button,     {0}},
            {"Copy FEN",   BTN_FENCOPY,   T->button,     {0}},
            {"Paste FEN",  BTN_FENPASTE,  T->button,     {0}},
            {"Draw",       BTN_DRAW,      T->button,     {0}},
            {"Resign",     BTN_RESIGN,    T->button_red, {0}},
        };
        int nbtn = sizeof(buttons)/sizeof(buttons[0]);
        int btn_y = L.win_h - L.bottom_bar_h + 10;
        int btn_h = 36;
        int btn_total_w = L.win_w - 32;
        int btn_w = (btn_total_w - (nbtn-1)*6) / nbtn;
        if (btn_w > 110) btn_w = 110;
        int total_w = nbtn * btn_w + (nbtn-1)*6;
        int btn_x0 = (L.win_w - total_w) / 2;
        for (int i = 0; i < nbtn; i++) {
            buttons[i].r = (SDL_Rect){btn_x0 + i*(btn_w+6), btn_y, btn_w, btn_h};
        }

        /* ---------------- Events ---------------- */
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            else if (ev.type == SDL_WINDOWEVENT) {
                /* Layout will recompute next frame. */
            }
            else if (ev.type == SDL_TEXTINPUT && chat_focus) {
                size_t cur = strlen(chat_buf);
                size_t add = strlen(ev.text.text);
                if (cur + add < sizeof(chat_buf) - 1) {
                    memcpy(chat_buf + cur, ev.text.text, add + 1);
                }
            }
            else if (ev.type == SDL_KEYDOWN) {
                SDL_Keycode k = ev.key.keysym.sym;
                if (chat_focus) {
                    if (k == SDLK_BACKSPACE) {
                        size_t n = strlen(chat_buf);
                        if (n) chat_buf[n-1] = '\0';
                    } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
                        if (chat_buf[0]) {
                            Color stm = (move_count % 2 == 0) ? WHITE : BLACK;
                            const char *last_san =
                                move_count > 0 ? log[move_count - 1].text : "";
                            coach_ask(&coach, chat_buf, &board[move_count],
                                      last_san, stm);
                            chat_buf[0] = '\0';
                        }
                    } else if (k == SDLK_ESCAPE) {
                        chat_focus = 0;
                    }
                    continue;
                }
                if (prom_active) {
                    Type promoted = PAWN; int valid = 0;
                    switch (k) {
                        case SDLK_q: promoted=QUEEN;  valid=1; break;
                        case SDLK_r: promoted=ROOK;   valid=1; break;
                        case SDLK_b: promoted=BISHOP; valid=1; break;
                        case SDLK_n: promoted=KNIGHT; valid=1; break;
                        default: break;
                    }
                    if (valid) {
                        promote_pawn(&board[move_count], prom_r, prom_c, promoted);
                        if (move_count > 0)
                            compute_san(&board[move_count-1], &board[move_count],
                                        &log[move_count-1]);
                        prom_active = 0;
                        if (sound_on && audio_ready && sound[4])
                            Mix_PlayChannel(-1, sound[4], 0);
                    }
                    continue;
                }
                if (k == SDLK_ESCAPE) { selected = 0; sel_moves.count = 0; }
                if (k == SDLK_z && (SDL_GetModState() & KMOD_CTRL) && move_count > 0) {
                    move_count--;
                    selected = 0; sel_moves.count = 0; game_over = 0;
                }
                if (k == SDLK_y && (SDL_GetModState() & KMOD_CTRL) && move_count < latest_move) {
                    move_count++;
                    selected = 0; sel_moves.count = 0;
                }
            }
            else if (ev.type == SDL_MOUSEBUTTONDOWN) {
                int mx = ev.button.x, my = ev.button.y;
                /* --- Pills (top bar) --- */
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    SDL_Point pt = {mx, my};
                    /* Coach panel toggle button -- always rendered against the right edge. */
                    SDL_Rect toggle_btn = {L.win_w - g_toggle_btn_w - 4,
                                           L.top_bar_h + L.chat_h / 2 - 32,
                                           g_toggle_btn_w, 64};
                    if (SDL_PointInRect(&pt, &toggle_btn)) {
                        chat_open = !chat_open;
                        chat_focus = 0;
                        continue;
                    }
                    if (SDL_PointInRect(&pt, &pill_mode)) {
                        mode = (GameMode)((mode + 1) % MODE_COUNT);
                        coach_announce(&coach, MODE_LABELS[mode]);
                        continue;
                    }
                    if (SDL_PointInRect(&pt, &pill_theme)) {
                        theme_id = (theme_id + 1) % NTHEMES; continue;
                    }
                    if (SDL_PointInRect(&pt, &pill_sound)) {
                        sound_on = !sound_on; continue;
                    }
                    /* --- Chat input focus --- */
                    SDL_Rect chat_input = {L.chat_x + 8, L.chat_input_y + 4,
                                           L.chat_w - 80, L.chat_input_h - 8};
                    SDL_Rect send_btn = {chat_input.x + chat_input.w + 6,
                                         L.chat_input_y + 4, 64, L.chat_input_h - 8};
                    if (L.chat_w > 0 && SDL_PointInRect(&pt, &chat_input)) {
                        chat_focus = 1; continue;
                    }
                    if (L.chat_w > 0 && SDL_PointInRect(&pt, &send_btn)) {
                        if (chat_buf[0]) {
                            Color stm = (move_count % 2 == 0) ? WHITE : BLACK;
                            const char *last_san = move_count > 0 ? log[move_count-1].text : "";
                            coach_ask(&coach, chat_buf, &board[move_count], last_san, stm);
                            chat_buf[0] = '\0';
                        }
                        continue;
                    }
                    /* --- Toolbar --- */
                    int handled = 0;
                    for (int i = 0; i < nbtn; i++) {
                        if (SDL_PointInRect(&pt, &buttons[i].r)) {
                            handled = 1;
                            switch (buttons[i].id) {
                                case BTN_NEW:
                                    init_board(&board[0]);
                                    move_count = 0; latest_move = 0;
                                    game_over = 0; status_idx = 0;
                                    selected = 0; sel_moves.count = 0;
                                    prom_active = 0;
                                    memset(marked, 0, sizeof(marked));
                                    if (sound_on && audio_ready && sound[5])
                                        Mix_PlayChannel(-1, sound[5], 0);
                                    coach_announce(&coach, "New game.");
                                    confetti_active = 0;
                                    break;
                                case BTN_OPEN: {
                                    char path[1024] = "";
                                    if (file_dialog_open("Open Chess Game", path, sizeof(path))) {
                                        FILE *f = fopen(path, "r");
                                        if (f) {
                                            char fen[256] = "";
                                            if (fgets(fen, sizeof(fen), f)) {
                                                size_t n = strlen(fen);
                                                while (n>0 && (fen[n-1]=='\n'||fen[n-1]=='\r')) fen[--n]='\0';
                                                if (is_valid_fen(fen)) {
                                                    init_board(&board[0]);
                                                    fen_to_board(&board[0], fen);
                                                    move_count = 0; latest_move = 0;
                                                    game_over = 0; status_idx = 0;
                                                    selected = 0; sel_moves.count = 0;
                                                    coach_announce(&coach, "Position loaded.");
                                                } else {
                                                    coach_announce(&coach, "That file isn't a valid FEN.");
                                                }
                                            }
                                            fclose(f);
                                        }
                                    }
                                } break;
                                case BTN_SAVE: {
                                    char path[1024] = "";
                                    if (file_dialog_save("Save Chess Game", "game.fen", path, sizeof(path))) {
                                        char fen[256] = "";
                                        board_to_fen(&board[move_count], fen);
                                        FILE *f = fopen(path, "w");
                                        if (f) { fputs(fen, f); fputc('\n', f); fclose(f);
                                            coach_announce(&coach, "Saved."); }
                                        else  { coach_announce(&coach, "Save failed."); }
                                    }
                                } break;
                                case BTN_UNDO:
                                    if (move_count > 0) {
                                        move_count--;
                                        selected = 0; sel_moves.count = 0;
                                        game_over = 0; status_idx = 0;
                                        anim_active = 0;
                                    }
                                    break;
                                case BTN_REDO:
                                    if (move_count < latest_move) {
                                        move_count++;
                                        selected = 0; sel_moves.count = 0;
                                    }
                                    break;
                                case BTN_HINT: {
                                    Color stm = (move_count % 2 == 0) ? WHITE : BLACK;
                                    EngineMove m = engine_best_move(&board[move_count], stm, LEVEL_HARD);
                                    if (m.valid) {
                                        hint_active = 1;
                                        hint_from_r = m.from_row; hint_from_c = m.from_col;
                                        hint_to_r = m.to_row; hint_to_c = m.to_col;
                                        hint_until = SDL_GetTicks() + 3000;
                                        char buf[64];
                                        snprintf(buf, sizeof(buf), "Try %c%d \xe2\x86\x92 %c%d",
                                                 'a'+m.from_col, 8-m.from_row,
                                                 'a'+m.to_col, 8-m.to_row);
                                        coach_announce(&coach, buf);
                                    }
                                } break;
                                case BTN_FLIP:
                                    board_flipped = !board_flipped; break;
                                case BTN_FENCOPY: {
                                    char fen[256] = "";
                                    board_to_fen(&board[move_count], fen);
                                    SDL_SetClipboardText(fen);
                                    coach_announce(&coach, "FEN copied to clipboard.");
                                } break;
                                case BTN_FENPASTE: {
                                    if (SDL_HasClipboardText()) {
                                        char *txt = SDL_GetClipboardText();
                                        if (txt && is_valid_fen(txt)) {
                                            init_board(&board[0]);
                                            fen_to_board(&board[0], txt);
                                            move_count = 0; latest_move = 0;
                                            game_over = 0; selected = 0; sel_moves.count = 0;
                                            coach_announce(&coach, "Position loaded from clipboard.");
                                        } else {
                                            coach_announce(&coach, "Clipboard isn't a valid FEN.");
                                        }
                                        if (txt) SDL_free(txt);
                                    }
                                } break;
                                case BTN_DRAW:
                                    if (!game_over) {
                                        game_over = 1; status_idx = 18;
                                        if (sound_on && audio_ready && sound[6])
                                            Mix_PlayChannel(-1, sound[6], 0);
                                        coach_announce(&coach, "Draw by agreement.");
                                    }
                                    break;
                                case BTN_RESIGN:
                                    if (!game_over) {
                                        Color stm = (move_count % 2 == 0) ? WHITE : BLACK;
                                        game_over = 1;
                                        status_idx = (stm == WHITE) ? 16 : 17;
                                        if (sound_on && audio_ready && sound[6])
                                            Mix_PlayChannel(-1, sound[6], 0);
                                        coach_announce(&coach, status_strings[status_idx]);
                                    }
                                    break;
                            }
                            break;
                        }
                    }
                    if (handled) continue;

                    /* --- Promotion popup --- */
                    if (prom_active) {
                        PromOpt opts[4];
                        compute_prom_rects(&L, prom_r, prom_c, prom_color, board_flipped, opts);
                        for (int i = 0; i < 4; i++) {
                            if (SDL_PointInRect(&pt, &opts[i].rect)) {
                                promote_pawn(&board[move_count], prom_r, prom_c, opts[i].t);
                                if (move_count > 0)
                                    compute_san(&board[move_count-1], &board[move_count],
                                                &log[move_count-1]);
                                prom_active = 0;
                                if (sound_on && audio_ready && sound[4])
                                    Mix_PlayChannel(-1, sound[4], 0);
                                break;
                            }
                        }
                        continue;
                    }

                    if (game_over) continue;

                    /* --- Board: select / drag start --- */
                    int row = -1, col = -1;
                    if (screen_to_sq(&L, mx, my, board_flipped, &row, &col)) {
                        chat_focus = 0;
                        Color stm = (move_count % 2 == 0) ? WHITE : BLACK;
                        /* AI's turn? ignore. */
                        if (mode_has_cpu(mode) && stm != mode_human_side(mode)) {
                            continue;
                        }
                        Piece p = board[move_count].board_places[row][col];
                        if (selected && (row != sel_r || col != sel_c)) {
                            /* Click destination if it's a legal move. */
                            int legal = 0;
                            for (int k = 0; k < sel_moves.count; k++) {
                                if (sel_moves.moves[k].row == row &&
                                    sel_moves.moves[k].col == col) { legal = 1; break; }
                            }
                            if (legal) {
                                /* Capture sound? Particles? */
                                int was_capture = board[move_count].board_places[row][col].in_game;
                                move_piece(board, sel_r, sel_c, row, col, move_count,
                                           sound_on ? sound : NULL);
                                /* Animation */
                                anim_active = 1;
                                anim_start = SDL_GetTicks();
                                anim_from_r = sel_r; anim_from_c = sel_c;
                                anim_to_r = row; anim_to_c = col;
                                anim_type = p.piece_type;
                                anim_color = p.color;
                                if (was_capture) {
                                    SDL_Rect rr = square_screen(&L, row, col, board_flipped);
                                    spawn_particles(rr.x + rr.w/2, rr.y + rr.h/2,
                                                    (SDL_Color){255,200,80,255});
                                }
                                move_count++; latest_move = move_count;
                                compute_san(&board[move_count-1], &board[move_count],
                                            &log[move_count-1]);
                                if (check_pawn_promotion(&board[move_count], row, col)) {
                                    prom_active = 1;
                                    prom_r = row; prom_c = col;
                                    prom_color = (move_count % 2 == 1) ? WHITE : BLACK;
                                }
                                selected = 0; sel_moves.count = 0;
                                hint_active = 0;
                            } else if (p.in_game && p.color == stm) {
                                sel_r = row; sel_c = col;
                                sel_moves = get_possible_moves(&board[move_count], row, col);
                                /* Filter illegal */
                                MoveList legal_moves = {.count = 0};
                                for (int k = 0; k < sel_moves.count; k++) {
                                    int tr = sel_moves.moves[k].row;
                                    int tc = sel_moves.moves[k].col;
                                    if (tr<0||tr>=8||tc<0||tc>=8) continue;
                                    if (is_it_llegal_move(row, col, tr, tc, &board[move_count])) continue;
                                    legal_moves.moves[legal_moves.count++] = sel_moves.moves[k];
                                }
                                sel_moves = legal_moves;
                                selected = 1;
                                /* Start drag */
                                dragging = 1;
                                drag_from_r = row; drag_from_c = col;
                                drag_mx = mx; drag_my = my;
                                drag_started_at_x = mx; drag_started_at_y = my;
                            } else {
                                selected = 0; sel_moves.count = 0;
                            }
                        } else {
                            if (p.in_game && p.color == stm) {
                                sel_r = row; sel_c = col;
                                sel_moves = get_possible_moves(&board[move_count], row, col);
                                MoveList legal_moves = {.count = 0};
                                for (int k = 0; k < sel_moves.count; k++) {
                                    int tr = sel_moves.moves[k].row;
                                    int tc = sel_moves.moves[k].col;
                                    if (tr<0||tr>=8||tc<0||tc>=8) continue;
                                    if (is_it_llegal_move(row, col, tr, tc, &board[move_count])) continue;
                                    legal_moves.moves[legal_moves.count++] = sel_moves.moves[k];
                                }
                                sel_moves = legal_moves;
                                selected = 1;
                                dragging = 1;
                                drag_from_r = row; drag_from_c = col;
                                drag_mx = mx; drag_my = my;
                                drag_started_at_x = mx; drag_started_at_y = my;
                            } else {
                                selected = 0; sel_moves.count = 0;
                            }
                        }
                    }
                } else if (ev.button.button == SDL_BUTTON_RIGHT) {
                    int row, col;
                    if (screen_to_sq(&L, ev.button.x, ev.button.y, board_flipped, &row, &col)) {
                        marked[row][col] = !marked[row][col];
                    }
                    selected = 0; sel_moves.count = 0;
                }
            }
            else if (ev.type == SDL_MOUSEMOTION) {
                if (dragging) {
                    drag_mx = ev.motion.x; drag_my = ev.motion.y;
                }
            }
            else if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT) {
                if (dragging) {
                    int row, col;
                    int moved_via_drag = 0;
                    if (screen_to_sq(&L, ev.button.x, ev.button.y, board_flipped, &row, &col)) {
                        int diff_x = abs(ev.button.x - drag_started_at_x);
                        int diff_y = abs(ev.button.y - drag_started_at_y);
                        int is_real_drag = (diff_x > 8 || diff_y > 8);
                        int legal = 0;
                        for (int k = 0; k < sel_moves.count; k++) {
                            if (sel_moves.moves[k].row == row &&
                                sel_moves.moves[k].col == col) { legal = 1; break; }
                        }
                        if (is_real_drag && legal && (row != drag_from_r || col != drag_from_c)) {
                            int was_capture = board[move_count].board_places[row][col].in_game;
                            Piece p = board[move_count].board_places[drag_from_r][drag_from_c];
                            move_piece(board, drag_from_r, drag_from_c, row, col, move_count,
                                       sound_on ? sound : NULL);
                            anim_active = 1; anim_start = SDL_GetTicks();
                            anim_from_r = drag_from_r; anim_from_c = drag_from_c;
                            anim_to_r = row; anim_to_c = col;
                            anim_type = p.piece_type; anim_color = p.color;
                            if (was_capture) {
                                SDL_Rect rr = square_screen(&L, row, col, board_flipped);
                                spawn_particles(rr.x + rr.w/2, rr.y + rr.h/2,
                                                (SDL_Color){255,200,80,255});
                            }
                            move_count++; latest_move = move_count;
                            compute_san(&board[move_count-1], &board[move_count],
                                        &log[move_count-1]);
                            if (check_pawn_promotion(&board[move_count], row, col)) {
                                prom_active = 1; prom_r = row; prom_c = col;
                                prom_color = (move_count % 2 == 1) ? WHITE : BLACK;
                            }
                            selected = 0; sel_moves.count = 0;
                            moved_via_drag = 1;
                            hint_active = 0;
                        }
                    }
                    (void)moved_via_drag;
                    dragging = 0;
                }
            }
            else if (ev.type == SDL_MOUSEWHEEL && L.chat_w > 0) {
                /* Future: scroll chat. */
            }
        }

        /* ---------------- AI's turn? ---------------- */
        Color stm_now = (move_count % 2 == 0) ? WHITE : BLACK;
        if (!game_over && !prom_active && mode_has_cpu(mode) &&
            stm_now != mode_human_side(mode) && !g_ai.busy && !g_ai.done &&
            !anim_active) {
            ai_kick(&board[move_count], stm_now, mode_level(mode));
        }
        if (g_ai.done) {
            EngineMove m = g_ai.result;
            g_ai.done = 0;
            if (m.valid) {
                Piece p = board[move_count].board_places[m.from_row][m.from_col];
                int was_capture = board[move_count].board_places[m.to_row][m.to_col].in_game;
                move_piece(board, m.from_row, m.from_col, m.to_row, m.to_col, move_count,
                           sound_on ? sound : NULL);
                anim_active = 1; anim_start = SDL_GetTicks();
                anim_from_r = m.from_row; anim_from_c = m.from_col;
                anim_to_r = m.to_row; anim_to_c = m.to_col;
                anim_type = p.piece_type; anim_color = p.color;
                if (was_capture) {
                    SDL_Rect rr = square_screen(&L, m.to_row, m.to_col, board_flipped);
                    spawn_particles(rr.x + rr.w/2, rr.y + rr.h/2,
                                    (SDL_Color){255,200,80,255});
                }
                move_count++; latest_move = move_count;
                compute_san(&board[move_count-1], &board[move_count],
                            &log[move_count-1]);
                if (check_pawn_promotion(&board[move_count], m.to_row, m.to_col)) {
                    promote_pawn(&board[move_count], m.to_row, m.to_col, QUEEN);
                    if (move_count > 0)
                        compute_san(&board[move_count-1], &board[move_count],
                                    &log[move_count-1]);
                }
            }
        }

        /* ---------------- Game-end detection ---------------- */
        if (!game_over && move_count == latest_move) {
            Color stm = (move_count % 2 == 0) ? WHITE : BLACK;
            if (is_checkmate(&board[move_count], stm)) {
                game_over = 1;
                status_idx = (stm == BLACK) ? 1 : 2;
                start_confetti(L.board_x, L.board_y, L.board_size);
                if (sound_on && audio_ready && sound[6]) Mix_PlayChannel(-1, sound[6], 0);
                coach_announce(&coach, status_strings[status_idx]);
            } else if (is_stalemate(&board[move_count], stm)) {
                game_over = 1; status_idx = 3;
                coach_announce(&coach, status_strings[status_idx]);
            } else if (board[move_count].halfmove_clock >= 100) {
                game_over = 1; status_idx = 6;
                coach_announce(&coach, status_strings[status_idx]);
            } else if (is_insufficient_material(&board[move_count])) {
                game_over = 1; status_idx = 4;
                coach_announce(&coach, status_strings[status_idx]);
            } else if (is_threefold_repetition(board, move_count)) {
                game_over = 1; status_idx = 5;
                coach_announce(&coach, status_strings[status_idx]);
            }
        }

        if (!played_start_sound) {
            played_start_sound = 1;
            if (sound_on && audio_ready && sound[5]) Mix_PlayChannel(-1, sound[5], 0);
        }

        coach_poll(&coach);

        Uint32 now = SDL_GetTicks();
        if (anim_active && now - anim_start > 160) anim_active = 0;
        if (hint_active && now > hint_until) hint_active = 0;

        /* ====================== RENDER ====================== */
        fill_rect_c(ren, (SDL_Rect){0,0,L.win_w,L.win_h}, T->bg);

        /* Top bar */
        fill_rect_c(ren, (SDL_Rect){0,0,L.win_w,L.top_bar_h}, T->panel);
        draw_text(ren, f_title, "Chess Pro", T->text, 16, 8);
        /* Pills */
        char pill_text[64];
        snprintf(pill_text, sizeof(pill_text), "Mode: %s", MODE_SHORT[mode]);
        fill_round_rect(ren, pill_mode, 14, T->panel2);
        draw_text_centered(ren, f_small, pill_text, T->text, pill_mode);
        snprintf(pill_text, sizeof(pill_text), "Theme: %s", T->name);
        fill_round_rect(ren, pill_theme, 14, T->panel2);
        draw_text_centered(ren, f_small, pill_text, T->text, pill_theme);
        snprintf(pill_text, sizeof(pill_text), "Sound: %s", sound_on ? "On":"Off");
        fill_round_rect(ren, pill_sound, 14, T->panel2);
        draw_text_centered(ren, f_small, pill_text, T->text, pill_sound);

        /* Captured strip (top = pieces white has captured -> meaning black's captured) */
        render_captured(ren, f_small, &board[move_count], BLACK, piece_tex,
                        L.board_x, L.board_y - 24, L.board_size, 22, T->dim);
        render_captured(ren, f_small, &board[move_count], WHITE, piece_tex,
                        L.board_x, L.board_y + L.board_size + 4, L.board_size, 22, T->dim);

        /* Board squares + coordinate labels */
        for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) {
            SDL_Rect sq = square_screen(&L, r, c, board_flipped);
            SDL_Color base = ((r + c) & 1) ? T->dark : T->light;
            fill_rect_c(ren, sq, base);
        }
        /* Coordinate labels in corner of each rank/file */
        for (int r = 0; r < 8; r++) {
            char buf[2] = {(char)('8' - (board_flipped ? (7 - r) : r)), 0};
            int dr = board_flipped ? (7 - r) : r;
            SDL_Color col_text = ((r) & 1) ? T->light : T->dark;
            (void)dr;
            SDL_Rect sq = square_screen(&L, r, 0, board_flipped);
            draw_text(ren, f_small, buf, col_text, sq.x + 3, sq.y + 2);
        }
        for (int c = 0; c < 8; c++) {
            char buf[2] = {(char)('a' + (board_flipped ? (7 - c) : c)), 0};
            SDL_Rect sq = square_screen(&L, 7, c, board_flipped);
            SDL_Color col_text = ((7 + c) & 1) ? T->light : T->dark;
            draw_text(ren, f_small, buf, col_text, sq.x + sq.w - 12, sq.y + sq.h - 18);
        }

        /* Last-move highlight (diff between board[move_count-1] and board[move_count]) */
        if (move_count > 0) {
            Board *prev = &board[move_count - 1];
            Board *curr = &board[move_count];
            for (int r=0;r<8;r++) for (int c=0;c<8;c++) {
                Piece pp=prev->board_places[r][c], pc=curr->board_places[r][c];
                int diff = (pp.in_game != pc.in_game) ||
                           (pp.in_game && pc.in_game &&
                            (pp.piece_type != pc.piece_type || pp.color != pc.color));
                if (diff) {
                    SDL_Rect sq = square_screen(&L, r, c, board_flipped);
                    fill_rect_c(ren, sq, T->lastmove);
                }
            }
        }

        /* Selected square + move indicators */
        if (selected) {
            SDL_Rect sq = square_screen(&L, sel_r, sel_c, board_flipped);
            fill_rect_c(ren, sq, T->selected);
            for (int k = 0; k < sel_moves.count; k++) {
                int rr = sel_moves.moves[k].row, cc = sel_moves.moves[k].col;
                SDL_Rect d = square_screen(&L, rr, cc, board_flipped);
                int has_piece = board[move_count].board_places[rr][cc].in_game;
                if (has_piece) {
                    draw_ring(ren, d.x + d.w/2, d.y + d.h/2, d.w * 4 / 10, d.w / 12, T->dot);
                } else {
                    fill_circle_c(ren, d.x + d.w/2, d.y + d.h/2, d.w / 7, T->dot);
                }
            }
        }

        /* Right-click marks */
        for (int r=0;r<8;r++) for (int c=0;c<8;c++) if (marked[r][c]) {
            SDL_Rect d = square_screen(&L, r, c, board_flipped);
            SDL_Color c2 = T->marker;
            for (int t = 0; t < 4; t++) {
                SDL_Rect e = {d.x + t, d.y + t, d.w - 2*t, d.h - 2*t};
                draw_rect_c(ren, e, c2);
            }
        }

        /* Check overlay on the king */
        for (int side = 0; side < 2; side++) {
            int kr = board[move_count].players[side].king_row;
            int kc = board[move_count].players[side].king_col;
            if (kr<0||kc<0) continue;
            Color opp = (side == WHITE) ? BLACK : WHITE;
            if (is_square_attacked(&board[move_count], kr, kc, opp)) {
                SDL_Rect d = square_screen(&L, kr, kc, board_flipped);
                /* Pulsing alpha. */
                Uint32 t = SDL_GetTicks();
                int alpha = 120 + (int)(60 * sinf(t / 200.0f));
                SDL_Color c = T->check; c.a = (Uint8)alpha;
                fill_rect_c(ren, d, c);
            }
        }

        /* Pieces (skip the one being dragged / animated) */
        for (int r=0;r<8;r++) for (int c=0;c<8;c++) {
            Piece p = board[move_count].board_places[r][c];
            if (!p.in_game) continue;
            if (dragging && r == drag_from_r && c == drag_from_c) continue;
            if (anim_active && r == anim_to_r && c == anim_to_c) continue;
            SDL_Rect d = square_screen(&L, r, c, board_flipped);
            d.x += 4; d.y += 4; d.w -= 8; d.h -= 8;
            SDL_RenderCopy(ren, piece_tex[p.color][p.piece_type], NULL, &d);
        }

        /* Animated piece */
        if (anim_active) {
            float t = (float)(now - anim_start) / 160.0f;
            if (t > 1) t = 1;
            float ease = t < 0.5f ? 2*t*t : 1 - 2*(1-t)*(1-t);
            SDL_Rect a = square_screen(&L, anim_from_r, anim_from_c, board_flipped);
            SDL_Rect b = square_screen(&L, anim_to_r, anim_to_c, board_flipped);
            int x = (int)(a.x + (b.x - a.x) * ease);
            int y = (int)(a.y + (b.y - a.y) * ease);
            SDL_Rect d = {x + 4, y + 4, a.w - 8, a.h - 8};
            SDL_RenderCopy(ren, piece_tex[anim_color][anim_type], NULL, &d);
        }

        /* Dragged piece */
        if (dragging) {
            Piece p = board[move_count].board_places[drag_from_r][drag_from_c];
            if (p.in_game) {
                int sz = L.square_size;
                SDL_Rect d = {drag_mx - sz/2 + 2, drag_my - sz/2 + 2, sz - 4, sz - 4};
                /* Shadow */
                SDL_Rect sh = {d.x + 4, d.y + 6, d.w, d.h};
                set_color(ren, (SDL_Color){0,0,0,80});
                SDL_RenderFillRect(ren, &sh);
                SDL_RenderCopy(ren, piece_tex[p.color][p.piece_type], NULL, &d);
            }
        }

        /* Hint arrow */
        if (hint_active) {
            SDL_Rect a = square_screen(&L, hint_from_r, hint_from_c, board_flipped);
            SDL_Rect b = square_screen(&L, hint_to_r, hint_to_c, board_flipped);
            SDL_Color arr = T->accent; arr.a = 200;
            draw_arrow(ren, a.x + a.w/2, a.y + a.h/2, b.x + b.w/2, b.y + b.h/2,
                       arr, L.square_size / 10);
        }

        /* Particles overlay */
        render_particles(ren);

        /* Promotion popup */
        if (prom_active) {
            PromOpt opts[4];
            compute_prom_rects(&L, prom_r, prom_c, prom_color, board_flipped, opts);
            for (int i = 0; i < 4; i++) {
                fill_rect_c(ren, opts[i].rect, (SDL_Color){240,240,240,255});
                draw_rect_c(ren, opts[i].rect, (SDL_Color){80,80,80,255});
                SDL_Rect d = opts[i].rect;
                d.x += 4; d.y += 4; d.w -= 8; d.h -= 8;
                SDL_RenderCopy(ren, piece_tex[prom_color][opts[i].t], NULL, &d);
            }
        }

        /* Coach panel */
        if (L.chat_w > 0) {
            SDL_Rect panel = {L.chat_x, L.chat_y, L.chat_w, L.chat_h};
            fill_rect_c(ren, panel, T->panel);
            /* Header */
            SDL_Rect hdr = {L.chat_x, L.chat_y, L.chat_w, 32};
            fill_rect_c(ren, hdr, T->panel2);
            draw_text(ren, f_ui, "Coach", T->text, L.chat_x + 12, L.chat_y + 6);
            const char *llm = coach.llm_enabled ? "(LLM on)" : "(offline)";
            int llm_w = 80;
            draw_text(ren, f_small, llm, T->dim, L.chat_x + L.chat_w - llm_w, L.chat_y + 9);

            /* Messages */
            int my_top = L.chat_y + 40;
            int my_bot = L.chat_input_y - 8;
            int line_h = 20;
            int max_lines = (my_bot - my_top) / line_h;
            int start = coach.count > max_lines ? coach.count - max_lines : 0;
            int yy = my_top;
            for (int i = start; i < coach.count; i++) {
                SDL_Color col = T->text;
                const char *prefix = "";
                if (coach.history[i].role == COACH_USER) {
                    col = (SDL_Color){180, 220, 255, 255};
                    prefix = "you: ";
                } else if (coach.history[i].role == COACH_BOT) {
                    col = T->text;
                    prefix = "coach: ";
                } else {
                    col = T->dim;
                    prefix = "* ";
                }
                char buf[COACH_LINE_LEN + 16];
                snprintf(buf, sizeof(buf), "%s%s", prefix, coach.history[i].text);
                /* Word-wrap by trimming length to fit. */
                int avail = L.chat_w - 24;
                int max_chars = avail / 7;
                if ((int)strlen(buf) > max_chars && max_chars > 4) buf[max_chars] = '\0';
                draw_text(ren, f_small, buf, col, L.chat_x + 12, yy);
                yy += line_h;
            }

            /* Input */
            SDL_Rect chat_input = {L.chat_x + 8, L.chat_input_y + 4,
                                   L.chat_w - 80, L.chat_input_h - 8};
            SDL_Rect send_btn = {chat_input.x + chat_input.w + 6,
                                 L.chat_input_y + 4, 64, L.chat_input_h - 8};
            SDL_Color in_bg = chat_focus ? T->panel2 : (SDL_Color){30,30,30,255};
            fill_rect_c(ren, chat_input, in_bg);
            draw_rect_c(ren, chat_input,
                        chat_focus ? T->accent : T->dim);
            const char *display = chat_buf[0] ? chat_buf :
                                  (chat_focus ? "" : "Ask Coach...");
            SDL_Color tc = chat_buf[0] ? T->text : T->dim;
            draw_text(ren, f_small, display, tc, chat_input.x + 6, chat_input.y + 5);
            fill_round_rect(ren, send_btn, 10, T->accent);
            draw_text_centered(ren, f_small, "Send", T->text, send_btn);
        }

        /* Coach panel open/close toggle -- always shown against the right edge.
         * Hover hint via simple colour swap. */
        {
            SDL_Rect toggle_btn = {L.win_w - g_toggle_btn_w - 4,
                                   L.top_bar_h + L.chat_h / 2 - 32,
                                   g_toggle_btn_w, 64};
            int mx2, my2; SDL_GetMouseState(&mx2, &my2);
            SDL_Point pt2 = {mx2, my2};
            int hover = SDL_PointInRect(&pt2, &toggle_btn);
            fill_round_rect(ren, toggle_btn, 6, hover ? T->button_hv : T->panel2);
            draw_text_centered(ren, f_ui,
                               chat_open ? ">" : "<",
                               T->text, toggle_btn);
            /* tiny label below */
            SDL_Rect lbl = {toggle_btn.x - 18, toggle_btn.y + 70, 60, 16};
            (void)lbl;
        }

        /* Bottom toolbar */
        fill_rect_c(ren, (SDL_Rect){0, L.win_h - L.bottom_bar_h, L.win_w, L.bottom_bar_h}, T->panel);
        int mx, my; SDL_GetMouseState(&mx, &my);
        for (int i = 0; i < nbtn; i++) {
            SDL_Point pt = {mx, my};
            int hover = SDL_PointInRect(&pt, &buttons[i].r);
            SDL_Color c = buttons[i].c;
            if (hover) {
                c = (buttons[i].id == BTN_RESIGN) ?
                    (SDL_Color){200, 95, 95, 255} : T->button_hv;
            }
            fill_round_rect(ren, buttons[i].r, 8, c);
            draw_text_centered(ren, f_small, buttons[i].label, T->text, buttons[i].r);
        }

        /* Game-over overlay */
        if (game_over) {
            SDL_Rect dim = {L.board_x, L.board_y, L.board_size, L.board_size};
            set_color(ren, (SDL_Color){0,0,0,100});
            SDL_RenderFillRect(ren, &dim);
            SDL_Rect modal = {L.board_x + L.board_size/2 - 180,
                              L.board_y + L.board_size/2 - 80, 360, 160};
            fill_round_rect(ren, modal, 14, T->panel);
            draw_rect_c(ren, modal, T->accent);
            const char *line = status_strings[status_idx];
            draw_text_centered(ren, f_title, "Game over", T->text,
                               (SDL_Rect){modal.x, modal.y + 16, modal.w, 32});
            draw_text_centered(ren, f_ui, line, T->text,
                               (SDL_Rect){modal.x, modal.y + 64, modal.w, 28});
            draw_text_centered(ren, f_small, "Click 'New' for another game.",
                               T->dim,
                               (SDL_Rect){modal.x, modal.y + 100, modal.w, 24});
        }
        render_confetti(ren);

        /* AI thinking indicator */
        if (g_ai.busy) {
            SDL_Rect bar = {L.board_x, L.board_y - 24, 150, 18};
            fill_round_rect(ren, bar, 8, T->accent);
            draw_text_centered(ren, f_small, "Computer thinking...", T->text, bar);
        }

        SDL_RenderPresent(ren);
        SDL_Delay(8);
    }

    coach_shutdown(&coach);
    SDL_StopTextInput();

    /* Cleanup */
    for (int i = 0; i < 10; i++) if (sound[i]) Mix_FreeChunk(sound[i]);
    if (audio_ready) Mix_CloseAudio();
    for (int co = 0; co < 2; co++) for (int t = 0; t < 6; t++)
        if (piece_tex[co][t]) SDL_DestroyTexture(piece_tex[co][t]);
    TTF_CloseFont(f_title); TTF_CloseFont(f_ui); TTF_CloseFont(f_small);
    TTF_Quit();
    free(board); free(log);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

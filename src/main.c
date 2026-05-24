#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"

#define WINDOW_WIDTH 1100
#define WINDOW_HEIGHT 820
#define BOARD_OFFSET_X 60
#define BOARD_OFFSET_Y 90
#define SQUARE_SIZE 80
#define BOARD_SIZE_PX (SQUARE_SIZE * 8)
#define SIDE_PANEL_X (BOARD_OFFSET_X + BOARD_SIZE_PX + 20)
#define SIDE_PANEL_W (WINDOW_WIDTH - SIDE_PANEL_X - 20)

/* Chess.com inspired palette */
static const SDL_Color COL_LIGHT      = {238, 238, 210, 255}; /* cream  */
static const SDL_Color COL_DARK       = {118, 150,  86, 255}; /* green  */
static const SDL_Color COL_SELECTED   = {186, 202,  68, 255}; /* yellow-green */
static const SDL_Color COL_LASTMOVE   = {247, 247, 105, 180}; /* yellow */
static const SDL_Color COL_CHECK      = {235,  97,  80, 220}; /* red    */
static const SDL_Color COL_MOVE_DOT   = { 40,  40,  40, 110};
static const SDL_Color COL_BG         = { 49,  46,  43, 255};
static const SDL_Color COL_PANEL      = { 38,  36,  33, 255};
static const SDL_Color COL_PANEL_LITE = { 60,  57,  54, 255};
static const SDL_Color COL_BUTTON     = { 90, 130,  60, 255};
static const SDL_Color COL_BUTTON_HV  = {110, 160,  80, 255};
static const SDL_Color COL_BUTTON_RED = {175,  75,  75, 255};
static const SDL_Color COL_TEXT       = {235, 235, 235, 255};
static const SDL_Color COL_TEXT_DIM   = {180, 180, 180, 255};

/* ---------- Small helpers ---------- */
static void set_color(SDL_Renderer *ren, SDL_Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
}

static void fill_rect(SDL_Renderer *ren, SDL_Rect r, SDL_Color c) {
    set_color(ren, c);
    SDL_RenderFillRect(ren, &r);
}

static void draw_rect(SDL_Renderer *ren, SDL_Rect r, SDL_Color c) {
    set_color(ren, c);
    SDL_RenderDrawRect(ren, &r);
}

static void draw_text(SDL_Renderer *ren, TTF_Font *font, const char *text,
                      SDL_Color color, int x, int y) {
    if (!text || !*text) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_Rect r = {x, y, surf->w, surf->h};
    SDL_RenderCopy(ren, tex, NULL, &r);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

static void draw_text_centered(SDL_Renderer *ren, TTF_Font *font,
                               const char *text, SDL_Color color,
                               SDL_Rect rect) {
    if (!text || !*text) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_Rect dst = {
        rect.x + (rect.w - surf->w) / 2,
        rect.y + (rect.h - surf->h) / 2,
        surf->w, surf->h
    };
    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

/* Filled circle (Bresenham). */
static void fill_circle(SDL_Renderer *ren, int cx, int cy, int r, SDL_Color c) {
    set_color(ren, c);
    for (int dy = -r; dy <= r; dy++) {
        int dx_max = (int)SDL_sqrtf((float)(r * r - dy * dy));
        SDL_RenderDrawLine(ren, cx - dx_max, cy + dy, cx + dx_max, cy + dy);
    }
}

/* Ring (outlined annulus). */
static void draw_ring(SDL_Renderer *ren, int cx, int cy, int r, int thickness, SDL_Color c) {
    set_color(ren, c);
    int inner = r - thickness;
    for (int dy = -r; dy <= r; dy++) {
        int outer_dx = (int)SDL_sqrtf((float)(r * r - dy * dy));
        int inner_dx = 0;
        int inner_sq = inner * inner - dy * dy;
        if (inner_sq > 0) inner_dx = (int)SDL_sqrtf((float)inner_sq);
        if (inner_dx <= 0) {
            SDL_RenderDrawLine(ren, cx - outer_dx, cy + dy, cx + outer_dx, cy + dy);
        } else {
            SDL_RenderDrawLine(ren, cx - outer_dx, cy + dy, cx - inner_dx, cy + dy);
            SDL_RenderDrawLine(ren, cx + inner_dx, cy + dy, cx + outer_dx, cy + dy);
        }
    }
}

/* ---------- Board <-> screen translation (with optional flip) ---------- */
typedef struct {
    int x, y, w, h;
} ScreenRect;

static ScreenRect square_screen_rect(int row, int col, int flipped) {
    int dr = flipped ? (7 - row) : row;
    int dc = flipped ? (7 - col) : col;
    ScreenRect r;
    r.x = BOARD_OFFSET_X + dc * SQUARE_SIZE;
    r.y = BOARD_OFFSET_Y + dr * SQUARE_SIZE;
    r.w = SQUARE_SIZE;
    r.h = SQUARE_SIZE;
    return r;
}

static int screen_to_square(int x, int y, int flipped, int *row, int *col) {
    if (x < BOARD_OFFSET_X || x >= BOARD_OFFSET_X + BOARD_SIZE_PX) return 0;
    if (y < BOARD_OFFSET_Y || y >= BOARD_OFFSET_Y + BOARD_SIZE_PX) return 0;
    int dc = (x - BOARD_OFFSET_X) / SQUARE_SIZE;
    int dr = (y - BOARD_OFFSET_Y) / SQUARE_SIZE;
    *col = flipped ? (7 - dc) : dc;
    *row = flipped ? (7 - dr) : dr;
    return 1;
}

/* ---------- Move log / algebraic notation ---------- */
typedef struct {
    char text[16]; /* SAN-ish, e.g. "e4", "Nf3", "Bxc4", "O-O", "exd5", "e8=Q+" */
} MoveSAN;

static const char *piece_letter(Type t) {
    switch (t) {
        case KNIGHT: return "N";
        case BISHOP: return "B";
        case ROOK:   return "R";
        case QUEEN:  return "Q";
        case KING:   return "K";
        default:     return "";
    }
}

static void square_name(int row, int col, char *out) {
    out[0] = (char)('a' + col);
    out[1] = (char)('8' - row);
    out[2] = '\0';
}

/* Produce a simplified algebraic notation by diffing two consecutive board states.
 * Sets out->text. */
static void compute_san(Board *prev, Board *curr, MoveSAN *out) {
    int from_r = -1, from_c = -1;
    int to_r   = -1, to_c   = -1;
    int second_from_r = -1, second_from_c = -1;
    int second_to_r   = -1, second_to_c   = -1;
    Type moved_piece = PAWN;
    Color moved_color = WHITE;
    int captured = 0;
    Type promoted_to = PAWN;
    int promotion = 0;
    int en_passant = 0;

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            Piece p_prev = prev->board_places[r][c];
            Piece p_curr = curr->board_places[r][c];
            int prev_on = p_prev.in_game;
            int curr_on = p_curr.in_game;
            if (prev_on && !curr_on) {
                /* piece left this square */
                if (from_r < 0) { from_r = r; from_c = c; moved_piece = p_prev.piece_type; moved_color = p_prev.color; }
                else { second_from_r = r; second_from_c = c; }
            } else if (!prev_on && curr_on) {
                /* piece arrived here */
                if (to_r < 0) { to_r = r; to_c = c; }
                else { second_to_r = r; second_to_c = c; }
            } else if (prev_on && curr_on) {
                if (p_prev.piece_type != p_curr.piece_type ||
                    p_prev.color != p_curr.color) {
                    /* replaced - capture or promotion at this square */
                    if (p_prev.color != p_curr.color) {
                        /* capture: opponent's piece replaced by ours */
                        to_r = r; to_c = c;
                        captured = 1;
                    } else if (p_prev.piece_type == PAWN && p_curr.piece_type != PAWN) {
                        /* promotion in-place (couldn't happen for a non-capture, but safe) */
                        promotion = 1; promoted_to = p_curr.piece_type;
                    }
                }
            }
        }
    }

    /* Determine the original piece that moved -- if a promotion happened the
     * curr destination piece is the promoted type; the moving piece was PAWN. */
    if (from_r >= 0 && to_r >= 0) {
        Piece arrived = curr->board_places[to_r][to_c];
        Piece origin  = prev->board_places[from_r][from_c];
        moved_piece = origin.piece_type;
        moved_color = origin.color;
        if (origin.piece_type == PAWN && arrived.in_game && arrived.piece_type != PAWN) {
            promotion = 1;
            promoted_to = arrived.piece_type;
        }
        /* Detect en passant: pawn moved diagonally onto an empty square in prev */
        if (origin.piece_type == PAWN && from_c != to_c) {
            if (!prev->board_places[to_r][to_c].in_game) {
                en_passant = 1;
                captured = 1;
            }
        }
        /* Capture detected if dest had opponent piece */
        if (prev->board_places[to_r][to_c].in_game &&
            prev->board_places[to_r][to_c].color != origin.color) {
            captured = 1;
        }
    }

    /* Castling detection: king moved 2 columns, both king and rook moved. */
    int is_castle = 0;
    int castle_side = 0; /* 1 = kingside, 2 = queenside */
    if (moved_piece == KING && from_r >= 0 && to_r >= 0 &&
        from_r == to_r && (second_from_r >= 0)) {
        int col_diff = to_c - from_c;
        if (col_diff == 2)  { is_castle = 1; castle_side = 1; }
        if (col_diff == -2) { is_castle = 1; castle_side = 2; }
    }

    char buf[32];
    if (is_castle) {
        strcpy(buf, castle_side == 1 ? "O-O" : "O-O-O");
    } else {
        char dest[3] = {'-','-','\0'};
        if (to_r >= 0) square_name(to_r, to_c, dest);
        const char *plet = piece_letter(moved_piece);
        char src_file[2] = {'\0', '\0'};
        if (moved_piece == PAWN && captured) {
            src_file[0] = (char)('a' + (from_c >= 0 ? from_c : 0));
        }
        char promo[8] = "";
        if (promotion) {
            snprintf(promo, sizeof(promo), "=%s", piece_letter(promoted_to));
        }
        snprintf(buf, sizeof(buf), "%s%s%s%s%s",
                 plet, src_file, captured ? "x" : "", dest, promo);
    }

    /* Detect check / mate from the resulting position. */
    Color opp = (moved_color == WHITE) ? BLACK : WHITE;
    int in_check = curr->players[opp].is_in_check;
    int mate = 0;
    if (in_check) {
        if (get_total_possible_moves(curr, opp) == 0) mate = 1;
    }
    if (mate) strncat(buf, "#", sizeof(buf) - strlen(buf) - 1);
    else if (in_check) strncat(buf, "+", sizeof(buf) - strlen(buf) - 1);

    strncpy(out->text, buf, sizeof(out->text) - 1);
    out->text[sizeof(out->text) - 1] = '\0';
    (void)second_to_r; (void)second_to_c; (void)second_from_c; (void)en_passant;
}

/* ---------- Material values ---------- */
static int piece_value(Type t) {
    switch (t) {
        case PAWN:   return 1;
        case KNIGHT: return 3;
        case BISHOP: return 3;
        case ROOK:   return 5;
        case QUEEN:  return 9;
        default:     return 0;
    }
}

static int compute_material(Player *p) {
    int sum = 0;
    for (int i = 0; i < p->total_captured; i++) {
        sum += piece_value(p->captured_piece[i].piece_type);
    }
    return sum;
}

/* ---------- Promotion overlay state ---------- */
typedef struct {
    SDL_Rect rect;
    Type type;
} PromotionOption;

/* Draw the promotion overlay near the destination square and return the
 * 4 clickable option rectangles. */
static void compute_promotion_rects(int row, int col, Color color, int flipped,
                                    PromotionOption opts[4]) {
    Type types[4] = {QUEEN, KNIGHT, ROOK, BISHOP};
    ScreenRect base = square_screen_rect(row, col, flipped);
    int dir = (color == WHITE) ? 1 : -1; /* white promotes at row 0 => list down */
    /* For white promoting on row 0 (top), list 4 options going downward.
     * For black promoting on row 7 (bottom), list 4 options going upward. */
    if (color == BLACK) dir = -1;
    if (color == WHITE) dir = 1;
    int x = base.x;
    int y = base.y;
    for (int i = 0; i < 4; i++) {
        opts[i].type = types[i];
        opts[i].rect.x = x;
        opts[i].rect.y = y + dir * i * SQUARE_SIZE;
        opts[i].rect.w = SQUARE_SIZE;
        opts[i].rect.h = SQUARE_SIZE;
    }
}

/* ---------- Buttons ---------- */
typedef struct {
    SDL_Rect rect;
    const char *label;
    int action;
    SDL_Color color;
} Button;

enum {
    ACT_NONE = 0,
    ACT_NEW, ACT_SAVE, ACT_LOAD, ACT_UNDO, ACT_REDO,
    ACT_FLIP, ACT_RESIGN, ACT_DRAW
};

static int point_in_rect(int x, int y, SDL_Rect r) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

/* Captured piece icons drawn next to the player labels. */
static void render_captured_strip(SDL_Renderer *ren,
                                  SDL_Texture *piece_textures[2][6],
                                  Player *player_capturing,
                                  Color captured_color,
                                  int x, int y, int w, int icon_size) {
    int piece_counts[6] = {0,0,0,0,0,0};
    for (int i = 0; i < player_capturing->total_captured; i++) {
        Type t = player_capturing->captured_piece[i].piece_type;
        if (t >= 0 && t < 6) piece_counts[t]++;
    }
    Type order[5] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN};
    int cur_x = x;
    for (int i = 0; i < 5; i++) {
        Type t = order[i];
        for (int n = 0; n < piece_counts[t]; n++) {
            if (cur_x + icon_size > x + w) return;
            SDL_Rect dst = {cur_x, y, icon_size, icon_size};
            SDL_RenderCopy(ren, piece_textures[captured_color][t], NULL, &dst);
            cur_x += icon_size * 3 / 5; /* overlap slightly */
        }
        if (piece_counts[t] > 0) cur_x += 4;
    }
}

/* ---------- Game state ---------- */

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "Chess Pro -- by Mohamed & Fares",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        SDL_DestroyWindow(win);
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    if (TTF_Init() == -1) {
        fprintf(stderr, "TTF_Init Error: %s\n", TTF_GetError());
        return 1;
    }
    TTF_Font *font_title = TTF_OpenFont("assets/arial.ttf", 28);
    TTF_Font *font       = TTF_OpenFont("assets/arial.ttf", 18);
    TTF_Font *font_small = TTF_OpenFont("assets/arial.ttf", 14);
    if (!font_title || !font || !font_small) {
        fprintf(stderr, "TTF_OpenFont Error: %s\n", TTF_GetError());
        return 1;
    }

    /* Audio is optional -- the game must still run if no audio device exists. */
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
        sound[0] = Mix_LoadWAV("./assets/move-self.wav");
        sound[1] = Mix_LoadWAV("./assets/capture.wav");
        sound[2] = Mix_LoadWAV("./assets/move-check.wav");
        sound[3] = Mix_LoadWAV("./assets/castle.wav");
        sound[4] = Mix_LoadWAV("./assets/promote.wav");
        sound[5] = Mix_LoadWAV("./assets/game-start.wav");
        sound[6] = Mix_LoadWAV("./assets/game-end.wav");
        sound[7] = Mix_LoadWAV("./assets/illegal.wav");
    }

    SDL_Texture *piece_textures[2][6] = {{0}};
    piece_textures[WHITE][PAWN]   = IMG_LoadTexture(ren, "./assets/pawn_white.png");
    piece_textures[WHITE][ROOK]   = IMG_LoadTexture(ren, "./assets/rook_white.png");
    piece_textures[WHITE][KNIGHT] = IMG_LoadTexture(ren, "./assets/knight_white.png");
    piece_textures[WHITE][BISHOP] = IMG_LoadTexture(ren, "./assets/bishop_white.png");
    piece_textures[WHITE][QUEEN]  = IMG_LoadTexture(ren, "./assets/queen_white.png");
    piece_textures[WHITE][KING]   = IMG_LoadTexture(ren, "./assets/king_white.png");
    piece_textures[BLACK][PAWN]   = IMG_LoadTexture(ren, "./assets/pawn_black.png");
    piece_textures[BLACK][ROOK]   = IMG_LoadTexture(ren, "./assets/rook_black.png");
    piece_textures[BLACK][KNIGHT] = IMG_LoadTexture(ren, "./assets/knight_black.png");
    piece_textures[BLACK][BISHOP] = IMG_LoadTexture(ren, "./assets/bishop_black.png");
    piece_textures[BLACK][QUEEN]  = IMG_LoadTexture(ren, "./assets/queen_black.png");
    piece_textures[BLACK][KING]   = IMG_LoadTexture(ren, "./assets/king_black.png");

    /* Allocate the board history on the heap -- 500 Boards ~ 3MB which would
     * overflow Windows' default 1MB stack. */
    Board *board = (Board *)calloc(MAX_BOARDS, sizeof(Board));
    if (!board) {
        fprintf(stderr, "Failed to allocate board history\n");
        return 1;
    }

    MoveSAN *move_log = (MoveSAN *)calloc(MAX_BOARDS, sizeof(MoveSAN));
    if (!move_log) { free(board); return 1; }

    int move_count = 0;
    int latest_move = 0;
    int game_over = 0;
    int game_end_sound = 1;
    int there_is_a_promotion = 0;
    int promoted_row = -1, promoted_col = -1;
    int board_flipped = 0;
    int show_load_menu = 0;
    int played_start_sound = 0;
    int status_idx = 0;

    char save_names[MAX_SAVES][SAVE_NAME_LEN];
    int  save_count = 0;
    SDL_Rect save_rects[MAX_SAVES];
    int save_scroll = 0;

    MoveList highlighted_squares = {.count = 0};

    init_board(&board[0]);
    /* Initialize chessboard SDL_Rects for the initial position -- the layout
     * is also computed from screen rects so this is mostly a courtesy. */
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            board[0].chessboard[r][c] = (SDL_Rect){
                BOARD_OFFSET_X + c * SQUARE_SIZE,
                BOARD_OFFSET_Y + r * SQUARE_SIZE,
                SQUARE_SIZE, SQUARE_SIZE
            };
        }
    }

    /* Status messages (mirrors original strings, kept short for the UI). */
    static const char *status_text[] = {
        "",                                /* 0  empty   */
        "White wins by checkmate",         /* 1          */
        "Black wins by checkmate",         /* 2          */
        "Draw by stalemate",               /* 3          */
        "Draw by insufficient material",   /* 4          */
        "Draw by threefold repetition",    /* 5          */
        "Draw by fifty-move rule",         /* 6          */
        "Game saved",                      /* 7          */
        "Game loaded",                     /* 8          */
        "Move undone",                     /* 9          */
        "Move redone",                     /* 10         */
        "Invalid move",                    /* 11         */
        "Check",                           /* 12         */
        "Choose a promotion piece",        /* 13         */
        "Operation failed",                /* 14         */
        "Select a saved game to load",     /* 15         */
        "White resigned -- Black wins",    /* 16         */
        "Black resigned -- White wins",    /* 17         */
        "Draw by agreement",               /* 18         */
        "No saved games found",            /* 19         */
    };

    /* Bottom-row buttons */
    const int BTN_Y = BOARD_OFFSET_Y + BOARD_SIZE_PX + 40;
    Button buttons[] = {
        {{BOARD_OFFSET_X + 0  * 78, BTN_Y, 70, 36}, "New",    ACT_NEW,    COL_BUTTON},
        {{BOARD_OFFSET_X + 1  * 78, BTN_Y, 70, 36}, "Save",   ACT_SAVE,   COL_BUTTON},
        {{BOARD_OFFSET_X + 2  * 78, BTN_Y, 70, 36}, "Load",   ACT_LOAD,   COL_BUTTON},
        {{BOARD_OFFSET_X + 3  * 78, BTN_Y, 70, 36}, "Undo",   ACT_UNDO,   COL_BUTTON},
        {{BOARD_OFFSET_X + 4  * 78, BTN_Y, 70, 36}, "Redo",   ACT_REDO,   COL_BUTTON},
        {{BOARD_OFFSET_X + 5  * 78, BTN_Y, 70, 36}, "Flip",   ACT_FLIP,   COL_BUTTON},
        {{BOARD_OFFSET_X + 6  * 78, BTN_Y, 70, 36}, "Draw",   ACT_DRAW,   COL_BUTTON},
        {{BOARD_OFFSET_X + 7  * 78, BTN_Y, 70, 36}, "Resign", ACT_RESIGN, COL_BUTTON_RED},
    };
    int num_buttons = (int)(sizeof(buttons) / sizeof(buttons[0]));

    SDL_Event event;
    int running = 1;
    int mouse_x = 0, mouse_y = 0;

    while (running) {
        /* keep latest_move in sync */
        if (move_count > latest_move) latest_move = move_count;

        SDL_GetMouseState(&mouse_x, &mouse_y);

        /* ===== EVENT LOOP ===== */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_MOUSEWHEEL) {
                /* scroll move-history or load-menu */
                if (show_load_menu) {
                    save_scroll -= event.wheel.y * 2;
                    if (save_scroll < 0) save_scroll = 0;
                }
            } else if (event.type == SDL_KEYDOWN) {
                if (there_is_a_promotion) {
                    Type promoted_type = QUEEN;
                    int valid = 0;
                    switch (event.key.keysym.sym) {
                        case SDLK_q: promoted_type = QUEEN;  valid = 1; break;
                        case SDLK_r: promoted_type = ROOK;   valid = 1; break;
                        case SDLK_b: promoted_type = BISHOP; valid = 1; break;
                        case SDLK_n: promoted_type = KNIGHT; valid = 1; break;
                        default: break;
                    }
                    if (valid) {
                        promote_pawn(&board[move_count], promoted_row, promoted_col, promoted_type);
                        if (audio_ready && sound[4]) {
                            Mix_HaltChannel(-1);
                            Mix_PlayChannel(-1, sound[4], 0);
                        }
                        /* Refresh move log SAN to reflect promotion */
                        compute_san(&board[move_count - 1], &board[move_count], &move_log[move_count - 1]);
                        there_is_a_promotion = 0;
                        status_idx = 0;
                    }
                } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                    show_load_menu = 0;
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int mx = event.button.x;
                int my = event.button.y;

                /* --- Load menu modal --- */
                if (show_load_menu) {
                    int handled = 0;
                    for (int i = 0; i < save_count; i++) {
                        if (point_in_rect(mx, my, save_rects[i])) {
                            if (load_saved_game(save_names[i], &board[0])) {
                                move_count = 0;
                                latest_move = 0;
                                game_over   = 0;
                                game_end_sound = 1;
                                there_is_a_promotion = 0;
                                memset(move_log, 0, sizeof(MoveSAN) * MAX_BOARDS);
                                status_idx = 8;
                            } else {
                                status_idx = 14;
                            }
                            show_load_menu = 0;
                            handled = 1;
                            break;
                        }
                    }
                    /* close button (X) in title bar */
                    SDL_Rect close_x = {WINDOW_WIDTH / 2 + 140, 110, 30, 30};
                    if (!handled && point_in_rect(mx, my, close_x)) {
                        show_load_menu = 0;
                        handled = 1;
                    }
                    if (handled) continue;
                    /* clicking outside the modal: ignore */
                    SDL_Rect modal_rect = {WINDOW_WIDTH / 2 - 175, 110, 350, 500};
                    if (!point_in_rect(mx, my, modal_rect)) {
                        show_load_menu = 0;
                    }
                    continue;
                }

                /* --- Promotion popup --- */
                if (there_is_a_promotion) {
                    Color promoter_color = board[move_count].board_places[promoted_row][promoted_col].color;
                    PromotionOption opts[4];
                    compute_promotion_rects(promoted_row, promoted_col, promoter_color, board_flipped, opts);
                    int handled = 0;
                    for (int i = 0; i < 4; i++) {
                        if (point_in_rect(mx, my, opts[i].rect)) {
                            promote_pawn(&board[move_count], promoted_row, promoted_col, opts[i].type);
                            if (audio_ready && sound[4]) {
                                Mix_HaltChannel(-1);
                                Mix_PlayChannel(-1, sound[4], 0);
                            }
                            compute_san(&board[move_count - 1], &board[move_count], &move_log[move_count - 1]);
                            there_is_a_promotion = 0;
                            status_idx = 0;
                            handled = 1;
                            break;
                        }
                    }
                    if (handled) continue;
                    /* If the user clicked elsewhere while promoting, ignore. */
                    continue;
                }

                /* --- Bottom-row buttons --- */
                int button_clicked = 0;
                for (int i = 0; i < num_buttons; i++) {
                    if (point_in_rect(mx, my, buttons[i].rect)) {
                        button_clicked = 1;
                        switch (buttons[i].action) {
                            case ACT_NEW: {
                                init_board(&board[0]);
                                memset(move_log, 0, sizeof(MoveSAN) * MAX_BOARDS);
                                move_count = 0;
                                latest_move = 0;
                                game_over = 0;
                                game_end_sound = 1;
                                played_start_sound = 0;
                                there_is_a_promotion = 0;
                                highlighted_squares.count = 0;
                                status_idx = 0;
                            } break;
                            case ACT_SAVE: {
                                char fen[200];
                                board_to_fen(&board[move_count], fen);
                                status_idx = save_file(fen) ? 7 : 14;
                            } break;
                            case ACT_LOAD: {
                                save_count = list_saved_games(save_names, MAX_SAVES);
                                if (save_count == 0) { status_idx = 19; }
                                else { show_load_menu = 1; save_scroll = 0; status_idx = 15; }
                            } break;
                            case ACT_UNDO: {
                                if (move_count > 0) {
                                    move_count--;
                                    highlighted_squares.count = 0;
                                    game_over = 0;
                                    status_idx = 9;
                                }
                            } break;
                            case ACT_REDO: {
                                if (move_count < latest_move) {
                                    move_count++;
                                    highlighted_squares.count = 0;
                                    status_idx = 10;
                                }
                            } break;
                            case ACT_FLIP: board_flipped = !board_flipped; break;
                            case ACT_DRAW: {
                                if (!game_over) {
                                        game_over = 1;
                                    status_idx = 18;
                                    if (audio_ready && sound[6] && game_end_sound) {
                                        Mix_PlayChannel(-1, sound[6], 0);
                                        game_end_sound = 0;
                                    }
                                }
                            } break;
                            case ACT_RESIGN: {
                                if (!game_over) {
                                    Color side = (move_count % 2 == 0) ? WHITE : BLACK;
                                    game_over = 1;
                                    status_idx = (side == WHITE) ? 16 : 17;
                                    if (audio_ready && sound[6] && game_end_sound) {
                                        Mix_PlayChannel(-1, sound[6], 0);
                                        game_end_sound = 0;
                                    }
                                }
                            } break;
                        }
                        break;
                    }
                }
                if (button_clicked) continue;

                /* --- Board interaction --- */
                if (game_over) continue;
                int row, col;
                if (!screen_to_square(mx, my, board_flipped, &row, &col)) continue;
                Color side_to_move = (move_count % 2 == 0) ? WHITE : BLACK;

                Piece *clicked = &board[move_count].board_places[row][col];
                int is_own_piece = clicked->in_game && clicked->color == side_to_move;

                int clicked_highlight = 0;
                for (int i = 0; i < highlighted_squares.count; i++) {
                    if (highlighted_squares.moves[i].row == row &&
                        highlighted_squares.moves[i].col == col) {
                        clicked_highlight = 1;
                        break;
                    }
                }

                if (is_own_piece) {
                    /* select / re-select */
                    board[move_count].selected_piece = clicked;
                    MoveList possible = get_possible_moves(&board[move_count], row, col);
                    highlighted_squares = possible;
                    status_idx = 0;
                } else if (board[move_count].selected_piece && clicked_highlight) {
                    int from_row = board[move_count].selected_piece->row;
                    int from_col = board[move_count].selected_piece->col;
                    if (move_count + 1 >= MAX_BOARDS) {
                        status_idx = 14;
                        continue;
                    }
                    move_piece(board, from_row, from_col, row, col,
                               move_count, sound);
                    compute_san(&board[move_count], &board[move_count + 1], &move_log[move_count]);
                    move_count++;
                    latest_move = move_count;
                    status_idx = 0;
                    if (check_pawn_promotion(&board[move_count], row, col)) {
                        there_is_a_promotion = 1;
                        status_idx = 13;
                        promoted_row = row;
                        promoted_col = col;
                    }
                    board[move_count].selected_piece = NULL;
                    highlighted_squares.count = 0;
                    board[move_count].fullmove_number = (move_count / 2) + 1;
                } else if (board[move_count].selected_piece && !is_own_piece) {
                    /* clicking an illegal square: clear selection and (optionally)
                     * play the illegal sound if the user clicked a non-empty square. */
                    if (clicked->in_game && !clicked_highlight) {
                        if (audio_ready && sound[7]) Mix_PlayChannel(-1, sound[7], 0);
                    }
                    board[move_count].selected_piece = NULL;
                    highlighted_squares.count = 0;
                }
            }
        }

        /* ===== RENDER ===== */
        set_color(ren, COL_BG);
        SDL_RenderClear(ren);

        /* Title bar */
        draw_text(ren, font_title, "Chess Pro", COL_TEXT, BOARD_OFFSET_X, 12);

        /* Side panel background */
        fill_rect(ren, (SDL_Rect){SIDE_PANEL_X, BOARD_OFFSET_Y - 40,
                                  SIDE_PANEL_W, BOARD_SIZE_PX + 100}, COL_PANEL);

        /* Captured pieces strips + material */
        /* Determine which color is shown at top depending on flip */
        Color top_color    = board_flipped ? WHITE : BLACK;
        Color bottom_color = board_flipped ? BLACK : WHITE;
        Player *top_player    = &board[move_count].players[top_color];
        Player *bottom_player = &board[move_count].players[bottom_color];
        int top_material    = compute_material(top_player);
        int bottom_material = compute_material(bottom_player);

        /* Header strip (above board) shows opponent (the one captured by the other) */
        /* Captures by the top player (i.e., bottom color pieces captured by them) */
        char header_lbl[64];
        snprintf(header_lbl, sizeof(header_lbl), "%s",
                 top_color == WHITE ? "White" : "Black");
        draw_text(ren, font_small, header_lbl, COL_TEXT, BOARD_OFFSET_X, BOARD_OFFSET_Y - 26);
        render_captured_strip(ren, piece_textures, top_player, bottom_color,
                              BOARD_OFFSET_X + 60, BOARD_OFFSET_Y - 28, 350, 24);
        if (top_material > bottom_material) {
            char delta[16];
            snprintf(delta, sizeof(delta), "+%d", top_material - bottom_material);
            draw_text(ren, font_small, delta, COL_TEXT_DIM,
                      BOARD_OFFSET_X + 420, BOARD_OFFSET_Y - 26);
        }

        /* Footer strip (below board) shows bottom-color */
        char footer_lbl[64];
        snprintf(footer_lbl, sizeof(footer_lbl), "%s",
                 bottom_color == WHITE ? "White" : "Black");
        draw_text(ren, font_small, footer_lbl, COL_TEXT,
                  BOARD_OFFSET_X, BOARD_OFFSET_Y + BOARD_SIZE_PX + 8);
        render_captured_strip(ren, piece_textures, bottom_player, top_color,
                              BOARD_OFFSET_X + 60, BOARD_OFFSET_Y + BOARD_SIZE_PX + 8,
                              350, 24);
        if (bottom_material > top_material) {
            char delta[16];
            snprintf(delta, sizeof(delta), "+%d", bottom_material - top_material);
            draw_text(ren, font_small, delta, COL_TEXT_DIM,
                      BOARD_OFFSET_X + 420, BOARD_OFFSET_Y + BOARD_SIZE_PX + 10);
        }

        /* Board squares */
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                ScreenRect sr = square_screen_rect(row, col, board_flipped);
                SDL_Rect dst = {sr.x, sr.y, sr.w, sr.h};
                int dark = (row + col) % 2 != 0;
                fill_rect(ren, dst, dark ? COL_DARK : COL_LIGHT);
                /* keep the logical board's chessboard[] in sync with the display rect */
                board[move_count].chessboard[row][col] = dst;
            }
        }

        /* Highlight last move squares */
        if (move_count > 0) {
            for (int r = 0; r < 8; r++) {
                for (int c = 0; c < 8; c++) {
                    Piece p_prev = board[move_count - 1].board_places[r][c];
                    Piece p_curr = board[move_count].board_places[r][c];
                    int changed = (p_prev.in_game != p_curr.in_game) ||
                                  (p_prev.in_game && p_curr.in_game &&
                                   (p_prev.piece_type != p_curr.piece_type ||
                                    p_prev.color != p_curr.color));
                    if (changed) {
                        ScreenRect sr = square_screen_rect(r, c, board_flipped);
                        SDL_Rect dst = {sr.x, sr.y, sr.w, sr.h};
                        fill_rect(ren, dst, COL_LASTMOVE);
                    }
                }
            }
        }

        /* Highlight selected piece's square */
        if (board[move_count].selected_piece) {
            int sr_row = board[move_count].selected_piece->row;
            int sr_col = board[move_count].selected_piece->col;
            ScreenRect sr = square_screen_rect(sr_row, sr_col, board_flipped);
            SDL_Rect dst = {sr.x, sr.y, sr.w, sr.h};
            fill_rect(ren, dst, COL_SELECTED);
        }

        /* Red overlay on king if it's in check */
        Color cur_color = (move_count % 2 == 0) ? WHITE : BLACK;
        int k_row = board[move_count].players[cur_color].king_row;
        int k_col = board[move_count].players[cur_color].king_col;
        Color opp_color = (cur_color == WHITE) ? BLACK : WHITE;
        int king_in_check = is_square_attacked(&board[move_count], k_row, k_col, opp_color);
        if (king_in_check) {
            ScreenRect sr = square_screen_rect(k_row, k_col, board_flipped);
            SDL_Rect dst = {sr.x, sr.y, sr.w, sr.h};
            fill_rect(ren, dst, COL_CHECK);
        }

        /* Coordinate labels */
        for (int i = 0; i < 8; i++) {
            char letter_str[2] = {0,0};
            char number_str[2] = {0,0};
            int file_index = board_flipped ? (7 - i) : i;
            int rank_index = board_flipped ? (7 - i) : i;
            letter_str[0] = (char)('a' + file_index);
            number_str[0] = (char)('8' - rank_index);
            draw_text(ren, font_small, letter_str, COL_TEXT_DIM,
                      BOARD_OFFSET_X + i * SQUARE_SIZE + 6,
                      BOARD_OFFSET_Y + BOARD_SIZE_PX - 18);
            draw_text(ren, font_small, number_str, COL_TEXT_DIM,
                      BOARD_OFFSET_X + 4,
                      BOARD_OFFSET_Y + i * SQUARE_SIZE + 4);
        }

        /* Draw pieces */
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                Piece p = board[move_count].board_places[row][col];
                if (!p.in_game) continue;
                ScreenRect sr = square_screen_rect(row, col, board_flipped);
                SDL_Rect dst = {sr.x + 4, sr.y + 4, sr.w - 8, sr.h - 8};
                SDL_Texture *tex = piece_textures[p.color][p.piece_type];
                if (tex) SDL_RenderCopy(ren, tex, NULL, &dst);
            }
        }

        /* Draw move-target indicators (dot for empty, ring for capture) */
        for (int i = 0; i < highlighted_squares.count; i++) {
            int row = highlighted_squares.moves[i].row;
            int col = highlighted_squares.moves[i].col;
            if (row < 0 || row >= 8 || col < 0 || col >= 8) continue;
            ScreenRect sr = square_screen_rect(row, col, board_flipped);
            int cx = sr.x + sr.w / 2;
            int cy = sr.y + sr.h / 2;
            Piece tgt = board[move_count].board_places[row][col];
            if (tgt.in_game) {
                draw_ring(ren, cx, cy, sr.w / 2 - 4, 4, COL_MOVE_DOT);
            } else {
                fill_circle(ren, cx, cy, sr.w / 6, COL_MOVE_DOT);
            }
        }

        /* ===== Side panel: turn indicator + move history ===== */
        {
            SDL_Rect turn_rect = {SIDE_PANEL_X + 14, BOARD_OFFSET_Y - 32, SIDE_PANEL_W - 28, 32};
            fill_rect(ren, turn_rect, COL_PANEL_LITE);
            const char *turn_str = (move_count % 2 == 0) ? "White to move" : "Black to move";
            if (game_over) turn_str = "Game over";
            draw_text_centered(ren, font, turn_str, COL_TEXT, turn_rect);
        }

        /* Move history list */
        {
            SDL_Rect hist_rect = {SIDE_PANEL_X + 14, BOARD_OFFSET_Y + 8,
                                  SIDE_PANEL_W - 28, BOARD_SIZE_PX - 30};
            fill_rect(ren, hist_rect, COL_PANEL_LITE);
            draw_rect(ren, hist_rect, COL_PANEL);

            int line_h = 18;
            int max_lines = hist_rect.h / line_h - 1;
            int total_full_moves = (latest_move + 1) / 2;
            int start = 0;
            if (total_full_moves > max_lines) start = total_full_moves - max_lines;
            int y = hist_rect.y + 8;
            for (int i = start; i < total_full_moves; i++) {
                int white_idx = i * 2;
                int black_idx = white_idx + 1;
                char line[64];
                if (black_idx < latest_move) {
                    snprintf(line, sizeof(line), "%2d.  %-8s   %s",
                             i + 1, move_log[white_idx].text, move_log[black_idx].text);
                } else if (white_idx < latest_move) {
                    snprintf(line, sizeof(line), "%2d.  %-8s", i + 1, move_log[white_idx].text);
                } else {
                    continue;
                }
                /* Highlight current position */
                int curr_half = move_count - 1;
                if ((curr_half == white_idx || curr_half == black_idx) && move_count > 0) {
                    SDL_Rect hl = {hist_rect.x + 4, y - 2, hist_rect.w - 8, line_h};
                    fill_rect(ren, hl, COL_PANEL);
                }
                draw_text(ren, font_small, line, COL_TEXT, hist_rect.x + 10, y);
                y += line_h;
            }
            if (total_full_moves == 0) {
                draw_text(ren, font_small, "No moves yet.", COL_TEXT_DIM,
                          hist_rect.x + 12, hist_rect.y + 12);
            }
        }

        /* Status line under move history */
        {
            SDL_Rect st = {SIDE_PANEL_X + 14, BOARD_OFFSET_Y + BOARD_SIZE_PX - 18,
                           SIDE_PANEL_W - 28, 32};
            fill_rect(ren, st, COL_PANEL_LITE);
            const char *msg = (status_idx >= 0 &&
                               status_idx < (int)(sizeof(status_text)/sizeof(status_text[0])))
                              ? status_text[status_idx] : "";
            if (king_in_check && status_idx == 0 && !game_over) msg = "Check";
            draw_text_centered(ren, font_small, msg, COL_TEXT, st);
        }

        /* Bottom-row buttons */
        for (int i = 0; i < num_buttons; i++) {
            int hover = point_in_rect(mouse_x, mouse_y, buttons[i].rect);
            SDL_Color c = buttons[i].color;
            if (hover) c = (buttons[i].action == ACT_RESIGN)
                           ? (SDL_Color){200, 95, 95, 255} : COL_BUTTON_HV;
            fill_rect(ren, buttons[i].rect, c);
            draw_rect(ren, buttons[i].rect, (SDL_Color){0, 0, 0, 80});
            draw_text_centered(ren, font_small, buttons[i].label, COL_TEXT, buttons[i].rect);
        }

        /* ===== Promotion overlay ===== */
        if (there_is_a_promotion) {
            /* dim the board */
            fill_rect(ren, (SDL_Rect){BOARD_OFFSET_X, BOARD_OFFSET_Y,
                                       BOARD_SIZE_PX, BOARD_SIZE_PX},
                      (SDL_Color){0, 0, 0, 130});
            Color promoter_color = board[move_count].board_places[promoted_row][promoted_col].color;
            PromotionOption opts[4];
            compute_promotion_rects(promoted_row, promoted_col, promoter_color, board_flipped, opts);
            for (int i = 0; i < 4; i++) {
                fill_rect(ren, opts[i].rect, (SDL_Color){235, 235, 235, 245});
                draw_rect(ren, opts[i].rect, (SDL_Color){0, 0, 0, 255});
                SDL_Rect inner = {opts[i].rect.x + 6, opts[i].rect.y + 6,
                                  opts[i].rect.w - 12, opts[i].rect.h - 12};
                SDL_RenderCopy(ren, piece_textures[promoter_color][opts[i].type], NULL, &inner);
            }
        }

        /* ===== Game over overlay ===== */
        if (game_over) {
            fill_rect(ren, (SDL_Rect){BOARD_OFFSET_X, BOARD_OFFSET_Y,
                                      BOARD_SIZE_PX, BOARD_SIZE_PX},
                      (SDL_Color){0, 0, 0, 130});
            SDL_Rect modal = {BOARD_OFFSET_X + BOARD_SIZE_PX/2 - 180,
                              BOARD_OFFSET_Y + BOARD_SIZE_PX/2 - 80, 360, 160};
            fill_rect(ren, modal, COL_PANEL_LITE);
            draw_rect(ren, modal, COL_BUTTON);
            const char *title = "Game Over";
            if (status_idx == 1) title = "White wins!";
            else if (status_idx == 2) title = "Black wins!";
            else if (status_idx == 3) title = "Stalemate";
            else if (status_idx == 4) title = "Draw - Insufficient material";
            else if (status_idx == 5) title = "Draw - Threefold repetition";
            else if (status_idx == 6) title = "Draw - Fifty-move rule";
            else if (status_idx == 16) title = "Black wins by resignation";
            else if (status_idx == 17) title = "White wins by resignation";
            else if (status_idx == 18) title = "Draw by agreement";
            draw_text_centered(ren, font_title, title, COL_TEXT,
                               (SDL_Rect){modal.x, modal.y + 16, modal.w, 40});
            draw_text_centered(ren, font_small,
                               "Click 'New' to start another game.",
                               COL_TEXT_DIM,
                               (SDL_Rect){modal.x, modal.y + 70, modal.w, 30});
        }

        /* ===== Load menu modal ===== */
        if (show_load_menu) {
            fill_rect(ren, (SDL_Rect){0, 0, WINDOW_WIDTH, WINDOW_HEIGHT},
                      (SDL_Color){0, 0, 0, 160});
            SDL_Rect modal = {WINDOW_WIDTH/2 - 175, 110, 350, 500};
            fill_rect(ren, modal, COL_PANEL_LITE);
            draw_rect(ren, modal, COL_BUTTON);
            draw_text_centered(ren, font_title, "Load Game", COL_TEXT,
                               (SDL_Rect){modal.x, modal.y + 10, modal.w, 36});
            SDL_Rect close_x = {modal.x + modal.w - 38, modal.y + 12, 30, 30};
            fill_rect(ren, close_x, COL_BUTTON_RED);
            draw_text_centered(ren, font, "X", COL_TEXT, close_x);

            int y = modal.y + 56;
            int row_h = 32;
            int visible_count = save_count - save_scroll;
            if (visible_count < 0) visible_count = 0;
            if (visible_count > 12) visible_count = 12;
            for (int i = 0; i < visible_count; i++) {
                int idx = i + save_scroll;
                SDL_Rect row = {modal.x + 16, y, modal.w - 32, row_h - 4};
                int hover = point_in_rect(mouse_x, mouse_y, row);
                fill_rect(ren, row, hover ? COL_BUTTON_HV : COL_PANEL);
                draw_text(ren, font_small, save_names[idx], COL_TEXT,
                          row.x + 10, row.y + 6);
                save_rects[idx] = row;
                y += row_h;
            }
            if (save_count > 12) {
                char info[64];
                snprintf(info, sizeof(info), "Scroll for more (%d total)", save_count);
                draw_text_centered(ren, font_small, info, COL_TEXT_DIM,
                                   (SDL_Rect){modal.x, modal.y + modal.h - 30, modal.w, 20});
            }
        }

        /* Initial start-of-game sound */
        if (!played_start_sound && move_count == 0 && audio_ready && sound[5]) {
            Mix_PlayChannel(-1, sound[5], 0);
            played_start_sound = 1;
        }

        /* Game-end detection (only on the latest move, not when reviewing history) */
        if (!game_over && move_count == latest_move) {
            Color stm = (move_count % 2 == 0) ? WHITE : BLACK;
            if (is_checkmate(&board[move_count], stm)) {
                game_over = 1;
                status_idx = (stm == BLACK) ? 1 : 2;
            } else if (is_stalemate(&board[move_count], stm)) {
                game_over = 1;
                status_idx = 3;
            } else if (board[move_count].halfmove_clock >= 100) { /* 50 full moves */
                game_over = 1;
                status_idx = 6;
            } else if (is_insufficient_material(&board[move_count])) {
                game_over = 1;
                status_idx = 4;
            } else if (is_threefold_repetition(board, move_count)) {
                game_over = 1;
                status_idx = 5;
            }
            if (game_over && audio_ready && sound[6] && game_end_sound) {
                Mix_PlayChannel(-1, sound[6], 0);
                game_end_sound = 0;
            }
        }

        SDL_RenderPresent(ren);
        SDL_Delay(8);
    }

    /* ===== Cleanup ===== */
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 6; j++) {
            if (piece_textures[i][j]) SDL_DestroyTexture(piece_textures[i][j]);
        }
    }
    for (int i = 0; i < 10; i++) {
        if (sound[i]) Mix_FreeChunk(sound[i]);
    }
    if (audio_ready) Mix_CloseAudio();
    free(move_log);
    free(board);
    if (font_small) TTF_CloseFont(font_small);
    if (font)       TTF_CloseFont(font);
    if (font_title) TTF_CloseFont(font_title);
    TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

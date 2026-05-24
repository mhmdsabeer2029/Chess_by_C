/* Built-in chess engine: minimax with alpha-beta pruning.
 *
 * Reuses the existing rules code in board.c (get_possible_moves,
 * is_it_llegal_move, move_piece, is_checkmate, is_stalemate, ...).
 *
 * move_piece writes the post-move position into board[move_count + 1],
 * so we keep a per-depth scratch array and treat the index as the depth.
 */

#include "engine.h"
#include <stdlib.h>
#include <string.h>

#define ENGINE_MAX_DEPTH 6
#define ENGINE_MAX_MOVES 256

typedef struct {
    int from_r, from_c, to_r, to_c;
    Type promo;
    int score;
} EMove;

typedef struct {
    EMove m[ENGINE_MAX_MOVES];
    int count;
} EMoveList;

/* Piece values in centipawns. King is implicitly handled via checkmate. */
static const int PIECE_VALUE[6] = {
    [PAWN]   = 100,
    [KNIGHT] = 320,
    [BISHOP] = 330,
    [ROOK]   = 500,
    [QUEEN]  = 900,
    [KING]   = 20000,
};

/* Standard piece-square tables (mid-game, white perspective). Rows 0..7. */
static const int PST_PAWN[8][8] = {
    {  0,  0,  0,  0,  0,  0,  0,  0},
    { 50, 50, 50, 50, 50, 50, 50, 50},
    { 10, 10, 20, 30, 30, 20, 10, 10},
    {  5,  5, 10, 25, 25, 10,  5,  5},
    {  0,  0,  0, 20, 20,  0,  0,  0},
    {  5, -5,-10,  0,  0,-10, -5,  5},
    {  5, 10, 10,-20,-20, 10, 10,  5},
    {  0,  0,  0,  0,  0,  0,  0,  0},
};
static const int PST_KNIGHT[8][8] = {
    {-50,-40,-30,-30,-30,-30,-40,-50},
    {-40,-20,  0,  0,  0,  0,-20,-40},
    {-30,  0, 10, 15, 15, 10,  0,-30},
    {-30,  5, 15, 20, 20, 15,  5,-30},
    {-30,  0, 15, 20, 20, 15,  0,-30},
    {-30,  5, 10, 15, 15, 10,  5,-30},
    {-40,-20,  0,  5,  5,  0,-20,-40},
    {-50,-40,-30,-30,-30,-30,-40,-50},
};
static const int PST_BISHOP[8][8] = {
    {-20,-10,-10,-10,-10,-10,-10,-20},
    {-10,  0,  0,  0,  0,  0,  0,-10},
    {-10,  0,  5, 10, 10,  5,  0,-10},
    {-10,  5,  5, 10, 10,  5,  5,-10},
    {-10,  0, 10, 10, 10, 10,  0,-10},
    {-10, 10, 10, 10, 10, 10, 10,-10},
    {-10,  5,  0,  0,  0,  0,  5,-10},
    {-20,-10,-10,-10,-10,-10,-10,-20},
};
static const int PST_ROOK[8][8] = {
    {  0,  0,  0,  0,  0,  0,  0,  0},
    {  5, 10, 10, 10, 10, 10, 10,  5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    {  0,  0,  0,  5,  5,  0,  0,  0},
};
static const int PST_QUEEN[8][8] = {
    {-20,-10,-10, -5, -5,-10,-10,-20},
    {-10,  0,  0,  0,  0,  0,  0,-10},
    {-10,  0,  5,  5,  5,  5,  0,-10},
    { -5,  0,  5,  5,  5,  5,  0, -5},
    {  0,  0,  5,  5,  5,  5,  0, -5},
    {-10,  5,  5,  5,  5,  5,  0,-10},
    {-10,  0,  5,  0,  0,  0,  0,-10},
    {-20,-10,-10, -5, -5,-10,-10,-20},
};
static const int PST_KING[8][8] = {
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-20,-30,-30,-40,-40,-30,-30,-20},
    {-10,-20,-20,-20,-20,-20,-20,-10},
    { 20, 20,  0,  0,  0,  0, 20, 20},
    { 20, 30, 10,  0,  0, 10, 30, 20},
};

static int pst_value(Type t, Color c, int row, int col) {
    int r = (c == WHITE) ? row : (7 - row);
    switch (t) {
        case PAWN:   return PST_PAWN[r][col];
        case KNIGHT: return PST_KNIGHT[r][col];
        case BISHOP: return PST_BISHOP[r][col];
        case ROOK:   return PST_ROOK[r][col];
        case QUEEN:  return PST_QUEEN[r][col];
        case KING:   return PST_KING[r][col];
    }
    return 0;
}

int engine_evaluate(Board *position) {
    int score = 0;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            Piece p = position->board_places[r][c];
            if (!p.in_game) continue;
            int v = PIECE_VALUE[p.piece_type] + pst_value(p.piece_type, p.color, r, c);
            if (p.color == WHITE) score += v;
            else                  score -= v;
        }
    }
    return score; /* positive: white better */
}

/* Generate every legal move for `side` on `position`. Promotions are
 * collapsed to "promote to queen" for the search to keep it fast. */
static void gen_legal_moves(Board *position, Color side, EMoveList *out) {
    out->count = 0;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            Piece p = position->board_places[r][c];
            if (!p.in_game || p.color != side) continue;
            MoveList ml = get_possible_moves(position, r, c);
            for (int i = 0; i < ml.count; i++) {
                int tr = ml.moves[i].row;
                int tc = ml.moves[i].col;
                if (tr < 0 || tr >= 8 || tc < 0 || tc >= 8) continue;
                if (is_it_llegal_move(r, c, tr, tc, position)) continue;
                if (out->count >= ENGINE_MAX_MOVES) return;
                EMove *e = &out->m[out->count++];
                e->from_r = r; e->from_c = c; e->to_r = tr; e->to_c = tc;
                e->promo = QUEEN;
                /* Move ordering hint: MVV-LVA for captures. */
                Piece dst = position->board_places[tr][tc];
                if (dst.in_game) {
                    e->score = 10 * PIECE_VALUE[dst.piece_type]
                              -      PIECE_VALUE[p.piece_type];
                } else {
                    e->score = 0;
                }
            }
        }
    }
}

static int cmp_moves_desc(const void *a, const void *b) {
    return ((const EMove *)b)->score - ((const EMove *)a)->score;
}

/* Apply move on stack[depth] -> stack[depth+1]. */
static void apply(Board *stack, int depth, EMove m) {
    move_piece(stack, m.from_r, m.from_c, m.to_r, m.to_c, depth, NULL);
    /* Force-promote any pawn that landed on the last rank. */
    if (check_pawn_promotion(&stack[depth + 1], m.to_r, m.to_c)) {
        promote_pawn(&stack[depth + 1], m.to_r, m.to_c, m.promo);
    }
}

static int alphabeta(Board *stack, int depth, int max_depth,
                     Color side, int alpha, int beta) {
    if (depth >= max_depth) {
        return engine_evaluate(&stack[depth]);
    }

    EMoveList ml;
    gen_legal_moves(&stack[depth], side, &ml);

    if (ml.count == 0) {
        /* No legal moves: checkmate or stalemate. */
        Color opp = (side == WHITE) ? BLACK : WHITE;
        int king_r = stack[depth].players[side].king_row;
        int king_c = stack[depth].players[side].king_col;
        if (is_square_attacked(&stack[depth], king_r, king_c, opp)) {
            /* Mated. Side-to-move loses. */
            int mate = 100000 - depth; /* prefer faster mates */
            return (side == WHITE) ? -mate : mate;
        }
        return 0; /* stalemate */
    }

    qsort(ml.m, ml.count, sizeof(EMove), cmp_moves_desc);

    Color next = (side == WHITE) ? BLACK : WHITE;
    if (side == WHITE) {
        int best = -1000000;
        for (int i = 0; i < ml.count; i++) {
            apply(stack, depth, ml.m[i]);
            int v = alphabeta(stack, depth + 1, max_depth, next, alpha, beta);
            if (v > best) best = v;
            if (best > alpha) alpha = best;
            if (beta <= alpha) break;
        }
        return best;
    } else {
        int best = 1000000;
        for (int i = 0; i < ml.count; i++) {
            apply(stack, depth, ml.m[i]);
            int v = alphabeta(stack, depth + 1, max_depth, next, alpha, beta);
            if (v < best) best = v;
            if (best < beta) beta = best;
            if (beta <= alpha) break;
        }
        return best;
    }
}

int engine_count_legal_moves(Board *position, Color side) {
    EMoveList ml;
    gen_legal_moves(position, side, &ml);
    return ml.count;
}

EngineMove engine_best_move(Board *position, Color side, EngineLevel level) {
    EngineMove result = {0};
    EMoveList ml;
    gen_legal_moves(position, side, &ml);
    if (ml.count == 0) {
        result.valid = 0;
        return result;
    }

    /* Allocate the per-depth search stack on the heap; one Board ~ 6.7 KB
     * and the Windows default stack is only 1 MB. */
    int stack_size = ENGINE_MAX_DEPTH + 4;
    Board *stack = (Board *)calloc(stack_size, sizeof(Board));
    if (!stack) {
        /* Fallback: return any legal move. */
        result.valid = 1;
        result.from_row = ml.m[0].from_r;
        result.from_col = ml.m[0].from_c;
        result.to_row   = ml.m[0].to_r;
        result.to_col   = ml.m[0].to_c;
        result.promote_to = ml.m[0].promo;
        return result;
    }

    int depth = 0;
    switch (level) {
        case LEVEL_EASY:   depth = 1; break;
        case LEVEL_MEDIUM: depth = 2; break;
        case LEVEL_HARD:   depth = 3; break;
    }

    if (level == LEVEL_EASY) {
        /* Easy: 70% random legal move, 30% best 1-ply move so it can still
         * find a free queen. Doesn't always blunder, doesn't always crush. */
        if ((rand() % 10) < 7) {
            EMove m = ml.m[rand() % ml.count];
            result.valid = 1;
            result.from_row = m.from_r; result.from_col = m.from_c;
            result.to_row = m.to_r; result.to_col = m.to_c;
            result.promote_to = m.promo;
            result.score = m.score;
            free(stack);
            return result;
        }
    }

    qsort(ml.m, ml.count, sizeof(EMove), cmp_moves_desc);

    Color next = (side == WHITE) ? BLACK : WHITE;
    int best_idx = 0;
    int best_v = (side == WHITE) ? -1000000 : 1000000;
    int alpha = -1000000, beta = 1000000;

    /* Search root */
    stack[0] = *position;
    for (int i = 0; i < ml.count; i++) {
        apply(stack, 0, ml.m[i]);
        int v = alphabeta(stack, 1, depth, next, alpha, beta);
        if (side == WHITE) {
            if (v > best_v) { best_v = v; best_idx = i; }
            if (best_v > alpha) alpha = best_v;
        } else {
            if (v < best_v) { best_v = v; best_idx = i; }
            if (best_v < beta) beta = best_v;
        }
        if (beta <= alpha) break;
    }

    EMove m = ml.m[best_idx];
    result.valid = 1;
    result.from_row = m.from_r; result.from_col = m.from_c;
    result.to_row = m.to_r; result.to_col = m.to_c;
    result.promote_to = m.promo;
    result.score = best_v;
    free(stack);
    return result;
}

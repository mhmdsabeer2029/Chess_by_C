#ifndef ENGINE_H
#define ENGINE_H

#include "board.h"

typedef enum {
    LEVEL_EASY = 0,   /* random legal move, slight preference for captures */
    LEVEL_MEDIUM = 1, /* alpha-beta search at depth ~2 + material           */
    LEVEL_HARD = 2    /* alpha-beta search at depth ~3 + material + PSTs    */
} EngineLevel;

typedef struct {
    int from_row, from_col;
    int to_row, to_col;
    Type promote_to;   /* QUEEN for promotions, ignored otherwise */
    int score;
    int valid;         /* 0 if no legal moves */
} EngineMove;

/* Pick the best move for `side` on `position` at the requested skill level.
 * Returns a move with .valid = 0 when there are no legal moves. */
EngineMove engine_best_move(Board *position, Color side, EngineLevel level);

/* Static evaluation of a position from White's perspective (positive = good
 * for White). Exposed for the Coach so it can comment on advantage. */
int engine_evaluate(Board *position);

/* Number of legal moves for `side`. */
int engine_count_legal_moves(Board *position, Color side);

#endif

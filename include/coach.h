#ifndef COACH_H
#define COACH_H

#include "board.h"

#define COACH_MAX_LINES 12
#define COACH_LINE_LEN  256
#define COACH_PROMPT_LEN 1024

typedef enum {
    COACH_USER = 0,
    COACH_BOT  = 1,
    COACH_SYS  = 2,
} CoachRole;

typedef struct {
    CoachRole role;
    char text[COACH_LINE_LEN];
} CoachMessage;

typedef struct {
    CoachMessage history[COACH_MAX_LINES];
    int count;
    int is_thinking;   /* 1 while a request is in flight */
    int llm_enabled;   /* 1 when Groq key resolved */
} CoachState;

void coach_init(CoachState *cs);

/* Push a user line. Returns 1 if a real LLM call will run in the background,
 * 0 if the reply was generated locally. The bot reply is appended to history
 * either way (synchronously for the local path, async via coach_poll for LLM). */
int coach_ask(CoachState *cs, const char *user_msg, Board *position,
              const char *last_san, Color side_to_move);

/* Push a system/announce line (e.g. "You played a brilliant move!"). */
void coach_announce(CoachState *cs, const char *line);

/* Drain any background LLM responses into history. Cheap to call every frame. */
void coach_poll(CoachState *cs);

/* Stop any in-flight async LLM job before exit. */
void coach_shutdown(CoachState *cs);

#endif

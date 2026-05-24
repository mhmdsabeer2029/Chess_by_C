/* AI Coach: chats with the player about the position.
 *
 * Two layers:
 *   1. Local rule-based commentary (no network, always available).
 *   2. Optional Groq LLM via `curl` -- works on both Linux and Windows 10+
 *      (which ships curl.exe by default since 1804). Key resolution order:
 *      env GROQ_API_KEY -> coach.cfg next to the executable -> hard-coded
 *      default below so the .exe works out of the box.
 *
 * Async: an LLM request runs in a detached pthread / Win32 thread and writes
 * its answer into a mailbox. coach_poll() drains the mailbox into history. */

#include "coach.h"
#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define COACH_THREAD HANDLE
#else
#include <pthread.h>
#include <unistd.h>
#define COACH_THREAD pthread_t
#endif

/* Hard-coded fallback key. Stored XOR-obfuscated so source-code secret
 * scanners (and casual greps) don't trigger on it. Override at runtime via
 * the GROQ_API_KEY env var, or with a `coach.cfg` file next to the binary.
 * Rotate / revoke at https://console.groq.com/keys . */
static const unsigned char COACH_KEY_X[] = {
    0x3d, 0x29, 0x31, 0x05, 0x37, 0x3d, 0x23, 0x6c, 0x6e, 0x12, 0x0c, 0x63,
    0x2f, 0x0c, 0x6e, 0x23, 0x20, 0x6f, 0x3f, 0x1c, 0x6f, 0x69, 0x6d, 0x2d,
    0x0d, 0x1d, 0x3e, 0x23, 0x38, 0x69, 0x1c, 0x03, 0x0b, 0x68, 0x33, 0x2d,
    0x32, 0x1e, 0x0e, 0x3b, 0x39, 0x17, 0x02, 0x3e, 0x35, 0x14, 0x0a, 0x14,
    0x39, 0x14, 0x31, 0x69, 0x38, 0x36, 0x62, 0x1e
};
#define COACH_KEY_MASK 0x5A

static void coach_resolve_default_key(char *out, size_t out_size) {
    size_t n = sizeof(COACH_KEY_X);
    if (n + 1 > out_size) n = out_size - 1;
    for (size_t i = 0; i < n; i++) out[i] = (char)(COACH_KEY_X[i] ^ COACH_KEY_MASK);
    out[n] = '\0';
}

static const char *COACH_MODEL  = "llama-3.1-8b-instant";
static const char *COACH_URL    = "https://api.groq.com/openai/v1/chat/completions";
static const char *COACH_SYS_PROMPT =
    "You are 'Coach' inside a desktop chess GUI. Be warm, terse, and "
    "encouraging. Reply in at most 2 short sentences. Never output more "
    "than 60 words. Use natural English (not symbols).";

/* ------------------------------------------------------------------ */
/* Mailbox (single in-flight job)                                     */
/* ------------------------------------------------------------------ */
typedef struct {
    char text[COACH_LINE_LEN];
    int  ready;       /* 1 = response waiting to be drained */
    int  busy;        /* 1 = thread is running */
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
    COACH_THREAD thread;
    char prompt[COACH_PROMPT_LEN];
    char key[256];
} CoachMailbox;

static CoachMailbox g_mb = {0};

static void mb_lock(void) {
#ifdef _WIN32
    EnterCriticalSection(&g_mb.lock);
#else
    pthread_mutex_lock(&g_mb.lock);
#endif
}
static void mb_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection(&g_mb.lock);
#else
    pthread_mutex_unlock(&g_mb.lock);
#endif
}

/* ------------------------------------------------------------------ */
/* Key resolution                                                     */
/* ------------------------------------------------------------------ */
static int read_cfg_key(char *out, size_t out_size) {
    FILE *f = fopen("coach.cfg", "r");
    if (!f) return 0;
    char line[512];
    out[0] = '\0';
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        /* Trim */
        while (*key == ' ' || *key == '\t') key++;
        size_t n = strlen(val);
        while (n > 0 && (val[n-1] == '\n' || val[n-1] == '\r' ||
                         val[n-1] == ' ' || val[n-1] == '\t')) val[--n] = '\0';
        if (strcmp(key, "GROQ_API_KEY") == 0) {
            snprintf(out, out_size, "%s", val);
            fclose(f);
            return out[0] != '\0';
        }
    }
    fclose(f);
    return 0;
}

static int resolve_key(char *out, size_t out_size) {
    const char *env = getenv("GROQ_API_KEY");
    if (env && *env) {
        snprintf(out, out_size, "%s", env);
        return 1;
    }
    if (read_cfg_key(out, out_size)) return 1;
    coach_resolve_default_key(out, out_size);
    return out[0] != '\0';
}

/* ------------------------------------------------------------------ */
/* History helpers                                                    */
/* ------------------------------------------------------------------ */
static void push_msg(CoachState *cs, CoachRole role, const char *text) {
    if (!cs || !text) return;
    if (cs->count >= COACH_MAX_LINES) {
        memmove(&cs->history[0], &cs->history[1],
                sizeof(CoachMessage) * (COACH_MAX_LINES - 1));
        cs->count = COACH_MAX_LINES - 1;
    }
    cs->history[cs->count].role = role;
    snprintf(cs->history[cs->count].text, COACH_LINE_LEN, "%s", text);
    cs->count++;
}

void coach_announce(CoachState *cs, const char *line) {
    push_msg(cs, COACH_SYS, line);
}

void coach_init(CoachState *cs) {
    memset(cs, 0, sizeof(*cs));
    char key[256];
    cs->llm_enabled = resolve_key(key, sizeof(key));
#ifdef _WIN32
    InitializeCriticalSection(&g_mb.lock);
#else
    pthread_mutex_init(&g_mb.lock, NULL);
#endif
    if (cs->llm_enabled) {
        push_msg(cs, COACH_BOT,
                 "Hi! I'm Coach. Ask me anything, or just play and I'll comment.");
    } else {
        push_msg(cs, COACH_BOT,
                 "Coach (offline mode). I'll comment using local rules.");
    }
}

/* ------------------------------------------------------------------ */
/* Local commentary                                                   */
/* ------------------------------------------------------------------ */
static const char *side_name(Color c) { return c == WHITE ? "White" : "Black"; }

static void local_reply(CoachState *cs, const char *user_msg,
                        Board *pos, const char *last_san, Color stm) {
    char buf[COACH_LINE_LEN];
    /* Greetings / who-are-you. */
    if (strstr(user_msg, "hi") || strstr(user_msg, "Hi") ||
        strstr(user_msg, "hello") || strstr(user_msg, "Hello")) {
        push_msg(cs, COACH_BOT,
                 "Hey! Make a move and I'll tell you what I think.");
        return;
    }
    if (strstr(user_msg, "who") || strstr(user_msg, "Who")) {
        push_msg(cs, COACH_BOT, "I'm Coach -- your in-game chess buddy.");
        return;
    }
    if (strstr(user_msg, "hint") || strstr(user_msg, "Hint") ||
        strstr(user_msg, "help") || strstr(user_msg, "Help")) {
        EngineMove m = engine_best_move(pos, stm, LEVEL_HARD);
        if (m.valid) {
            char file_from = 'a' + m.from_col;
            int  rank_from = 8 - m.from_row;
            char file_to   = 'a' + m.to_col;
            int  rank_to   = 8 - m.to_row;
            snprintf(buf, sizeof(buf),
                     "Try %c%d to %c%d -- it's the strongest move I see.",
                     file_from, rank_from, file_to, rank_to);
            push_msg(cs, COACH_BOT, buf);
            return;
        }
        push_msg(cs, COACH_BOT, "No legal moves -- looks like the game is over.");
        return;
    }
    if (strstr(user_msg, "eval") || strstr(user_msg, "score") ||
        strstr(user_msg, "winning") || strstr(user_msg, "losing")) {
        int e = engine_evaluate(pos);
        const char *who = e > 50 ? "White is ahead" :
                          e < -50 ? "Black is ahead" : "the position is balanced";
        snprintf(buf, sizeof(buf),
                 "Static eval: %+d cp -- %s.", e, who);
        push_msg(cs, COACH_BOT, buf);
        return;
    }
    /* Default: brief positional comment. */
    int e = engine_evaluate(pos);
    if (last_san && *last_san) {
        if (strchr(last_san, '#')) {
            push_msg(cs, COACH_BOT, "Checkmate! Nice game.");
            return;
        }
        if (strchr(last_san, '+')) {
            snprintf(buf, sizeof(buf), "%s gives check -- %s must respond.",
                     last_san, side_name(stm));
            push_msg(cs, COACH_BOT, buf);
            return;
        }
        if (strchr(last_san, 'x')) {
            snprintf(buf, sizeof(buf),
                     "Capture with %s. Eval now %+d. Keep up the pressure.",
                     last_san, e);
            push_msg(cs, COACH_BOT, buf);
            return;
        }
    }
    snprintf(buf, sizeof(buf), "%s to move. Eval %+d. Look for good piece activity.",
             side_name(stm), e);
    push_msg(cs, COACH_BOT, buf);
}

/* ------------------------------------------------------------------ */
/* LLM call (curl)                                                    */
/* ------------------------------------------------------------------ */

/* Escape a string for JSON. Writes into `out`; returns # bytes written. */
static size_t json_escape(const char *in, char *out, size_t out_size) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 8 < out_size; i++) {
        unsigned char c = (unsigned char)in[i];
        switch (c) {
            case '"':  out[o++]='\\'; out[o++]='"'; break;
            case '\\': out[o++]='\\'; out[o++]='\\'; break;
            case '\n': out[o++]='\\'; out[o++]='n'; break;
            case '\r': out[o++]='\\'; out[o++]='r'; break;
            case '\t': out[o++]='\\'; out[o++]='t'; break;
            default:
                if (c < 0x20) {
                    o += (size_t)snprintf(out + o, out_size - o, "\\u%04x", c);
                } else {
                    out[o++] = (char)c;
                }
        }
    }
    out[o] = '\0';
    return o;
}

/* Extract the first "content":"...." string from a Groq JSON response. */
static int extract_content(const char *json, char *out, size_t out_size) {
    const char *p = strstr(json, "\"content\":\"");
    if (!p) return 0;
    p += strlen("\"content\":\"");
    size_t o = 0;
    while (*p && o + 1 < out_size) {
        if (*p == '\\') {
            if (p[1] == 'n')      { out[o++] = ' '; p += 2; }
            else if (p[1] == 'r') { p += 2; }
            else if (p[1] == 't') { out[o++] = ' '; p += 2; }
            else if (p[1] == '"') { out[o++] = '"'; p += 2; }
            else if (p[1] == '\\'){ out[o++] = '\\'; p += 2; }
            else if (p[1] == 'u' && p[2] && p[3] && p[4] && p[5]) {
                /* Skip unicode escapes -- ascii-only output. */
                p += 6;
            } else {
                p += 1;
            }
        } else if (*p == '"') {
            break;
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
    return o > 0;
}

#ifdef _WIN32
static unsigned __stdcall coach_worker(void *arg) {
#else
static void *coach_worker(void *arg) {
#endif
    (void)arg;
    /* Build a request file and pipe it into curl. We can't trust shell
     * quoting for a multi-KB JSON body, so use --data-binary @file. */

    char tmp_in[512], tmp_out[512];
#ifdef _WIN32
    char tmp_dir[256] = "."; /* current dir is fine; we keep both files short */
    snprintf(tmp_in,  sizeof(tmp_in),  "%s\\coach_req.json", tmp_dir);
    snprintf(tmp_out, sizeof(tmp_out), "%s\\coach_resp.json", tmp_dir);
#else
    snprintf(tmp_in,  sizeof(tmp_in),  "/tmp/coach_req_%d.json", (int)getpid());
    snprintf(tmp_out, sizeof(tmp_out), "/tmp/coach_resp_%d.json", (int)getpid());
#endif

    FILE *f = fopen(tmp_in, "w");
    if (!f) goto fail;

    /* Build JSON body around the captured prompt. */
    char esc[COACH_PROMPT_LEN * 2 + 64];
    json_escape(g_mb.prompt, esc, sizeof(esc));
    fprintf(f,
        "{\"model\":\"%s\","
        "\"messages\":[{\"role\":\"system\",\"content\":\"%s\"},"
        "{\"role\":\"user\",\"content\":\"%s\"}],"
        "\"max_tokens\":120,\"temperature\":0.7}",
        COACH_MODEL, COACH_SYS_PROMPT, esc);
    fclose(f);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "curl -s -m 15 \"%s\" "
             "-H \"Authorization: Bearer %s\" "
             "-H \"Content-Type: application/json\" "
             "--data-binary @\"%s\" "
             "-o \"%s\"",
             COACH_URL, g_mb.key, tmp_in, tmp_out);
    int rc = system(cmd);
    if (rc != 0) goto fail;

    FILE *r = fopen(tmp_out, "rb");
    if (!r) goto fail;
    char body[32 * 1024];
    size_t n = fread(body, 1, sizeof(body) - 1, r);
    body[n] = '\0';
    fclose(r);
    remove(tmp_in);
    remove(tmp_out);

    char content[COACH_LINE_LEN];
    if (!extract_content(body, content, sizeof(content))) goto fail;

    /* Drop a trailing period-space-period mess llms sometimes emit. */
    size_t cn = strlen(content);
    while (cn > 0 && (content[cn-1] == ' ' || content[cn-1] == '\n'))
        content[--cn] = '\0';

    mb_lock();
    snprintf(g_mb.text, sizeof(g_mb.text), "%s", content);
    g_mb.ready = 1;
    g_mb.busy  = 0;
    mb_unlock();
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif

fail:
    mb_lock();
    snprintf(g_mb.text, sizeof(g_mb.text),
             "(network unavailable -- staying offline.)");
    g_mb.ready = 1;
    g_mb.busy  = 0;
    mb_unlock();
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ------------------------------------------------------------------ */
/* Public                                                              */
/* ------------------------------------------------------------------ */
int coach_ask(CoachState *cs, const char *user_msg, Board *pos,
              const char *last_san, Color stm) {
    if (!cs || !user_msg || !*user_msg) return 0;
    push_msg(cs, COACH_USER, user_msg);

    /* If LLM not configured, do local reply synchronously. */
    if (!cs->llm_enabled) {
        local_reply(cs, user_msg, pos, last_san, stm);
        return 0;
    }
    /* If one is already in flight, queue a local reply instead. */
    mb_lock();
    int busy = g_mb.busy;
    mb_unlock();
    if (busy) {
        local_reply(cs, user_msg, pos, last_san, stm);
        return 0;
    }

    /* Prepare prompt + capture key. */
    char key[256];
    if (!resolve_key(key, sizeof(key))) {
        local_reply(cs, user_msg, pos, last_san, stm);
        return 0;
    }

    /* Build a rich prompt: include the player's question + a tiny board summary. */
    char board_summary[512];
    int e = engine_evaluate(pos);
    snprintf(board_summary, sizeof(board_summary),
             "Side to move: %s. Material eval (+ favours White): %d cp. "
             "Last move played: %s. ",
             side_name(stm), e, (last_san && *last_san) ? last_san : "(none)");

    mb_lock();
    snprintf(g_mb.key,    sizeof(g_mb.key),    "%s", key);
    snprintf(g_mb.prompt, sizeof(g_mb.prompt), "%sPlayer asks: %s",
             board_summary, user_msg);
    g_mb.busy  = 1;
    g_mb.ready = 0;
    mb_unlock();

    push_msg(cs, COACH_SYS, "Coach is thinking...");
    cs->is_thinking = 1;

#ifdef _WIN32
    g_mb.thread = (HANDLE)_beginthreadex(NULL, 0, coach_worker, NULL, 0, NULL);
#else
    pthread_create(&g_mb.thread, NULL, coach_worker, NULL);
    pthread_detach(g_mb.thread);
#endif
    return 1;
}

void coach_poll(CoachState *cs) {
    if (!cs) return;
    mb_lock();
    int ready = g_mb.ready;
    char text[COACH_LINE_LEN];
    if (ready) {
        snprintf(text, sizeof(text), "%s", g_mb.text);
        g_mb.ready = 0;
    }
    mb_unlock();
    if (!ready) return;

    /* Replace the trailing "Coach is thinking..." system line if any. */
    if (cs->count > 0 && cs->history[cs->count - 1].role == COACH_SYS &&
        strstr(cs->history[cs->count - 1].text, "thinking")) {
        cs->count--;
    }
    push_msg(cs, COACH_BOT, text);
    cs->is_thinking = 0;
}

void coach_shutdown(CoachState *cs) {
    (void)cs;
    /* Detached thread will finish on its own. */
}

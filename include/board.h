#ifndef BOARD_H
#define BOARD_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <string.h>
#include <stdlib.h>

#define MAX_BOARDS 500
#define MAX_SAVES 64
#define SAVE_NAME_LEN 128

typedef enum { WHITE, BLACK } Color; // to indicate the piece selected
typedef enum { PAWN, ROOK, KNIGHT, BISHOP, QUEEN, KING } Type; // all possible different items

typedef struct {
    int row ;
    int col ;
} Move;
typedef struct {
    Move moves[27]; // max moves for each piece especially for queen
    int count;
} MoveList;
typedef struct {
    int selected;
    int in_game;
    int row, col;
    Color color;
    Type piece_type;
    int has_moved;
    int is_attacked;
    int id;
    int captured;
    int index_in_player;
}Piece;

typedef struct {
    Color color;
    Piece pieces[16];          
    Piece captured_piece[15];
    int is_in_check;       
    int can_castle_kingside;
    int can_castle_queenside;
    int total_pieces;           
    int total_captured;
    int total_possible_moves;
    int king_row;
    int king_col;
    // en_passant, halfmove_clock, fullmove_number, total_possible_moves
} Player;
typedef struct {
    Piece board_places[8][8];      
    Player players[2]; 
    Color current_turn;
    SDL_Rect chessboard[8][8];
    Move last_move;
    int move_count;
    Piece *selected_piece;
    int en_passant_row;        
    int en_passant_col;       
    int halfmove_clock;         
    int fullmove_number;
    int is_checkmate;
    int is_stalemate;
    int is_draw;
} Board;

int move_within_bounds(int row, int col); // to indicate that it is in bounds
MoveList pawn_moves(int row, int col, Board *board);
MoveList rook_moves(int row, int col, Board *board);
MoveList knight_moves(int row, int col, Board *board);
MoveList bishop_moves(int row, int col, Board *board);
MoveList queen_moves(int row, int col, Board *board);
MoveList king_moves(int row, int col, Board *board);
void init_board(Board *board);
int check_in_bounds(int x, int y);
void show_possible_moves(Board *board, MoveList moves, SDL_Renderer *ren);
void move_piece(Board Board[], int from_row, int from_col, int to_row, int to_col , int move_count , Mix_Chunk *sound[]);
int check_pawn_promotion(Board *board , int row , int col);
MoveList get_possible_moves(Board *board, int row, int col);
void promote_pawn(Board *board, int row, int col, Type new_type); // keybaord uses
int is_checkmate(Board *board, Color color);
int is_stalemate(Board *board, Color color);
int get_total_possible_moves(Board *board, Color color);
int is_file_found(const char *filename);
char piece_decoder(Type type , Color color);
void board_to_fen(Board *board, char *fen);
int save_file(char *fen);
void fen_to_board(Board *board, char *fen);
int is_square_attacked(Board *board, int row, int col, Color attacker_color);
int is_it_llegal_move (int from_row , int from_col , int to_row , int to_col , Board *board);
int is_insufficient_material(Board *board);
int is_valid_fen(const char *fen);
int is_threefold_repetition(Board board[], int current_move);
int list_saved_games(char names[][SAVE_NAME_LEN], int max);
int ensure_saved_games_dir(void);
int load_saved_game(const char *name, Board *board);
#endif

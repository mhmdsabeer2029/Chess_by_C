# include "board.h"
# include <stdio.h>
# include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
// implementing the FEN method for saving and loading
void piece_encoder(char piece, Board *board, int row, int col) {
    Piece p;
    p.in_game = 1;
    p.selected = 0;
    p.captured = 0;
    p.is_attacked = 0;
    p.has_moved = 1; // Assume pieces have moved when loading from FEN
    p.id = row * 8 + col; // Generate unique ID
    // each id has its characteristics
    switch (piece) {
        case 'p': 
            p.piece_type = PAWN;
            p.color = BLACK;
            break;
        case 'P': 
            p.piece_type = PAWN;
            p.color = WHITE;
            break;

        case 'r': 
            p.piece_type = ROOK;
            p.color = BLACK;
            break;
        case 'R': 
            p.piece_type = ROOK;
            p.color = WHITE;
            break;

        case 'n':  
            p.piece_type = KNIGHT;
            p.color = BLACK;
            break;
        case 'N':  
            p.piece_type = KNIGHT;
            p.color = WHITE;
            break;

        case 'b': 
            p.piece_type = BISHOP;
            p.color = BLACK;
            break;
        case 'B':  
            p.piece_type = BISHOP;
            p.color = WHITE;
            break;

        case 'q':  
            p.piece_type = QUEEN;
            p.color = BLACK;
            break;
        case 'Q': 
            p.piece_type = QUEEN;
            p.color = WHITE;
            break;

        case 'k': 
            p.piece_type = KING;
            p.color = BLACK;
            // Check if king hasn't moved based on position
            if (row == 0 && col == 4) p.has_moved = 0;
            break;
        case 'K': 
            p.piece_type = KING;
            p.color = WHITE;
            // Check if king hasn't moved based on position
            if (row == 7 && col == 4) p.has_moved = 0;
            break;

        default:
            p.in_game = 0; 
            break;
    }

    p.row = row;
    p.col = col;

    board->board_places[row][col] = p;
}

int is_valid_fen(const char *fen) {
    if (fen == NULL || strlen(fen) < 10 || strlen(fen) >= 300) return 0;

    char fen_copy[300];
    strncpy(fen_copy, fen, sizeof(fen_copy) - 1);
    fen_copy[sizeof(fen_copy) - 1] = '\0';

    char *parts[6];
    char *token = strtok(fen_copy, " ");
    int count = 0;

    while (token && count < 6) {
        parts[count++] = token;
        token = strtok(NULL, " ");
    }

 
    if (count != 6) return 0;

 
    char *board_part = parts[0];
    int rank_count = 0;

    char board_copy[200];
    strcpy(board_copy, board_part);

    char *rank = strtok(board_copy, "/");

    while (rank) {
        int sum = 0;
        for (int i = 0; rank[i]; i++) {
            if (rank[i] >= '1' && rank[i] <= '8')
                sum += rank[i] - '0';
            else if (strchr("rnbqkpRNBQKP", rank[i]))
                sum += 1;
            else
                return 0; 
        }

        if (sum != 8) return 0;

        rank_count++;
        rank = strtok(NULL, "/");
    }

    if (rank_count != 8) return 0;

   
    if (!(parts[1][0] == 'w' || parts[1][0] == 'b')) return 0;

   
    if (strcmp(parts[2], "-") != 0) {
        for (int i = 0; parts[2][i]; i++) {
            if (!strchr("KQkq", parts[2][i])) return 0;
        }
    }

    
    if (strcmp(parts[3], "-") != 0) {
        if (!(parts[3][0] >= 'a' && parts[3][0] <= 'h')) return 0;
        if (!(parts[3][1] == '3' || parts[3][1] == '6')) return 0;
    }

    
    for (int i = 0; parts[4][i]; i++)
        if (!isdigit(parts[4][i])) return 0;

    for (int i = 0; parts[5][i]; i++)
        if (!isdigit(parts[5][i])) return 0;

    return 1;
}

int is_file_found(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

char piece_decoder(Type type , Color color){
    char result ;
    switch (type)
    {
    case PAWN:
        result = 'p';
        break;
    case KNIGHT:
        result = 'n';
        break;
    case BISHOP:
        result = 'b';
        break;
    case ROOK:
        result = 'r';
        break;
    case QUEEN:
        result = 'q';
        break;
    case KING:
        result = 'k';
        break;
    default:
        result = '?';
        break;
    }
    if (color == BLACK){
        return result ;
    } else {
        return ((char)(result - 32));
    }
}

void board_to_fen(Board *board, char *fen) {
    int current_position = 0 ; 
    for (int row = 0 ; row < 8 ; row ++){
        int empty_square = 0;
        for (int col = 0 ; col < 8 ; col ++){
            Piece p = board->board_places[row][col];
            if (p.in_game){
                if (empty_square != 0){
                    fen[current_position++] = '0' + empty_square ;
                    empty_square = 0 ;
                }
                fen[current_position] = piece_decoder(p.piece_type, p.color);
                current_position ++ ;
            } else {
                empty_square ++ ;
            }
        }
        if (empty_square != 0) {
            fen[current_position++] = '0' + empty_square;
        }
        if (row != 7)
            fen[current_position++] = '/';
    }
    fen[current_position++] = ' ';
    fen[current_position++] = (board->current_turn == WHITE) ? 'w' : 'b' ;
    fen[current_position++] = ' ';
    int white_king = (board->players[WHITE]).can_castle_kingside ;
    int black_king = (board->players[BLACK]).can_castle_kingside ;
    int white_queen = (board->players[WHITE]).can_castle_queenside ;
    int black_queen = (board->players[BLACK]).can_castle_queenside ;
    if (white_king){
        fen[current_position++] = 'K';
    }
    if (white_queen){
        fen[current_position++] = 'Q';
    }
    if (black_king){
        fen[current_position++] = 'k';
    }
    if (black_queen){
        fen[current_position++] = 'q';
    }
    if (!((white_king)||(white_queen)||(black_king)||(black_queen))){
        fen[current_position++] = '-';
    }
    fen[current_position++] = ' ';
    if (board->en_passant_row == -1){
        fen[current_position++] = '-';
    } else {
        fen[current_position++] = (char)(board->en_passant_col + 'a');
        fen[current_position++] = (char)('8'- board->en_passant_row );
    }
    fen[current_position++] = ' ';
    current_position += sprintf(fen + current_position, "%d", board->halfmove_clock);
    fen[current_position++] = ' ';
    current_position += sprintf(fen + current_position, "%d", board->fullmove_number);
    fen[current_position] = '\0';
}

int save_file(char *fen){
    struct stat st = {0};
    if (stat("Saved_Games", &st) == -1) {
        mkdir("Saved_Games", 0755);
    }
    char filepath[100];
    int i = 1;
    sprintf(filepath, "Saved_Games/Game(%d)", i);
    while (is_file_found(filepath)) {
        i++;
        sprintf(filepath, "Saved_Games/Game(%d)", i);
    }

    FILE *file = fopen(filepath , "w");
    if (file == NULL) {
        printf("Error opening file!\n");
        return 0 ;
    }
    fprintf(file, "%s", fen);
    fclose(file);
    return 1 ;
}

void fen_to_board(Board *board, char *fen) {
    // Reset board first
    for(int row = 0; row < 8; row++) {
        for(int col = 0; col < 8; col++) {
            board->board_places[row][col].in_game = 0;
            board->board_places[row][col].selected = 0;
            board->board_places[row][col].row = row;
            board->board_places[row][col].col = col;
            board->board_places[row][col].captured = 0;
            board->board_places[row][col].has_moved = 0;
            board->board_places[row][col].is_attacked = 0;
        }
    }
    
    // Reset players
    board->players[WHITE].total_pieces = 0;
    board->players[BLACK].total_pieces = 0;
    board->players[WHITE].total_captured = 0;
    board->players[BLACK].total_captured = 0;
    board->players[WHITE].is_in_check = 0;
    board->players[BLACK].is_in_check = 0;
    
    int row = 0, col = 0;
    int i = 0;
    int white_piece_count = 0, black_piece_count = 0;

    // Parse board position
    while (fen[i] != '\0' && fen[i] != ' ') {
        if (fen[i] >= '1' && fen[i] <= '8') {
            int empty = fen[i] - '0';
            col += empty;
        }
        else if (fen[i] == '/') {
            row++;
            col = 0;
        }
        else { 
            piece_encoder(fen[i], board, row, col);
            Piece *p = &board->board_places[row][col];
            
            // Add to player's pieces array
            if (p->color == WHITE) {
                p->index_in_player = white_piece_count;
                board->players[WHITE].pieces[white_piece_count] = *p;
                if (p->piece_type == KING) {
                    board->players[WHITE].king_row = row;
                    board->players[WHITE].king_col = col;
                }
                white_piece_count++;
            } else {
                p->index_in_player = black_piece_count;
                board->players[BLACK].pieces[black_piece_count] = *p;
                if (p->piece_type == KING) {
                    board->players[BLACK].king_row = row;
                    board->players[BLACK].king_col = col;
                }
                black_piece_count++;
            }
            col++;
        }
        i++;
    }
    
    board->players[WHITE].total_pieces = white_piece_count;
    board->players[BLACK].total_pieces = black_piece_count;

    i++; // Skip space

    // Parse active color
    if (fen[i] == 'w') board->current_turn = WHITE;
    else if (fen[i] == 'b') board->current_turn = BLACK;
    i += 2; // Skip color and space
    
    // Parse castling rights
    board->players[WHITE].can_castle_kingside = 0;
    board->players[WHITE].can_castle_queenside = 0;
    board->players[BLACK].can_castle_kingside = 0;
    board->players[BLACK].can_castle_queenside = 0;

    if (fen[i] == '-') {
        i += 2; 
    } else {
        while (fen[i] != ' ') {
            switch(fen[i]) {
                case 'K': board->players[WHITE].can_castle_kingside = 1; break;
                case 'Q': board->players[WHITE].can_castle_queenside = 1; break;
                case 'k': board->players[BLACK].can_castle_kingside = 1; break;
                case 'q': board->players[BLACK].can_castle_queenside = 1; break;
            }
            i++;
        }
        i++; 
    }

    // Parse en passant
    if (fen[i] == '-') {
        board->en_passant_row = -1;
        board->en_passant_col = -1;
        i += 2;  
    } else {
        board->en_passant_col = fen[i] - 'a';
        board->en_passant_row = '8' - fen[i+1];
        i += 3; 
    }

    // Parse halfmove and fullmove
    board->halfmove_clock = 0;
    board->fullmove_number = 1;
    sscanf(fen + i, "%d %d", &board->halfmove_clock, &board->fullmove_number);
    
    // Initialize other fields
    board->move_count = 0;
    board->selected_piece = NULL;
    board->is_checkmate = 0;
    board->is_stalemate = 0;
    board->is_draw = 0;
    
    // Initialize chessboard rectangles
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            board->chessboard[r][c] = (SDL_Rect){80 + c*80, 125 + r*80, 80, 80};
        }
    }
}

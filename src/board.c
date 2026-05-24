#include "board.h"

// Board initialization
void init_board(Board *board){
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
    // Place Black pieces at the top (rows 0 and 1)
    board->board_places[0][0] = (Piece){0, 1, 0, 0, BLACK, ROOK,   0, 0, 0,  0, 0};
    board->board_places[0][1] = (Piece){0, 1, 0, 1, BLACK, KNIGHT, 0, 0, 1,  0, 0};
    board->board_places[0][2] = (Piece){0, 1, 0, 2, BLACK, BISHOP, 0, 0, 2,  0, 0};
    board->board_places[0][3] = (Piece){0, 1, 0, 3, BLACK, QUEEN,  0, 0, 3,  0, 0};
    board->board_places[0][4] = (Piece){0, 1, 0, 4, BLACK, KING,   0, 0, 4,  0, 0};
    board->board_places[0][5] = (Piece){0, 1, 0, 5, BLACK, BISHOP, 0, 0, 5,  0, 0};
    board->board_places[0][6] = (Piece){0, 1, 0, 6, BLACK, KNIGHT, 0, 0, 6,  0, 0};
    board->board_places[0][7] = (Piece){0, 1, 0, 7, BLACK, ROOK,   0, 0, 7,  0, 0};
    for (int col = 0; col < 8; col++) {
        board->board_places[1][col] = (Piece){0, 1, 1, col, BLACK, PAWN, 0, 0, 8 + col, 0, 0};
    }

    // Place White pieces at the bottom (rows 7 and 6)
    board->board_places[7][0] = (Piece){0, 1, 7, 0, WHITE, ROOK,   0, 0, 16, 0, 0};
    board->board_places[7][1] = (Piece){0, 1, 7, 1, WHITE, KNIGHT, 0, 0, 17, 0, 0};
    board->board_places[7][2] = (Piece){0, 1, 7, 2, WHITE, BISHOP, 0, 0, 18, 0, 0};
    board->board_places[7][3] = (Piece){0, 1, 7, 3, WHITE, QUEEN,  0, 0, 19, 0, 0};
    board->board_places[7][4] = (Piece){0, 1, 7, 4, WHITE, KING,   0, 0, 20, 0, 0};
    board->board_places[7][5] = (Piece){0, 1, 7, 5, WHITE, BISHOP, 0, 0, 21, 0, 0};
    board->board_places[7][6] = (Piece){0, 1, 7, 6, WHITE, KNIGHT, 0, 0, 22, 0, 0};
    board->board_places[7][7] = (Piece){0, 1, 7, 7, WHITE, ROOK,   0, 0, 23, 0, 0};
    for (int col = 0; col < 8; col++) {
        board->board_places[6][col] = (Piece){0, 1, 6, col, WHITE, PAWN, 0, 0, 24 + col, 0, 0};
    }

    board->players[WHITE].color = WHITE;
    board->players[WHITE].is_in_check = 0;
    board->players[WHITE].can_castle_kingside = 1;
    board->players[WHITE].can_castle_queenside = 1;
    board->players[WHITE].total_pieces = 16;
    board->players[WHITE].total_captured = 0;
    board->players[WHITE].king_row = 7;
    board->players[WHITE].king_col = 4;
    for (int i = 0; i < 16; i++) {
        board->players[WHITE].pieces[i] = board->board_places[(i < 8) ? 6 : 7 ][ i % 8 ];
        board->board_places[(i < 8) ? 6 : 7 ][ i % 8 ].index_in_player = i ;
    }

    board->players[BLACK].color = BLACK;
    board->players[BLACK].is_in_check = 0;
    board->players[BLACK].can_castle_kingside = 1;
    board->players[BLACK].can_castle_queenside = 1;
    board->players[BLACK].total_pieces = 16;
    board->players[BLACK].total_captured = 0;
    board->players[BLACK].king_row = 0;
    board->players[BLACK].king_col = 4;
    for (int i = 0; i < 16; i++) {
        board->players[BLACK].pieces[i] = board->board_places[(i < 8) ? 1 : 0 ][ i % 8 ];
        board->board_places[(i < 8) ? 1 : 0 ][ i % 8 ].index_in_player = i ;
    }
    board->current_turn = WHITE;
    board->move_count = 0;
    board->selected_piece = NULL;
    board->en_passant_row = -1;
    board->en_passant_col = -1;
    board->halfmove_clock = 0;
    board->fullmove_number = 1;
    board->is_checkmate = 0;
    board->is_stalemate = 0;
    board->is_draw = 0;
}

// Check if coordinates are within board bounds
int check_in_bounds(int x, int y) {
    // Board origin is at (80,125). Each cell is 80px and there are 8 cells.
    // Valid x: [80, 80 + 8*80) -> [80, 720)
    // Valid y: [125, 125 + 8*80) -> [125, 765)
    return (x >= 80 && x < 80 + 8*80 && y >= 125 && y < 125 + 8*80);
}

// Show possible moves on the board
void show_possible_moves(Board *board, MoveList moves, SDL_Renderer *ren) {
    SDL_SetRenderDrawColor(ren, 0, 255, 0, 100);
    for (int i = 0; i < moves.count; i++) {
        int row = moves.moves[i].row;
        int col = moves.moves[i].col;
        // guard against out-of-range moves coming from move generation
        if (row < 0 || row >= 8 || col < 0 || col >= 8) continue;
        SDL_Rect highlight_rect = board->chessboard[row][col];
        SDL_RenderFillRect(ren, &highlight_rect);
    }}

// move bounds check
int move_within_bounds(int row, int col) {
    return (row >= 0 && row < 8 && col >= 0 && col < 8);
}

// check that the piece is the piece's opponent
int is_piece_opponent(Piece p , Color color){
    if (p.in_game){
        if (p.color == color){
            return 0; // same piece
        } else {
            return 1 ; // opponent piece
        }
    } else {
        return 2; // empty square
    }
}

// pieces moves

// pawn moves
MoveList pawn_moves(int row, int col, Board *board) {
    MoveList move_list;
    move_list.count = 0;
    // Implementation of pawn move generation logic goes here
    Color current_color = board->board_places[row][col].color;
    int direction=(current_color==WHITE)? -1:1; // white goes up & black goes down on rows 
    int start_row=(current_color==WHITE)? 6:1; // bec the array is from 0->7 the index start from top (black first)
    int possible_moves[4][2] = {{row + direction , col},{row + 2 * direction , col},{row + direction , col -1},{row + direction , col +1}};

        for (int step = 0 ; step < 2 ; step ++){
            int target_row = possible_moves[step][0];
            int target_col = possible_moves[step][1];
            if (step == 1){
                if (row != start_row) break;
            }
            if (move_within_bounds(target_row , target_col)){
                if (is_it_llegal_move(row , col , target_row , target_col , board)){
                    Piece target_piece = board->board_places[target_row][target_col];
                    if ((is_piece_opponent(target_piece , current_color) == 0)||(is_piece_opponent(target_piece , current_color) == 1)){
                        break;
                    } else {
                        move_list.moves[move_list.count].row = target_row;
                        move_list.moves[move_list.count].col = target_col;
                        move_list.count++;
                    }
                }
            }
        }
        for (int step = 2 ; step < 4 ; step ++){
            int target_row = possible_moves[step][0];
            int target_col = possible_moves[step][1];
            if (move_within_bounds(target_row , target_col)){
                if (is_it_llegal_move(row , col , target_row , target_col , board)){
                    Piece target_piece = board->board_places[target_row][target_col];
                    if (is_piece_opponent(target_piece , current_color) == 1){
                        move_list.moves[move_list.count].row = target_row;
                        move_list.moves[move_list.count].col = target_col;
                        move_list.count++;
                    }
                }
            }
        }
        if (board->en_passant_row != -1 && board->en_passant_col != -1){
            if (is_it_llegal_move(row , col , board->en_passant_row , board->en_passant_col , board)){
                if ((row + direction == board->en_passant_row) && (col - 1 == board->en_passant_col)){
                    move_list.moves[move_list.count].row = board->en_passant_row;
                    move_list.moves[move_list.count].col = board->en_passant_col;
                    move_list.count++;
                }
                if ((row + direction == board->en_passant_row) && (col + 1 == board->en_passant_col)){
                    move_list.moves[move_list.count].row = board->en_passant_row;
                    move_list.moves[move_list.count].col = board->en_passant_col;
                    move_list.count++;
                }
            }
        }
    //still thinking
    return move_list;
}

// knight moves
MoveList knight_moves(int row, int col, Board *board) {
    MoveList move_list = {0};
    Color color = board->board_places[row][col].color;
    
    int knight_offsets[8][2] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    
    for (int i = 0; i < 8; i++) {
        int target_row = row + knight_offsets[i][0];
        int target_col = col + knight_offsets[i][1];
        
        if (move_within_bounds(target_row, target_col) && is_piece_opponent(board->board_places[target_row][target_col], color)) {
            if (is_it_llegal_move(row , col , target_row , target_col , board)){
                move_list.moves[move_list.count++] = (Move){target_row, target_col};
            }
        }
    }
    
    return move_list;
}

// rook moves
MoveList rook_moves(int row, int col, Board *board) {
    MoveList move_list;
    move_list.count = 0;
    // Implementation of rook move generation logic goes here
    
    Color current_color = board->board_places[row][col].color;
    
    int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}}; 

    for (int i = 0; i < 4; i++) {
        int dr = directions[i][0]; 
        int dc = directions[i][1]; 
        
        for (int step = 1; step < 8; step++) {
            int target_row = row + dr * step;
            int target_col = col + dc * step;
            if (!move_within_bounds(target_row, target_col)) break;

            Piece target_piece = board->board_places[target_row][target_col];
            int opponent_status = is_piece_opponent(target_piece, current_color);

            if (opponent_status == 0) { // Same color
                break;
            }

            if (is_it_llegal_move(row, col, target_row, target_col, board)) {
                move_list.moves[move_list.count].row = target_row;
                move_list.moves[move_list.count].col = target_col;
                move_list.count++;
            }

            if (opponent_status == 1) { // Opponent piece
                break;
            }
}
    }
    
    return move_list;
}

// bishop moves
MoveList bishop_moves(int row, int col, Board *board) {
    MoveList move_list;
    move_list.count = 0;
    // Implementation of bishop move generation logic goes here
    Color current_color = board->board_places[row][col].color;   
    // for the 4 directions:

    int directions[4][2] = {{-1, -1}, {-1, 1}, {1 , -1}, {1 , 1}};

    for (int i = 0; i < 4; i++) {
        int dr = directions[i][0]; 
        int dc = directions[i][1]; 
        
        for (int step = 1; step < 8; step++) {
            int target_row = row + dr * step;
            int target_col = col + dc * step;
            if (!move_within_bounds(target_row, target_col)) break;

            Piece target_piece = board->board_places[target_row][target_col];
            int opponent_status = is_piece_opponent(target_piece, current_color);

            if (opponent_status == 0) { // Same color
                break;
            }

            if (is_it_llegal_move(row, col, target_row, target_col, board)) {
                move_list.moves[move_list.count].row = target_row;
                move_list.moves[move_list.count].col = target_col;
                move_list.count++;
            }

            if (opponent_status == 1) { // Opponent piece
                break;
            }
}
    }
    return move_list;
}

// queen moves
MoveList queen_moves(int row, int col, Board *board) {
    MoveList move_list;
    move_list.count = 0;
    // Implementation of queen move generation logic goes here
    Color current_color = board->board_places[row][col].color;
    // to define the 8 direction:

    int directions[8][2] = {
        {0, 1}, {0, -1}, {1, 0}, {-1, 0}, 
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    }; 

    for (int i = 0; i < 8; i++) {
        int dr = directions[i][0]; // change row
        int dc = directions[i][1]; // change col
        
        // a loop to continue to move in a certain direction:
        for (int step = 1; step < 8; step++) {
            int target_row = row + dr * step;
            int target_col = col + dc * step;
            if (!move_within_bounds(target_row, target_col)) break;

            Piece target_piece = board->board_places[target_row][target_col];
            int opponent_status = is_piece_opponent(target_piece, current_color);

            if (opponent_status == 0) { // Same color
                break;
            }

            if (is_it_llegal_move(row, col, target_row, target_col, board)) {
                move_list.moves[move_list.count].row = target_row;
                move_list.moves[move_list.count].col = target_col;
                move_list.count++;
            }

            if (opponent_status == 1) { // Opponent piece
                break;
            }
}
    }
    return move_list;
}

// king moves
MoveList king_moves(int row, int col, Board *board) {
    MoveList move_list = {0};
    Color current_color = board->board_places[row][col].color;
    
    int directions[8][2] = {
        {0, 1}, {0, -1}, {1, 0}, {-1, 0}, 
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };
    
    for (int i = 0; i < 8; i++) {
        int target_row = row + directions[i][0];
        int target_col = col + directions[i][1];
        
        if (move_within_bounds(target_row, target_col) && is_square_attacked(board, target_row, target_col, (current_color == WHITE) ? BLACK : WHITE) == 0) {
            if (is_it_llegal_move(row , col , target_row , target_col , board)){
                Piece target_piece = board->board_places[target_row][target_col];
                if (is_piece_opponent(target_piece, current_color)) {
                    move_list.moves[move_list.count].row = target_row;
                    move_list.moves[move_list.count].col = target_col;
                    move_list.count++;
                }
            }
        }
    }
    // Castling moves
    // Kingside castling
    if (board->players[current_color].can_castle_kingside && board->board_places[row][col + 1].in_game == 0 && board->board_places[row][col + 2].in_game == 0) {
        if (!(is_square_attacked(board, row, col, (current_color == WHITE) ? BLACK : WHITE)) &&
            !(is_square_attacked(board, row, col + 1, (current_color == WHITE) ? BLACK : WHITE)) &&
            !(is_square_attacked(board, row, col + 2, (current_color == WHITE) ? BLACK : WHITE))) {
            move_list.moves[move_list.count].row = row;
            move_list.moves[move_list.count].col = col + 2;
            move_list.count++;
        }
    }

    // Queenside castling  
    if (board->players[current_color].can_castle_queenside && board->board_places[row][col - 1].in_game == 0 && board->board_places[row][col - 2].in_game == 0 && board->board_places[row][col - 3].in_game == 0) {
        if (!(is_square_attacked(board, row, col, (current_color == WHITE) ? BLACK : WHITE)) &&
            !(is_square_attacked(board, row, col - 1, (current_color == WHITE) ? BLACK : WHITE)) &&
            !(is_square_attacked(board, row, col - 2, (current_color == WHITE) ? BLACK : WHITE))) {
            move_list.moves[move_list.count].row = row;
            move_list.moves[move_list.count].col = col - 2;
            move_list.count++;
        }
    }
    
    return move_list;
}

int check_pawn_promotion(Board *board , int row , int col){
    Piece p = board->board_places[row][col];
    Color color = p.color ;
    if (p.piece_type != PAWN){
        return 0;
    }
    if ((color == WHITE)&&(row == 0)){
        return 1;
    } else if ((color == BLACK)&&(row == 7)){
        return 1;
    }
    return 0 ;
}

void promote_pawn(Board *board, int row, int col, Type new_type) {
    // Update the piece on the board
    board->board_places[row][col].piece_type = new_type;
    
    // Update the piece in the player's pieces array
    Color player_color = board->board_places[row][col].color;
    int piece_id = board->board_places[row][col].id;
    
    // Find and update the piece in the player's pieces array
    for (int i = 0; i < 16; i++) {
        if (board->players[player_color].pieces[i].id == piece_id) {
            board->players[player_color].pieces[i].piece_type = new_type;
            break;
        }
    }
}

// Get possible moves for a piece
MoveList get_possible_moves(Board *board, int row, int col) {
    MoveList move_list;
    move_list.count = 0;
    // Implementation of move generation logic goes here
    if (board->board_places[row][col].piece_type == PAWN) {
        move_list = pawn_moves(row, col, board);
    } else if (board->board_places[row][col].piece_type == ROOK) {
        move_list = rook_moves(row, col, board);
    } else if (board->board_places[row][col].piece_type == KNIGHT) {
        move_list = knight_moves(row, col, board);
    } else if (board->board_places[row][col].piece_type == BISHOP) {
        move_list = bishop_moves(row, col, board);
    } else if (board->board_places[row][col].piece_type == QUEEN) {
        move_list = queen_moves(row, col, board);
    } else if (board->board_places[row][col].piece_type == KING) {
        move_list = king_moves(row, col, board);
    }
    return move_list;}

// move_piece implementation would go here
void move_piece(Board board[], int from_row, int from_col, int to_row, int to_col, int move_count, Mix_Chunk *sound[]) {
    int sound_index = 0;
    Mix_HaltChannel(-1);
    
    board[move_count + 1] = board[move_count];
    
    Piece moving_piece = board[move_count].board_places[from_row][from_col];
    Piece dest_piece = board[move_count].board_places[to_row][to_col];
    int dest_was_occupied = dest_piece.in_game;
    int location = moving_piece.index_in_player;
    if (!moving_piece.in_game) return;

    board[move_count + 1].board_places[to_row][to_col] = moving_piece;
    board[move_count + 1].board_places[to_row][to_col].row = to_row;
    board[move_count + 1].board_places[to_row][to_col].col = to_col;
    board[move_count + 1].board_places[to_row][to_col].has_moved = 1;
    board[move_count + 1].board_places[from_row][from_col].in_game = 0;
    // board[move_count + 1].players[moving_piece.color].pieces[location] = moving_piece ;
    board[move_count + 1].players[moving_piece.color].pieces[location].row = to_row;
    board[move_count + 1].players[moving_piece.color].pieces[location].col = to_col;
    board[move_count + 1].players[moving_piece.color].pieces[location].has_moved = 1;
    if (moving_piece.piece_type == KING) {
        board[move_count + 1].players[moving_piece.color].king_row = to_row;
        board[move_count + 1].players[moving_piece.color].king_col = to_col;
        board[move_count + 1].players[moving_piece.color].can_castle_kingside = 0;
        board[move_count + 1].players[moving_piece.color].can_castle_queenside = 0;
        
        // (Castling)
        if (abs(to_col - from_col) == 2) {
            sound_index = 3;
            if (to_col == 6) { // kingside
                Piece rook = board[move_count + 1].board_places[to_row][7];
                int rook_index = rook.index_in_player;
                board[move_count + 1].board_places[to_row][5] = rook;
                board[move_count + 1].board_places[to_row][5].col = 5;
                board[move_count + 1].board_places[to_row][5].has_moved = 1;
                board[move_count + 1].board_places[to_row][7].in_game = 0;
                board[move_count + 1].players[moving_piece.color].pieces[rook_index].col = 5;
                board[move_count + 1].players[moving_piece.color].pieces[rook_index].has_moved = 1;
            
            } else if (to_col == 2) { // queenside
                Piece rook = board[move_count + 1].board_places[to_row][0];
                int rook_index = rook.index_in_player;
                board[move_count + 1].board_places[to_row][3] = rook;
                board[move_count + 1].board_places[to_row][3].col = 3;
                board[move_count + 1].board_places[to_row][3].has_moved = 1;
                board[move_count + 1].board_places[to_row][0].in_game = 0;
                board[move_count + 1].players[moving_piece.color].pieces[rook_index].col = 3;
                board[move_count + 1].players[moving_piece.color].pieces[rook_index].has_moved = 1;
            }
        }
    }
    if (dest_was_occupied) {
        sound_index = 1;
        Color capturer_color = moving_piece.color;
        Color captured_color = dest_piece.color;
        int captured_index = board[move_count + 1].players[capturer_color].total_captured;
        board[move_count + 1].players[capturer_color].captured_piece[captured_index] = dest_piece;
        board[move_count + 1].players[capturer_color].total_captured++;
        board[move_count + 1].players[captured_color].total_pieces--;
    }
    Color opponent_color = (moving_piece.color == WHITE) ? BLACK : WHITE;
    int opponent_king_row = board[move_count + 1].players[opponent_color].king_row;
    int opponent_king_col = board[move_count + 1].players[opponent_color].king_col;
    
    if (is_square_attacked(&board[move_count + 1], opponent_king_row, opponent_king_col, moving_piece.color)) {
        board[move_count + 1].players[opponent_color].is_in_check = 1;
        sound_index = 2;
    } else {
        board[move_count + 1].players[opponent_color].is_in_check = 0;
    }

  //    En Passant
  
    if (moving_piece.piece_type == PAWN) {
        if (abs(to_row - from_row) == 2) {
            board[move_count + 1].en_passant_row = (from_row + to_row) / 2;
            board[move_count + 1].en_passant_col = from_col;
        } else {
            // En passant capture
            if (to_row == board[move_count].en_passant_row && 
                to_col == board[move_count].en_passant_col) {
                sound_index = 1;
                int captured_pawn_row = (moving_piece.color == WHITE) ? to_row + 1 : to_row - 1;
                Piece captured_pawn = board[move_count + 1].board_places[captured_pawn_row][to_col];
                
                board[move_count + 1].players[moving_piece.color].captured_piece[
                    board[move_count + 1].players[moving_piece.color].total_captured++
                ] = captured_pawn;
                board[move_count + 1].players[captured_pawn.color].total_pieces--;
                board[move_count + 1].board_places[captured_pawn_row][to_col].in_game = 0;
            }
            board[move_count + 1].en_passant_row = -1;
            board[move_count + 1].en_passant_col = -1;
        }
    } else {
        board[move_count + 1].en_passant_row = -1;
        board[move_count + 1].en_passant_col = -1;
    }

    if (moving_piece.piece_type == ROOK) {
        Color mc = moving_piece.color;
        int home_row = (mc == WHITE) ? 7 : 0;
        if (from_row == home_row && from_col == 0) {
            board[move_count + 1].players[mc].can_castle_queenside = 0;
        } else if (from_row == home_row && from_col == 7) {
            board[move_count + 1].players[mc].can_castle_kingside = 0;
        }
    }

    // If a rook was captured on its home square, the owner loses castling on that side.
    if (dest_was_occupied && dest_piece.piece_type == ROOK) {
        Color owner = dest_piece.color;
        int owner_home_row = (owner == WHITE) ? 7 : 0;
        if (to_row == owner_home_row && to_col == 0) {
            board[move_count + 1].players[owner].can_castle_queenside = 0;
        } else if (to_row == owner_home_row && to_col == 7) {
            board[move_count + 1].players[owner].can_castle_kingside = 0;
        }
    }

    board[move_count + 1].last_move = (Move){to_row, to_col};
    board[move_count + 1].move_count = move_count + 1;
    board[move_count + 1].current_turn = opponent_color;
    
    if (dest_was_occupied || moving_piece.piece_type == PAWN) {
        board[move_count + 1].halfmove_clock = 0;
    } else {
        board[move_count + 1].halfmove_clock = board[move_count].halfmove_clock + 1;
    }
    
    board[move_count + 1].fullmove_number = board[move_count].fullmove_number + 
                                            (moving_piece.color == BLACK ? 1 : 0);

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            board[move_count + 1].chessboard[i][j] = (SDL_Rect){80 + j*80, 125 + i*80, 80, 80};
        }
    }

    if (sound && sound[sound_index]) {
        Mix_PlayChannel(-1, sound[sound_index], 0);
    }
}

// detect if the square is attacked or not
int is_square_attacked(Board *board, int row, int col, Color attacker_color) {
    // Implementation of attack detection logic goes here
    // Check for pawn attacks
    int pawn_direction = (attacker_color == WHITE) ? 1 : -1;
    int pawn_attack_rows[2] = {row + pawn_direction, row + pawn_direction};
    int pawn_attack_cols[2] = {col - 1, col + 1};
    for (int i = 0; i < 2; i++) {
        int r = pawn_attack_rows[i];
        int c = pawn_attack_cols[i];
        if (move_within_bounds(r, c)) {
            Piece p = board->board_places[r][c];
            if (p.in_game && p.color == attacker_color && p.piece_type == PAWN) {
                return 1; // Square is attacked by a pawn
            }
        }
    }
    // Check for knight attacks
    int knight_offsets[8][2] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    for (int i = 0; i < 8; i++) {
        int r = row + knight_offsets[i][0];
        int c = col + knight_offsets[i][1];
        if (move_within_bounds(r, c)) {
            Piece p = board->board_places[r][c];
            if (p.in_game && p.color == attacker_color && p.piece_type == KNIGHT) {
                return 1; // Square is attacked by a knight
            }
        }
    }
    // Check for rook/queen attacks (horizontal and vertical)
    int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int i = 0; i < 4; i++) {
        int dr = directions[i][0];
        int dc = directions[i][1];
        for (int step = 1; step < 8; step++) {
            int r = row + dr * step;
            int c = col + dc * step;
            if (!move_within_bounds(r, c)) break;
            Piece p = board->board_places[r][c];
            if (p.in_game) {
                if (p.color == attacker_color && (p.piece_type == ROOK || p.piece_type == QUEEN)) {
                    return 1; // Square is attacked by a rook or queen
                } else {
                    break; // Blocked by another piece
                }
            }
        }
    }
    // Check for bishop/queen attacks (diagonal)
    int diag_directions[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
    for (int i = 0; i < 4; i++) {
        int dr = diag_directions[i][0];
        int dc = diag_directions[i][1];
        for (int step = 1; step < 8; step++) {
            int r = row + dr * step;
            int c = col + dc * step;
            if (!move_within_bounds(r, c)) break;
            Piece p = board->board_places[r][c];
            if (p.in_game) {
                if (p.color == attacker_color && (p.piece_type == BISHOP || p.piece_type == QUEEN)) {
                    return 1; // Square is attacked by a bishop or queen
                } else {
                    break; // Blocked by another piece
                }
            }
        }
    }
    // Check for king attacks
    int king_offsets[8][2] = {
        {-1, -1}, {-1, 0}, {-1, 1},
        {0, -1},          {0, 1},
        {1, -1}, {1, 0}, {1, 1}
    };
    for (int i = 0; i < 8; i++) {
        int r = row + king_offsets[i][0];
        int c = col + king_offsets[i][1];
        if (move_within_bounds(r, c)) {
            Piece p = board->board_places[r][c];
            if (p.in_game && p.color == attacker_color && p.piece_type == KING) {
                return 1; // Square is attacked by a king
            }
        }
    }

    return 0; // Square is not attacked
}

// detect if there is checkmate
int is_checkmate(Board *board, Color color){
    if ((get_total_possible_moves(board , color) == 0)&&board->players[color].is_in_check){
        return 1;
    }
    return 0;
}

int is_it_llegal_move (int from_row , int from_col , int to_row , int to_col , Board *board){
    Board *Virtual_board = (Board*)malloc(sizeof(Board));
    if (Virtual_board == NULL) return 0;
    
    *Virtual_board = *board;  
    
    Piece moving_piece = board->board_places[from_row][from_col];
    Color color = moving_piece.color;
    
    Virtual_board->board_places[from_row][from_col].in_game = 0;
    Virtual_board->board_places[to_row][to_col] = moving_piece;
    Virtual_board->board_places[to_row][to_col].row = to_row;
    Virtual_board->board_places[to_row][to_col].col = to_col;
    
    if (moving_piece.piece_type == KING){
        Virtual_board->players[color].king_row = to_row;
        Virtual_board->players[color].king_col = to_col;
    }
    
    int is_attacked = is_square_attacked(Virtual_board, 
                                         Virtual_board->players[color].king_row, 
                                         Virtual_board->players[color].king_col, 
                                         (color == WHITE)? BLACK : WHITE);
    
    free(Virtual_board);
    if (is_attacked){
        return 0;
    }
    return 1;
}

int get_total_possible_moves(Board *board, Color color){
    int total_possible_moves = 0;
    MoveList piece_moves;
    for (int row = 0; row < 8; row++){
        for (int col = 0; col < 8; col++){
            Piece p = board->board_places[row][col];
            if (p.in_game && p.color == color){
                piece_moves = get_possible_moves(board, row, col);
                total_possible_moves += piece_moves.count;
            }
        }
    }
    
    return total_possible_moves;
}

int is_stalemate(Board *board, Color color){
    if ((get_total_possible_moves(board , color) == 0)&&board->players[color].is_in_check == 0){
        return 1;
    }
    return 0;
}

int is_insufficient_material(Board *board) {
    int white_knights = 0, white_bishops = 0;
    int black_knights = 0, black_bishops = 0;

    int white_bishop_square_color = -1;
    int black_bishop_square_color = -1;

    for (int i = 0; i < 16; i++) {
        Piece p;

        // WHITE pieces
        p = board->players[WHITE].pieces[i];
        if (p.in_game && p.piece_type != KING) {
            if (p.piece_type == PAWN || p.piece_type == ROOK || p.piece_type == QUEEN)
                return 0;

            if (p.piece_type == KNIGHT)
                white_knights++;

            if (p.piece_type == BISHOP) {
                white_bishops++;
                white_bishop_square_color = (p.row + p.col) % 2;
            }
        }

        // BLACK pieces
        p = board->players[BLACK].pieces[i];
        if (p.in_game && p.piece_type != KING) {
            if (p.piece_type == PAWN || p.piece_type == ROOK || p.piece_type == QUEEN)
                return 0;

            if (p.piece_type == KNIGHT)
                black_knights++;

            if (p.piece_type == BISHOP) {
                black_bishops++;
                black_bishop_square_color = (p.row + p.col) % 2;
            }
        }
    }

    // 1) K vs K
    if (white_knights + white_bishops == 0 &&
        black_knights + black_bishops == 0)
        return 1;

    // 2) K + minor vs K
    if ((white_knights == 1 && white_bishops == 0 && black_knights + black_bishops == 0) ||
        (black_knights == 1 && black_bishops == 0 && white_knights + white_bishops == 0))
        return 1;

    if ((white_bishops == 1 && white_knights == 0 && black_knights + black_bishops == 0) ||
        (black_bishops == 1 && black_knights == 0 && white_knights + white_bishops == 0))
        return 1;

    // 3) K + B vs K + B (same color bishops)
    if (white_bishops == 1 && black_bishops == 1 &&
        white_knights == 0 && black_knights == 0 &&
        white_bishop_square_color == black_bishop_square_color)
        return 1;

    return 0;
}

int is_threefold_repetition(Board board[], int current_move) {
    if (current_move < 8) return 0; // Need at least 8 moves for repetition
    
    char current_fen[200];
    board_to_fen(&board[current_move], current_fen);
    
    // Extract only position part (before first space)
    char current_pos[100];
    int i = 0;
    while (current_fen[i] != ' ' && current_fen[i] != '\0') {
        current_pos[i] = current_fen[i];
        i++;
    }
    current_pos[i] = '\0';
    
    int repetition_count = 1; // Current position counts as 1
    
    // Check previous positions (only need to check positions with same turn)
    // Go back by 4 moves each time (2 full moves = same player's turn)
    for (int move = current_move - 4; move >= 0; move -= 4) {
        char check_fen[200];
        board_to_fen(&board[move], check_fen);
        
        // Extract position part
        char check_pos[100];
        int j = 0;
        while (check_fen[j] != ' ' && check_fen[j] != '\0') {
            check_pos[j] = check_fen[j];
            j++;
        }
        check_pos[j] = '\0';
        
        // Compare positions
        if (strcmp(current_pos, check_pos) == 0) {
            repetition_count++;
            if (repetition_count >= 3) {
                return 1; // Threefold repetition found
            }
        }
        
        // If halfmove clock was reset, no need to check further back
        if (board[move].halfmove_clock == 0) break;
    }
    
    return 0; // No threefold repetition
}
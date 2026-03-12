#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include "board.h"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 920

void draw_text(SDL_Renderer* ren, TTF_Font* font,const char* text, SDL_Color color, int x, int y)
{
    SDL_Surface* surf = TTF_RenderText_Solid(font, text, color);
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_Rect rect = {x, y, surf->w, surf->h};
    SDL_RenderCopy(ren, tex, NULL, &rect);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

int is_clicked_highlighted_square(MoveList highlighted , int row , int col) {
    for (int i = 0; i < highlighted.count; i++) {
        if (highlighted.moves[i].row == row && highlighted.moves[i].col == col) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_Window *win = SDL_CreateWindow("Mohamed & Fares's Chess" ,SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,WINDOW_WIDTH,WINDOW_HEIGHT,SDL_WINDOW_SHOWN);
    if (win == NULL) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (ren == NULL) {
        SDL_DestroyWindow(win);
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  
    if (TTF_Init() == -1) {
        printf("TTF_Init Error: %s\n", TTF_GetError());
        return 1;
    }
    
    TTF_Font* font = TTF_OpenFont("assets/arial.ttf", 24); 
    if (!font) {
        printf("TTF_OpenFont Error: %s\n", TTF_GetError());
        return 1;
    }
    SDL_Init(SDL_INIT_AUDIO);
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("Mix_OpenAudio Error: %s\n", Mix_GetError());
        return 1;
    }
    Mix_AllocateChannels(16);

    // Load sound effects
    Mix_Chunk *sound[10];   // array of pointers to Mix_Chunk
    sound[0] = Mix_LoadWAV("./assets/move-self.wav");
    sound[1] = Mix_LoadWAV("./assets/capture.wav");
    sound[2] = Mix_LoadWAV("./assets/move-check.wav");
    sound[3] = Mix_LoadWAV("./assets/castle.wav");
    sound[4] = Mix_LoadWAV("./assets/promote.wav");
    sound[5] = Mix_LoadWAV("./assets/game-start.wav");
    sound[6] = Mix_LoadWAV("./assets/game-end.wav");
    sound[7] = Mix_LoadWAV("./assets/illegal.wav");
    
    // Load pieces images
    SDL_Texture* piece_textures[2][6]; // [color][piece_type]
    
    // White pieces
    piece_textures[WHITE][PAWN] = IMG_LoadTexture(ren, "./assets/pawn_white.png");
    piece_textures[WHITE][ROOK] = IMG_LoadTexture(ren, "./assets/rook_white.png");
    piece_textures[WHITE][KNIGHT] = IMG_LoadTexture(ren, "./assets/knight_white.png");
    piece_textures[WHITE][BISHOP] = IMG_LoadTexture(ren, "./assets/bishop_white.png");
    piece_textures[WHITE][QUEEN] = IMG_LoadTexture(ren, "./assets/queen_white.png");
    piece_textures[WHITE][KING] = IMG_LoadTexture(ren, "./assets/king_white.png");
    
    // Black pieces
    piece_textures[BLACK][PAWN] = IMG_LoadTexture(ren, "./assets/pawn_black.png");
    piece_textures[BLACK][ROOK] = IMG_LoadTexture(ren, "./assets/rook_black.png");
    piece_textures[BLACK][KNIGHT] = IMG_LoadTexture(ren, "./assets/knight_black.png");
    piece_textures[BLACK][BISHOP] = IMG_LoadTexture(ren, "./assets/bishop_black.png");
    piece_textures[BLACK][QUEEN] = IMG_LoadTexture(ren, "./assets/queen_black.png");
    piece_textures[BLACK][KING] = IMG_LoadTexture(ren, "./assets/king_black.png");
    
    // Game variables
    
    //selected square
    MoveList highlighted_squares;
    highlighted_squares.count = 0;
    for (int i = 0; i < 27; i++) {
        highlighted_squares.moves[i].row = -1;
        highlighted_squares.moves[i].col = -1;
    }

    int running = 1 , x , y , game_over = 0; // main loop flag , x, y; // mouse coordinates
    int there_is_a_promotion = 0 ; // for the promotion
    int promoted_row , promoted_col ;
    int move_count = 0; // move counter and also indicates turn (white starts)
    int latest_move = move_count;
    char fen[100]; // FEN string for saving/loading
    Board board[500]; // array of boards to store game states for undo/redo
    init_board(&board[move_count]); // initialize the first board
    SDL_Event event;

    SDL_Rect pieces_captured[2][15]; // rectangles for captured pieces display

    SDL_Color color_black = {0, 0, 0, 255};
    char letters[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'};
    int numbers[] = {8, 7, 6, 5, 4, 3, 2, 1};
    char options[][20] = {"New", "Save", "Load", "Undo", "Redo"};
    // Initialize chessboard squares
    for (int i=0 ; i < 8 ; i++){
        for (int j=0 ; j < 8 ; j++){
            (board[move_count].chessboard[i][j]).x = 80 + j*80;
            (board[move_count].chessboard[i][j]).y = 125 + i*80;
            (board[move_count].chessboard[i][j]).w = 80;
            (board[move_count].chessboard[i][j]).h = 80;
        };
    };
    // Initialize captured pieces areas
    for (int i=0 ; i<2 ; i++){
        for (int j=0 ; j<15 ; j++){
            pieces_captured[i][j].x = 100 + j*40;
            pieces_captured[i][j].y = (i==0) ? 810 : 70;
            pieces_captured[i][j].w = 40;
            pieces_captured[i][j].h = 40;
        };
    };
    // commands
    char commands[][100] = {" ","White wins","Black wins","Draw by stalemate",
        "Draw by insufficient material","Draw by threefold repetition",
        "Draw by fifty-move rule","Game saved","Game loaded","Move undone",
        "Move redone","Invalid move","Check","Which piece to promote to? (Q,R,B,N)","Error has occured" , "Save it in Saved_Games,enter file name in the terminal"};
    int command_index = 0;
    int times = 0;
    int game_end = 1 ;
    // Main loop
    while (running) {
        latest_move = (move_count > latest_move) ? move_count : latest_move;
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderClear(ren);
        // draw chessboard
        for (int i=0 ; i<8 ; i++){
            for (int j=0 ; j<8 ; j++){
                if ((i+j)%2 == 0){
                    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                } else {
                    SDL_SetRenderDrawColor(ren, 128, 128, 128, 255);
                        }
                SDL_RenderFillRect(ren, &(board[move_count].chessboard[i][j]));
            }
        }
        // draw letters and numbers
        for (int i=0 ; i<8 ; i++){
            char letter_str[2] = {letters[i], '\0'};
            draw_text(ren, font, letter_str, color_black, 80 + i*80 + 30, 780);
            
            char number_str[3];
            sprintf(number_str, "%d", numbers[i]);
            draw_text(ren, font, number_str, color_black, 40, 80 + i*80 + 75);
        }
        // draw captured pieces areas
        for (int i=0 ; i<2 ; i++){
            for (int j=0 ; j<15 ; j++){
                SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
                SDL_RenderDrawRect(ren, &pieces_captured[i][j]);
            };
        };
        // making the square of the king red when it is in check :)
        Color current_player_color = (move_count % 2 == 0) ? WHITE : BLACK;
        int k_row = board[move_count].players[current_player_color].king_row;
        int k_col = board[move_count].players[current_player_color].king_col;
        Color opponent_color = (current_player_color == WHITE) ? BLACK : WHITE;
        if (is_square_attacked(&board[move_count], k_row, k_col, opponent_color)) {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 255); 
            SDL_RenderFillRect(ren, &(board[move_count].chessboard[k_row][k_col]));
        }
        // draw pieces
        for(int row = 0; row < 8; row++) {
            for(int col = 0; col < 8; col++) {
                Piece piece = board[move_count].board_places[row][col];
                if (piece.in_game) {
                    SDL_Texture* tex = piece_textures[piece.color][piece.piece_type];
                    SDL_Rect dest_rect = board[move_count].chessboard[row][col];
                    SDL_RenderCopy(ren, tex, NULL, &dest_rect);
                }
            }
        }
        // show highlights after drawing pieces so they are visible on top
        show_possible_moves(&board[move_count] , highlighted_squares , ren);
        // options
        for (int i = 0; i < 5; i++) {
            char* option_str = options[i];
            SDL_Surface* option_surf = TTF_RenderText_Solid(font, option_str, color_black);
            SDL_Texture* option_tex = SDL_CreateTextureFromSurface(ren, option_surf);
            SDL_Rect button_rect = {90 + 128*i, 10, 110, 40};
            SDL_SetRenderDrawColor(ren, 180, 220, 255, 255);
            SDL_RenderFillRect(ren, &button_rect);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderDrawRect(ren, &button_rect);
            SDL_Rect text_rect;
            text_rect.w = option_surf->w;
            text_rect.h = option_surf->h;
            text_rect.x = button_rect.x + (button_rect.w - text_rect.w) / 2;
            text_rect.y = button_rect.y + (button_rect.h - text_rect.h) / 2;
            SDL_RenderCopy(ren, option_tex, NULL, &text_rect);
            SDL_FreeSurface(option_surf);
            SDL_DestroyTexture(option_tex);
        }
        // player turn display
        const char* turn_str = (move_count % 2 == 0) ? "White's Turn" : "Black's Turn";
        SDL_Surface* turn_surf = TTF_RenderText_Solid(font, turn_str, color_black);
        SDL_Texture* turn_tex = SDL_CreateTextureFromSurface(ren, turn_surf);
        SDL_Rect turn_rect = {WINDOW_WIDTH - turn_surf->w - 20, 880, turn_surf->w, turn_surf->h};
        SDL_RenderCopy(ren, turn_tex, NULL, &turn_rect);
        SDL_FreeSurface(turn_surf);
        SDL_DestroyTexture(turn_tex);
        // show current command
        const char* command_str = commands[command_index];
        draw_text(ren, font, command_str, color_black, 20, 880);
        if (move_count == 0 && times == 0) {Mix_PlayChannel(-1, sound[5], 0); times = 1 ;}
        // Event handling
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
            // Handle other events (mouse clicks, etc.) here
            if (there_is_a_promotion && event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;
                Type promoted_type = QUEEN; // default
                int valid_promotion = 0;
                
                if (key == SDLK_q) {
                    promoted_type = QUEEN;
                    valid_promotion = 1;
                }
                else if (key == SDLK_r) {
                    promoted_type = ROOK;
                    valid_promotion = 1;
                }
                else if (key == SDLK_b) {
                    promoted_type = BISHOP;
                    valid_promotion = 1;
                }
                else if (key == SDLK_n) {
                    promoted_type = KNIGHT;
                    valid_promotion = 1;
                }
                
                if (valid_promotion) {
                    promote_pawn(&board[move_count], promoted_row, promoted_col, promoted_type);
                    Mix_HaltChannel(-1);
                    Mix_PlayChannel(-1, sound[4], 0);
                    there_is_a_promotion = 0;
                    command_index = 0;
                }
                continue;   
            }
            if (there_is_a_promotion) {
                continue;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && !game_over) {
                x = event.button.x;
                y = event.button.y;
                // Handle mouse click at (x, y)
                if (check_in_bounds(x, y)) {
                    int row = (y - 125) / 80;
                    int col = (x - 80) / 80;
                    if ((board[move_count].board_places[row][col].in_game) && (board[move_count].board_places[row][col].color == ((move_count % 2 == 0) ? WHITE : BLACK))) {
                        // Piece selected
                        // int selecting = 1;
                        board[move_count].selected_piece = &board[move_count].board_places[(y - 125) / 80][(x - 80) / 80];
                        MoveList possible_moves = get_possible_moves(&board[move_count], board[move_count].selected_piece->row, board[move_count].selected_piece->col);
                        for (int i=0 ; i < possible_moves.count ; i++){
                            highlighted_squares.moves[i].row = possible_moves.moves[i].row;
                            highlighted_squares.moves[i].col = possible_moves.moves[i].col;
                        }
                        highlighted_squares.count = possible_moves.count;
                        } else if ((board[move_count].selected_piece != NULL) && is_clicked_highlighted_square(highlighted_squares , row , col)) {
                            // Move selected piece
                            int from_row = board[move_count].selected_piece->row;
                            int from_col = board[move_count].selected_piece->col;
                            int to_row = row;
                            int to_col = col;
                            move_piece(board, from_row, from_col, to_row, to_col , move_count , sound);
                            command_index = 0 ;
                            move_count++;
                            if (check_pawn_promotion(&board[move_count] , to_row , to_col)){
                                there_is_a_promotion = 1 ; 
                                command_index = 13 ;
                                promoted_row = to_row ;
                                promoted_col = to_col ;
                            }
                            // Clear selection and highlights
                            board[move_count].selected_piece = NULL;
                            highlighted_squares.count = 0;
                            for (int i = 0; i < 27; i++) {
                                highlighted_squares.moves[i].row = -1;
                                highlighted_squares.moves[i].col = -1;
                            }
                            board[move_count].fullmove_number = (move_count / 2) + 1;
                            // current_player = board[move_count].players[(move_count % 2 == 0) ? WHITE : BLACK];
                            }
                } else if (x >= 90 && x <= 200 && y >= 10 && y <= 50) {
                    // New Game button clicked
                    move_count = 0;
                    latest_move = 0;
                    times = 0 ;
                    game_over = 0;
                    game_end = 1;
                    there_is_a_promotion = 0;
                    init_board(&board[move_count]);
                    highlighted_squares.count = 0;
                    for (int i = 0; i < 27; i++) {
                        highlighted_squares.moves[i].row = -1;
                        highlighted_squares.moves[i].col = -1; }
                        command_index = 0;
                    } else if (x >= 218 && x <= 328 && y >= 10 && y <= 50) {
                        // Save Game button clicked
                        // Implement save functionality here
                        board_to_fen(&board[move_count],fen);
                        if (save_file(fen))
                        command_index = 7; // "Game saved"
                        else command_index = 14; // "Error has occured"
                    } else if (x >= 346 && x <= 456 && y >= 10 && y <= 50) {
                        // Load Game button clicked
                        command_index = 15;

                        char filename[100];
                        char filepath[200];

                        printf("Enter file name: ");
                        scanf("%99s", filename);   
                        sprintf(filepath, "Saved_Games/%s", filename);

                        if (is_file_found(filepath)) {
                            FILE *file = fopen(filepath, "r");
                            if (!file) {
                                printf("Error opening the file!\n");
                                command_index = 14;
                            } else {
                                char fen[200];
                                if (fgets(fen, sizeof(fen), file) == NULL) {
                                    printf("Error reading file!\n");
                                    command_index = 14;
                                } else {
                                    fen[strcspn(fen, "\n")] = '\0';
                                    
                                    if (!is_valid_fen(fen)) {
                                        printf("Invalid FEN file!\n");
                                        command_index = 14;
                                    } else {
                                        // Reset everything
                                        move_count = 0;
                                        latest_move = 0;
                                        times = 0;
                                        game_over = 0;
                                        game_end = 1;
                                        there_is_a_promotion = 0;
                                        
                                        // Initialize board and load FEN
                                        init_board(&board[move_count]);
                                        fen_to_board(&board[move_count], fen);
                                        
                                        // Clear selections
                                        board[move_count].selected_piece = NULL;
                                        highlighted_squares.count = 0;
                                        for (int i = 0; i < 27; i++) {
                                            highlighted_squares.moves[i].row = -1;
                                            highlighted_squares.moves[i].col = -1;
                                        }
                                        
                                        printf("Game Loaded Successfully!\n");
                                        command_index = 8;
                                    }
                                }
                                fclose(file);
                            }
                        } else {
                            printf("The file is not found!\n");
                            command_index = 14;
                        }
                    }
                    else if (x >= 474 && x <= 584 && y >= 10 && y <= 50) {
                        // Undo button clicked
                        if (move_count > 0) {
                            move_count--;
                            command_index = 9; // "Move undone"
                        }
                    } else if (x >= 602 && x <= 712 && y >= 10 && y <= 50) {
                        // Redo button clicked
                        if (move_count < latest_move) {
                            move_count++;
                            command_index = 10; // "Move redone"
                        }
                    }
            }
        }
    // white captured pieces
    if (board[move_count].players[WHITE].total_captured > 0) {
        for (int i = 0; i < board[move_count].players[WHITE].total_captured; i++) {
            Type captured_type = board[move_count].players[WHITE].captured_piece[i].piece_type;
            SDL_Texture* tex = piece_textures[BLACK][captured_type];
            SDL_RenderCopy(ren, tex, NULL, &pieces_captured[0][i]);
        }
    }
    // black captured pieces
    if (board[move_count].players[BLACK].total_captured > 0) {
        for (int i = 0; i < board[move_count].players[BLACK].total_captured; i++) {
            Type captured_type = board[move_count].players[BLACK].captured_piece[i].piece_type;
            SDL_Texture* tex = piece_textures[WHITE][captured_type];
            SDL_RenderCopy(ren, tex, NULL, &pieces_captured[1][i]);
        }
    }

        // check game status
        if (is_checkmate(&board[move_count], (move_count % 2) ? BLACK : WHITE)) {
            game_over = 1;
            if (move_count % 2) {
                command_index = 1; // White wins
            } else {
                command_index = 2; // Black wins
            }
            if (game_end) {
                Mix_PlayChannel(-1, sound[6], 0);
                game_end = 0;
            }
        } 
        else if (is_stalemate(&board[move_count], (move_count % 2) ? BLACK : WHITE)) {
            game_over = 1;
            command_index = 3;
            if (game_end) {
                Mix_PlayChannel(-1, sound[6], 0);
                game_end = 0;
            }
        } 
        else if (board[move_count].halfmove_clock >= 100) {
            game_over = 1;
            command_index = 6;
            if (game_end) {
                Mix_PlayChannel(-1, sound[6], 0);
                game_end = 0;
            }
        } 
        else if (is_insufficient_material(&board[move_count])) {
            game_over = 1;
            command_index = 4;
            if (game_end) {
                Mix_PlayChannel(-1, sound[6], 0);
                game_end = 0;
            }
        }
        else if (is_threefold_repetition(board, move_count)) {
            game_over = 1;
            command_index = 5; // Draw by threefold repetition
            if (game_end) {
                Mix_PlayChannel(-1, sound[6], 0);
                game_end = 0;
            }
        }

        SDL_RenderPresent(ren);
    }


    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 6; j++) {
            SDL_DestroyTexture(piece_textures[i][j]);
        }
    }
    for (int i = 0; i < 8; i++) {
        Mix_FreeChunk(sound[i]);
    }
    Mix_CloseAudio();
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

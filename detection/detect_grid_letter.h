#ifndef DETECT_GRID_LETTER_H
#define DETECT_GRID_LETTER_H

#include <SDL2/SDL.h>
#include <stdint.h>

// on réutilise draw_rect_thick pour dessiner les bboxes en bleu
void draw_rect_thick(SDL_Surface *surf,
                     int x0, int y0,
                     int x1, int y1,
                     Uint8 R, Uint8 G, Uint8 B,
                     int thickness);

// get_gray nous sert à mesurer l'encre
Uint8 get_gray(SDL_Surface *surf, int x, int y);

// trouve bbox de l'encre dans une sous-zone (utilisé pour resserrer)
int find_ink_bbox_in_cell(SDL_Surface *surf,
                          int x0, int y0,
                          int x1, int y1,
                          Uint8 black_thresh,
                          int *lx0, int *ly0,
                          int *lx1, int *ly1);

// nouvelle fonction pour la grille inspirée de la logique mots
void detect_letters_in_grid_like_words(SDL_Surface *img,
                                       SDL_Surface *disp,
                                       int gx0, int gy0,
                                       int gx1, int gy1,
                                       Uint8 black_thresh,
                                       double line_fill_thresh,
                                       int gap_line,
                                       double col_fill_thresh,
                                       int gap_col,
                                       Uint8 r, Uint8 g, Uint8 b);

#endif


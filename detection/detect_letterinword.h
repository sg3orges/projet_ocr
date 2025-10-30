#ifndef DETECT_LETTERINWORD_H
#define DETECT_LETTERINWORD_H

#include <SDL.h>
#include <stdint.h>

// Détecte les lettres dans la zone mots (words_xmin..words_xmax / words_ymin..words_ymax)
// et dessine un rectangle BLEU autour de chaque lettre sur la surface 'disp' (RGBA32).
//
// Paramètres :
//   img         = image source en niveaux de gris (analyse des pixels)
//   disp        = surface couleur ARGB32 sur laquelle on dessine les rectangles colorés
//   words_xmin, words_xmax, words_ymin, words_ymax = bounding box de la zone "mots"
//   black_thresh         = seuil pixel noir (ex: 128)
//   line_fill_thresh     = seuil pour dire "cette ligne contient du texte" (ex: 0.05 = 5% noir)
//   gap_line             = tolérance de trou vertical entre segments du même mot (ex: 3)
//   col_fill_thresh      = seuil pour dire "cette colonne fait partie d'une lettre" (ex: 0.10 = 10% noir)
//   gap_col              = tolérance de trou horizontal entre colonnes d'une même lettre (ex: 2)
//   r,g,b                = couleur du rectangle autour de chaque lettre (ex: 0,0,255 pour bleu)
//
// Effet de bord : dessine directement sur 'disp' (utilise draw_rect() que tu as déjà dans detection.c)
void detect_and_draw_letters_in_words(SDL_Surface *img,
                                      SDL_Surface *disp,
                                      int words_xmin, int words_xmax,
                                      int words_ymin, int words_ymax,
                                      uint8_t black_thresh,
                                      double line_fill_thresh,
                                      int gap_line,
                                      double col_fill_thresh,
                                      int gap_col,
                                      Uint8 r, Uint8 g, Uint8 b);

// Besoin que le .c puisse appeler draw_rect() qui est définie dans detection.c
// Donc on déclare juste son prototype ici pour que le compilateur soit content.
void draw_rect(SDL_Surface *surf, int x0, int y0, int x1, int y1,
               Uint8 R, Uint8 G, Uint8 B);

// Besoin aussi de get_gray, pour lire les pixels
uint8_t get_gray(SDL_Surface *surf, int x, int y);

#endif


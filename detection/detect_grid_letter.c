#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include "detect_grid_letter.h"

// On veut utiliser draw_rect_thick() définie dans detection2.c
// On la redéclare ici pour que le linker puisse lier.
void draw_rect_thick(SDL_Surface *surf,
                     int x0, int y0,
                     int x1, int y1,
                     Uint8 R, Uint8 G, Uint8 B,
                     int thickness);

// ------------------------------------------------------------
// get_gray local : lire un pixel en niveau de gris
// (0 = noir, 255 = blanc)
// ------------------------------------------------------------
static inline Uint8 get_gray_local(SDL_Surface *surf, int x, int y)
{
    Uint8 *p = (Uint8 *)surf->pixels
             + y * surf->pitch
             + x * surf->format->BytesPerPixel;

    Uint32 pixel = 0;
    for (int k = 0; k < surf->format->BytesPerPixel; k++) {
        ((Uint8*)&pixel)[k] = p[k];
    }

    Uint8 r,g,b;
    SDL_GetRGB(pixel, surf->format, &r, &g, &b);
    return (Uint8)((r + g + b) / 3);
}

// ------------------------------------------------------------
// find_ink_bbox_in_cell local : recherche la bbox serrée des
// pixels "assez sombres" (< black_thresh) dans une sous-zone.
// Renvoie 1 si trouvé, 0 sinon.
// ------------------------------------------------------------
static int find_ink_bbox_in_cell_local(SDL_Surface *surf,
                                       int x0, int y0,
                                       int x1, int y1,
                                       Uint8 black_thresh,
                                       int *lx0, int *ly0,
                                       int *lx1, int *ly1)
{
    int found = 0;
    int minx = x1;
    int maxx = x0;
    int miny = y1;
    int maxy = y0;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            Uint8 g = get_gray_local(surf, x, y);
            if (g < black_thresh) {
                if (!found) {
                    found = 1;
                    minx = maxx = x;
                    miny = maxy = y;
                } else {
                    if (x < minx) minx = x;
                    if (x > maxx) maxx = x;
                    if (y < miny) miny = y;
                    if (y > maxy) maxy = y;
                }
            }
        }
    }

    if (!found) return 0;
    *lx0 = minx;
    *lx1 = maxx;
    *ly0 = miny;
    *ly1 = maxy;
    return 1;
}

// ------------------------------------------------------------
// detect_letters_in_grid_like_words
//
// Principe : on applique la même logique que pour la colonne
// des mots, mais sur la zone de la grille complète.
// Étape 1 : On scanne les lignes (y) pour trouver des "bandes"
//           horizontales contenant du texte (les rangées de la grille).
// Étape 2 : Pour chaque bande, on scanne les colonnes (x) pour
//           isoler chaque lettre individuellement.
//
// Params :
//   gx0,gy0,gx1,gy1  -> sous-zone de la grille (déjà affinée)
//   black_thresh     -> seuil pour dire "pixel sombre"
//   line_fill_thresh -> % de noir minimal pour dire qu'une ligne y
//                       appartient à une rangée de texte
//   gap_line         -> tolérance de quelques pixels clairs pour
//                       rester dans la même bande verticale
//   col_fill_thresh  -> % de noir minimal pour dire qu'une colonne x
//                       appartient à une lettre
//   gap_col          -> tolérance entre colonnes sombres pour dire
//                       qu'on reste dans la même lettre
//   r,g,b            -> couleur du rectangle (typiquement bleu)
// ------------------------------------------------------------
void detect_letters_in_grid_like_words(
    SDL_Surface *img,
    SDL_Surface *disp,
    int gx0, int gy0,
    int gx1, int gy1,
    Uint8 black_thresh,
    double line_fill_thresh,
    int gap_line,
    double col_fill_thresh,
    int gap_col,
    Uint8 r, Uint8 g, Uint8 b)
{
    if (gx0 < 0) gx0 = 0;
    if (gy0 < 0) gy0 = 0;
    if (gx1 >= img->w) gx1 = img->w - 1;
    if (gy1 >= img->h) gy1 = img->h - 1;
    if (gx0 > gx1 || gy0 > gy1) return;

    int width  = gx1 - gx0 + 1;
    int height = gy1 - gy0 + 1;

    // 1) ratio de noir par ligne (y)
    double *row_ratio = malloc(sizeof(double) * height);
    if (!row_ratio) return;

    for (int yy = gy0; yy <= gy1; yy++) {
        int black = 0;
        int total = 0;
        for (int xx = gx0; xx <= gx1; xx++) {
            Uint8 gv = get_gray_local(img, xx, yy);
            if (gv < black_thresh) black++;
            total++;
        }
        row_ratio[yy - gy0] = (double)black / (double)total;
    }

    // On groupe les y consécutifs où row_ratio > line_fill_thresh
    // en "bandes" horizontales (lignes de la grille).
    int in_band = 0;
    int band_start = 0;
    int last_good_y = -100000;

    for (int yy = gy0; yy <= gy1; yy++) {
        double rr = row_ratio[yy - gy0];

        if (rr > line_fill_thresh) {
            // On est sur une ligne qui contient de l'encre
            if (!in_band) {
                in_band = 1;
                band_start = yy;
            }
            last_good_y = yy;
        } else {
            // ligne "claire"
            if (in_band) {
                // fin de bande si trou trop gros
                if (yy - last_good_y > gap_line) {
                    int band_y0 = band_start;
                    int band_y1 = last_good_y;

                    // traiter cette bande pour détecter les lettres
                    int band_h = band_y1 - band_y0 + 1;
                    if (band_h > 5) {
                        // 2) ratio de noir par colonne dans cette bande
                        double *col_ratio_local = malloc(sizeof(double) * width);
                        if (!col_ratio_local) {
                            in_band = 0;
                            continue;
                        }

                        for (int xx = gx0; xx <= gx1; xx++) {
                            int black2 = 0;
                            int total2 = 0;
                            for (int yy2 = band_y0; yy2 <= band_y1; yy2++) {
                                Uint8 gv2 = get_gray_local(img, xx, yy2);
                                if (gv2 < black_thresh) black2++;
                                total2++;
                            }
                            col_ratio_local[xx - gx0] =
                                (double)black2 / (double)total2;
                        }

                        // Grouper les colonnes sombres pour isoler chaque lettre
                        int in_letter = 0;
                        int letter_xstart = 0;
                        int last_good_x = -100000;

                        for (int xx = gx0; xx <= gx1; xx++) {
                            double cr = col_ratio_local[xx - gx0];

                            if (cr > col_fill_thresh) {
                                if (!in_letter) {
                                    in_letter = 1;
                                    letter_xstart = xx;
                                }
                                last_good_x = xx;
                            } else {
                                if (in_letter) {
                                    if (xx - last_good_x > gap_col) {
                                        int lx0 = letter_xstart;
                                        int lx1 = last_good_x;
                                        int ly0 = band_y0;
                                        int ly1 = band_y1;

                                        int bw = lx1 - lx0 + 1;
                                        int bh = ly1 - ly0 + 1;

                                        // filtrer trucs ridicules (genre lignes fines)
                                        if (bw > 5 && bh > 5) {
                                            // raffine bbox sur les pixels noirs réels
                                            int rx0, rx1, ry0, ry1;
                                            if (find_ink_bbox_in_cell_local(
                                                    img,
                                                    lx0, ly0,
                                                    lx1, ly1,
                                                    black_thresh,
                                                    &rx0, &ry0,
                                                    &rx1, &ry1
                                                ))
                                            {
                                                draw_rect_thick(disp,
                                                                rx0, ry0,
                                                                rx1, ry1,
                                                                r,g,b,
                                                                2);
                                            } else {
                                                draw_rect_thick(disp,
                                                                lx0, ly0,
                                                                lx1, ly1,
                                                                r,g,b,
                                                                2);
                                            }
                                        }

                                        in_letter = 0;
                                    }
                                }
                            }
                        }

                        // fermer dernière lettre encore ouverte
                        if (in_letter) {
                            int lx0 = letter_xstart;
                            int lx1 = last_good_x;
                            int ly0 = band_y0;
                            int ly1 = band_y1;

                            int bw = lx1 - lx0 + 1;
                            int bh = ly1 - ly0 + 1;
                            if (bw > 5 && bh > 5) {
                                int rx0, rx1, ry0, ry1;
                                if (find_ink_bbox_in_cell_local(
                                        img,
                                        lx0, ly0,
                                        lx1, ly1,
                                        black_thresh,
                                        &rx0, &ry0,
                                        &rx1, &ry1
                                    ))
                                {
                                    draw_rect_thick(disp,
                                                    rx0, ry0,
                                                    rx1, ry1,
                                                    r,g,b,
                                                    2);
                                } else {
                                    draw_rect_thick(disp,
                                                    lx0, ly0,
                                                    lx1, ly1,
                                                    r,g,b,
                                                    2);
                                }
                            }
                        }

                        free(col_ratio_local);
                    }

                    // terminer la bande
                    in_band = 0;
                }
            }
        }
    }

    // fermer la dernière bande si elle reste ouverte
    if (in_band) {
        int band_y0 = band_start;
        int band_y1 = last_good_y;

        if (band_y1 >= band_y0) {
            int band_h = band_y1 - band_y0 + 1;
            if (band_h > 5) {
                double *col_ratio_local = malloc(sizeof(double) * width);
                if (col_ratio_local) {
                    for (int xx = gx0; xx <= gx1; xx++) {
                        int black2 = 0;
                        int total2 = 0;
                        for (int yy2 = band_y0; yy2 <= band_y1; yy2++) {
                            Uint8 gv2 = get_gray_local(img, xx, yy2);
                            if (gv2 < black_thresh) black2++;
                            total2++;
                        }
                        col_ratio_local[xx - gx0] =
                            (double)black2 / (double)total2;
                    }

                    int in_letter = 0;
                    int letter_xstart = 0;
                    int last_good_x = -100000;

                    for (int xx = gx0; xx <= gx1; xx++) {
                        double cr = col_ratio_local[xx - gx0];

                        if (cr > col_fill_thresh) {
                            if (!in_letter) {
                                in_letter = 1;
                                letter_xstart = xx;
                            }
                            last_good_x = xx;
                        } else {
                            if (in_letter) {
                                if (xx - last_good_x > gap_col) {
                                    int lx0 = letter_xstart;
                                    int lx1 = last_good_x;
                                    int ly0 = band_y0;
                                    int ly1 = band_y1;
                                    int bw = lx1 - lx0 + 1;
                                    int bh = ly1 - ly0 + 1;
                                    if (bw > 5 && bh > 5) {
                                        int rx0, rx1, ry0, ry1;
                                        if (find_ink_bbox_in_cell_local(
                                                img,
                                                lx0, ly0,
                                                lx1, ly1,
                                                black_thresh,
                                                &rx0, &ry0,
                                                &rx1, &ry1
                                            ))
                                        {
                                            draw_rect_thick(disp,
                                                            rx0, ry0,
                                                            rx1, ry1,
                                                            r,g,b,
                                                            2);
                                        } else {
                                            draw_rect_thick(disp,
                                                            lx0, ly0,
                                                            lx1, ly1,
                                                            r,g,b,
                                                            2);
                                        }
                                    }
                                    in_letter = 0;
                                }
                            }
                        }
                    }

                    if (in_letter) {
                        int lx0 = letter_xstart;
                        int lx1 = last_good_x;
                        int ly0 = band_y0;
                        int ly1 = band_y1;
                        int bw = lx1 - lx0 + 1;
                        int bh = ly1 - ly0 + 1;
                        if (bw > 5 && bh > 5) {
                            int rx0, rx1, ry0, ry1;
                            if (find_ink_bbox_in_cell_local(
                                    img,
                                    lx0, ly0,
                                    lx1, ly1,
                                    black_thresh,
                                    &rx0, &ry0,
                                    &rx1, &ry1
                                ))
                            {
                                draw_rect_thick(disp,
                                                rx0, ry0,
                                                rx1, ry1,
                                                r,g,b,
                                                2);
                            } else {
                                draw_rect_thick(disp,
                                                lx0, ly0,
                                                lx1, ly1,
                                                r,g,b,
                                                2);
                            }
                        }
                    }

                    free(col_ratio_local);
                }
            }
        }
    }

    free(row_ratio);
}


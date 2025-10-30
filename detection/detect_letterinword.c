#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>
// Représente une "bande" horizontale de texte (un mot entier)
typedef struct {
    int y_start;
    int y_end;
} Band;

// Représente une boîte verticale de lettre dans une bande de mot
typedef struct {
    int x_start;
    int x_end;
} LetterBox;

// ------------------------------------------------------------
// Détecter les bandes horizontales = chaque MOT
// ------------------------------------------------------------
//
// On parcourt toutes les lignes y entre words_ymin et words_ymax.
// On regarde, pour chaque y, si cette ligne contient assez de pixels noirs
// dans [words_xmin .. words_xmax].
// On regroupe les lignes sombres consécutives en une "bande".
// Chaque bande est supposée être un mot écrit sur une ligne.
//
static Band* detect_word_bands(SDL_Surface *surf,
                               int x_min, int x_max,
                               int y_min, int y_max,
                               uint8_t black_thresh,
                               double line_fill_thresh,
                               int gap_max,
                               int *out_count)
{
    int max_bands = (y_max - y_min + 1);
    Band *bands = malloc(sizeof(Band) * max_bands);
    int band_count = 0;

    int in_band = 0;
    int run_start = 0;
    int last_good = -10000;

    for (int y = y_min; y <= y_max; y++) {
        int black = 0;
        int total = 0;

        // calcule ratio de noir sur la ligne y, entre x_min et x_max
        for (int x = x_min; x <= x_max; x++) {
            uint8_t g = get_gray(surf, x, y);
            if (g < black_thresh) black++;
            total++;
        }

        double ratio = (double)black / (double)total;

        if (ratio > line_fill_thresh) {
            // cette ligne contient du texte
            if (!in_band) {
                in_band = 1;
                run_start = y;
            }
            last_good = y;
        } else {
            // ligne assez blanche
            if (in_band) {
                // si le "trou" devient trop grand,
                // on considère que le mot précédent est fini
                if (y - last_good > gap_max) {
                    bands[band_count].y_start = run_start;
                    bands[band_count].y_end   = last_good;
                    band_count++;

                    in_band = 0;
                }
            }
        }
    }

    // on ferme la dernière bande si on finissait dans un mot
    if (in_band) {
        bands[band_count].y_start = run_start;
        bands[band_count].y_end   = last_good;
        band_count++;
    }

    *out_count = band_count;
    return bands;
}

// ------------------------------------------------------------
// Détecter les lettres à l'intérieur d'une bande (un mot)
// ------------------------------------------------------------
//
// On reste dans une seule bande verticale [band_ymin .. band_ymax].
// On balaie chaque colonne x dans [words_xmin .. words_xmax].
// Si une colonne a "assez de noir" dans CETTE bande,
// elle est considérée comme faisant partie d'une lettre.
// On regroupe les colonnes sombres consécutives pour obtenir
// une boîte par lettre.
//
static LetterBox* detect_letter_boxes_in_band(SDL_Surface *surf,
                                              int band_ymin, int band_ymax,
                                              int x_min, int x_max,
                                              uint8_t black_thresh,
                                              double col_fill_thresh,
                                              int gap_max,
                                              int *out_count)
{
    int max_letters = (x_max - x_min + 1);
    LetterBox *letters = malloc(sizeof(LetterBox) * max_letters);
    int letter_count = 0;

    int in_letter = 0;
    int run_start = 0;
    int last_good = -10000;

    for (int x = x_min; x <= x_max; x++) {
        int black = 0;
        int total = 0;

        // pour cette colonne x, on ne regarde que la hauteur du mot
        for (int y = band_ymin; y <= band_ymax; y++) {
            uint8_t g = get_gray(surf, x, y);
            if (g < black_thresh) black++;
            total++;
        }

        double ratio = (double)black / (double)total;

        if (ratio > col_fill_thresh) {
            // cette colonne appartient à une lettre (ou un bout de lettre)
            if (!in_letter) {
                in_letter = 1;
                run_start = x;
            }
            last_good = x;
        } else {
            // colonne blanche
            if (in_letter) {
                // si l'écart devient trop grand, la lettre est finie
                if (x - last_good > gap_max) {
                    letters[letter_count].x_start = run_start;
                    letters[letter_count].x_end   = last_good;
                    letter_count++;

                    in_letter = 0;
                }
            }
        }
    }

    // fermer la dernière lettre si on était en train d'en tracer une
    if (in_letter) {
        letters[letter_count].x_start = run_start;
        letters[letter_count].x_end   = last_good;
        letter_count++;
    }

    *out_count = letter_count;
    return letters;
}

// ------------------------------------------------------------
// Fonction publique : détecter & dessiner toutes les lettres
// ------------------------------------------------------------
//
// img  = image d'origine (N&B), pour analyser les pixels
// disp = surface couleur (RGBA32) sur laquelle on dessine les cadres bleus
//
// words_xmin/xmax/ymin/ymax = bounding box de la zone "liste de mots"
//
// Les paramètres de seuil (black_thresh, line_fill_thresh, col_fill_thresh, gap_line, gap_col)
// servent juste à régler la sensibilité.
//   - black_thresh: au-dessous de ça = "noir"
//   - line_fill_thresh: % de noir pour dire "cette ligne appartient à un mot"
//   - col_fill_thresh : % de noir pour dire "cette colonne appartient à une lettre"
//   - gap_line: trous verticaux autorisés dans un même mot
//   - gap_col : trous horizontaux autorisés dans une même lettre
//
// r,g,b = couleur du rectangle autour de CHAQUE lettre détectée
//
void detect_and_draw_letters_in_words(SDL_Surface *img,
                                      SDL_Surface *disp,
                                      int words_xmin, int words_xmax,
                                      int words_ymin, int words_ymax,
                                      uint8_t black_thresh,
                                      double line_fill_thresh,
                                      int gap_line,
                                      double col_fill_thresh,
                                      int gap_col,
                                      Uint8 r, Uint8 g, Uint8 b)
{
    // 1. On détecte les bandes horizontales (chaque mot)
    int band_count = 0;
    Band *bands = detect_word_bands(
        img,
        words_xmin, words_xmax,
        words_ymin, words_ymax,
        black_thresh,
        line_fill_thresh,
        gap_line,
        &band_count
    );

    // 2. Pour chaque bande (= mot), on découpe en lettres
    for (int b = 0; b < band_count; b++) {
        int by0 = bands[b].y_start;
        int by1 = bands[b].y_end;

        int letter_count = 0;
        LetterBox *letters = detect_letter_boxes_in_band(
            img,
            by0, by1,
            words_xmin, words_xmax,
            black_thresh,
            col_fill_thresh,
            gap_col,
            &letter_count
        );

        // 3. Pour chaque lettre détectée, on dessine un rectangle
        //    sur la surface couleur 'disp'
        for (int L = 0; L < letter_count; L++) {
            int lx0 = letters[L].x_start;
            int lx1 = letters[L].x_end;
            int ly0 = by0;
            int ly1 = by1;

            // petite sécurité : éviter les rectangles inversés
            if (lx0 < lx1 && ly0 < ly1) {
                draw_rect(disp, lx0, ly0, lx1, ly1, r, g, b);
            }
        }

        free(letters);
    }

    free(bands);
}


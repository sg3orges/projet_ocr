#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "detect_letterinword.h"
#include "detect_grid_letter.h"

// Creation d'un struct pour l'intervalle entre des zones
typedef struct 
{
    int start;
    int end;
} Interval;

// get_gray : lire un pixel en niveau de gris (0 = noir, 255 = blanc)
Uint8 get_gray(SDL_Surface *surf, int x, int y)
{
	//surf = image SDL
	//x, y = coordonnées du pixel à lire
    Uint8 *p = (Uint8 *)surf->pixels + y * surf->pitch + x * surf->format->BytesPerPixel;

    Uint32 pixel = 0;
    for (int k = 0; k < surf->format->BytesPerPixel; k++) 
    {
        ((Uint8*)&pixel)[k] = p[k];
    }

    Uint8 r,g,b;
    //Conversion en RGB
    SDL_GetRGB(pixel, surf->format, &r, &g, &b);
    return (Uint8)((r + g + b) / 3);
}

// draw_rect : dessine un contour rectangle coloré autour des zones

void draw_rect(SDL_Surface *surf, int x0, int y0, int x1, int y1, Uint8 R, Uint8 G, Uint8 B)
{
	//x0,y0 = coin superieur gauche
	//x1,y1 = coin inférieur droit
	//( R G B ) couleur du cadre
    if (!surf) 
    {
	    return;
    }
	//Cas de valeur invalide 
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= surf->w) x1 = surf->w - 1;
    if (y1 >= surf->h) y1 = surf->h - 1;
    if (x0 > x1 || y0 > y1) return;
	//determine la couleur
    Uint32 color = SDL_MapRGB(surf->format, R,G,B);

    // ligne du haut / bas
    for (int x = x0; x <= x1; x++) 
    {
        Uint8 *ptop = (Uint8*)surf->pixels + y0*surf->pitch + x*surf->format->BytesPerPixel;
        Uint8 *pbot = (Uint8*)surf->pixels + y1*surf->pitch + x*surf->format->BytesPerPixel;
        memcpy(ptop, &color, surf->format->BytesPerPixel);
        memcpy(pbot, &color, surf->format->BytesPerPixel);
    }

    // ligne gauche / droite
    for (int y = y0; y <= y1; y++)
    {
        Uint8 *pleft  = (Uint8*)surf->pixels + y*surf->pitch + x0*surf->format->BytesPerPixel;
        Uint8 *pright = (Uint8*)surf->pixels + y*surf->pitch + x1*surf->format->BytesPerPixel;
        memcpy(pleft,  &color, surf->format->BytesPerPixel);
        memcpy(pright, &color, surf->format->BytesPerPixel);
    }
}
//Esthetique on rajoute de l'épaisseur au rectangle coloré pour bien le distinguer
void draw_rect_thick(SDL_Surface *surf, int x0, int y0, int x1, int y1, Uint8 R, Uint8 G, Uint8 B, int thickness)
{
    for (int k = 0; k < thickness; k++) 
    {
        draw_rect(surf, x0 - k, y0 - k, x1 + k, y1 + k, R,G,B);
    }
}

// clone_to_rgba32 : crée une surface couleur 32 bits pour annoter
//surface initiale en gris donc création d'une map vierge pour annoter des rectangles couleurs
static SDL_Surface* clone_to_rgba32(SDL_Surface *src)
{
    SDL_Surface *dst = SDL_CreateRGBSurface(
        0,
        src->w,
        src->h,
        32,
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        0x00FF0000,
        0x0000FF00,
        0x000000FF,
        0xFF000000
#else
        0x000000FF,
        0x0000FF00,
        0x00FF0000,
        0xFF000000
#endif
    );

    if (!dst) 
    {
        printf("Erreur CreateRGBSurface: %s\n", SDL_GetError());
        return NULL;
    }

    SDL_Rect r = 
    {
	    0,0,src->w,src->h
    };
    if (SDL_BlitSurface(src, &r, dst, &r) != 0)
    {
        printf("Erreur: %s\n", SDL_GetError());
        SDL_FreeSurface(dst);
        return NULL;
    }

    return dst;
}

// compute_col_black_ratio : pour chaque colonne x, calcule le pourcentage de pixels "sombres" -> sert à repérer des blocs verticaux

static double* compute_col_black_ratio(SDL_Surface *surf, Uint8 thresh)
{
    int w = surf->w;//nombre total de colonnes
    int h = surf->h;//nombre total de lignes
    double *ratio = malloc(sizeof(double) * w);
    if (!ratio) 
    {
	    return NULL;
    }

    for (int x = 0; x < w; x++) //chaque colonne :
    {
        int black = 0;
        for (int y = 0; y < h; y++) //chaque ligne 
       	{
            Uint8 g = get_gray(surf, x, y);
            if (g < thresh) //compare le pixel avec le seuil thresh
	    {
		    black++;
	    }
        }
        ratio[x] = (double)black / (double)h; //Calcule du ration par colonne ( claire =0.0 | sombre de 0.3 à 0.6 )
    }
    return ratio;
}


static Interval* find_vertical_components(const double *col_ratio,int w, double col_thresh, int gap_max, int *out_count)
{
	//col_ratio = ratio de noir par colonne
	//w = largeur de l'image
	//col_thresh = seuil a partir duquel une colonne est "interessante"
	//gap_max = taille max d'une interuption avant coupure d'un bloc
	//out_count = nombre blocs trouvés
    Interval *components = malloc(sizeof(Interval)*w);
    if (!components) 
    {
        *out_count = 0;
        return NULL;
    }

    int count = 0; //bloc trouvé
    int in_block = 0; //bool savoir si on est dans un bloc ou non
    int block_start = 0; //premiere colonne du bloc
    int last_dark = -100000;//derniere colonne du bloc

    for (int x = 0; x < w; x++) //scanne toutes les colonnes de gauche a droite
    {
        if (col_ratio[x] > col_thresh) //si colonne sombre
       	{
            if (!in_block) //Si premiere on demarre un bloc
	    {
                in_block = 1;
                block_start = x;
            }
            last_dark = x; //met a jour
        } 
	else //colonne claire
       	{
            if (in_block) //on est dans un bloc
	    {
                if (x - last_dark > gap_max) //depasse le gap autorisé => bloc fini
		{
                    components[count].start = block_start;
                    components[count].end   = last_dark;
                    count++;
                    in_block = 0;
                }
            }
        }
    }

    if (in_block) //si on est encore dans une zone on l'a quitte
    {
        components[count].start = block_start;
        components[count].end   = last_dark;
        count++;
    }

    *out_count = count; //nombre de zone
    return components;//intervalle de ces zones
}

// compute_row_black_ratio : pour chaque ligne y, % de pixels sombres
// dans [xmin..xmax]. Sert à repérer le haut/bas utiles d'une zone.
static double* compute_row_black_ratio(SDL_Surface *surf, Uint8 thresh, int xmin, int xmax)
{
    int h = surf->h;
    double *ratio = malloc(sizeof(double)*h);
    if (!ratio)
    {
	    return NULL;
    }

    for (int y = 0; y < h; y++) //parcourt toutes les lignes
    {
        int black = 0; //nombre pixel noirs
        int total = 0; // nombre de pixels analysés
        for (int x = xmin; x <= xmax; x++) 
	{
            Uint8 g = get_gray(surf, x, y);
            if (g < thresh) 
	    {
		    black++;
	    }
            total++;
        }
        ratio[y] = (double)black / (double)total;
    }
    return ratio; // Profil horizontale cette fois de densité de noir sur l'image
}


// find_vertical_bounds : dans une zone [xmin..xmax], on cherche
// le y_min/y_max où l'image contient du "noir" (texte / grille).
// row_thresh = % min de noir pour dire "cette ligne est utile".
static void find_vertical_bounds(SDL_Surface *surf, Uint8 thresh, int xmin, int xmax, double row_thresh, int *out_ymin, int *out_ymax)
{

    double *row_ratio = compute_row_black_ratio(surf, thresh, xmin, xmax); //Calcule du profil verticale
    if (!row_ratio) 
    {
        *out_ymin = 0;
        *out_ymax = surf->h - 1;
        return;
    }

    int h = surf->h;
    int y0 = -1;//Premiere zone sombre trouvé
    int y1 = -1;//derniere zone sombre trouvé

    for (int y = 0; y < h; y++) //Parcourt de chaque ligne
    {
        if (row_ratio[y] > row_thresh)
       	{
            if (y0 < 0) 
	    {
		    y0 = y;
	    }
            y1 = y;
        }
    }

    free(row_ratio);
	//cas ou aucune ligne sombre a ete trouvé
    if (y0 < 0) {
        y0 = 0;
        y1 = h - 1;
    }

    *out_ymin = y0;
    *out_ymax = y1;
}



int main(int argc, char **argv)
{
    printf("\n==============================\n");
    printf(">>> detection2.c ACTIF <<<\n");
    printf("==============================\n");

    if (argc < 2) 
    {
        printf("Usage: %s image.bmp\n", argv[0]);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) 
    {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Surface *img = SDL_LoadBMP(argv[1]);
    if (!img) 
    {
        printf("SDL_LoadBMP error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int W = img->w;
    int H = img->h;
    // Analyse verticale globale pour trouver les 2 zones 

    double *col_ratio = compute_col_black_ratio(img, 128);
    if (!col_ratio)
    {
        printf("Erreur alloc col_ratio\n");
        SDL_FreeSurface(img);
        SDL_Quit();
        return 1;
    }

    int comp_count = 0;
    Interval *cols_comp = find_vertical_components(
        col_ratio,
        W,
        0.02,  // 2% de noir = colonne intéressante
        10,    // tolère petits trous
        &comp_count
    );
    free(col_ratio);

    if (comp_count < 2) 
    {
        printf("Pas assez de composantes détectées (%d)\n", comp_count);
        free(cols_comp);
        SDL_FreeSurface(img);
        SDL_Quit();
        return 1;
    }

    int words_xmin = cols_comp[0].start;
    int words_xmax = cols_comp[0].end;

    int grid_xmin = cols_comp[1].start;
    int grid_xmax = cols_comp[1].end;

    for (int i = 2; i < comp_count; i++) 
    {
        if (cols_comp[i].start < grid_xmin) grid_xmin = cols_comp[i].start;
        if (cols_comp[i].end   > grid_xmax) grid_xmax = cols_comp[i].end;
    }

    free(cols_comp);

    // Détecter y_min / y_max utiles dans les deux zones
    int words_ymin, words_ymax;
    int grid_ymin, grid_ymax;

    find_vertical_bounds(img, 128, words_xmin, words_xmax, 0.05, &words_ymin, &words_ymax);

    find_vertical_bounds(img, 128, grid_xmin, grid_xmax, 0.05, &grid_ymin, &grid_ymax);

    printf("\nZONE MOTS  : x=[%d,%d], y=[%d,%d]\n", words_xmin, words_xmax, words_ymin, words_ymax);
    printf("ZONE GRILLE: x=[%d,%d], y=[%d,%d]\n\n", grid_xmin, grid_xmax, grid_ymin, grid_ymax);

    // Création d'une surface de rendu couleur (32 bits)
    SDL_Surface *disp = clone_to_rgba32(img);
    if (!disp)
    {
        SDL_FreeSurface(img);
        SDL_Quit();
        return 1;
    }

    SDL_LockSurface(disp);

    //  On dessine les zones globales :
    
    draw_rect_thick(disp,
                    words_xmin, words_ymin,
                    words_xmax, words_ymax,
                    0,255,0,
                    1); // vert

    draw_rect_thick(disp,
                    grid_xmin, grid_ymin,
                    grid_xmax, grid_ymax,
                    255,0,0,
                    1); // rouge

    // 5. On affine légèrement la zone de la grille :
    //    -> on rentre de qques pixels pour ignorer la bordure externe
    int gx0 = grid_xmin + 8;
    int gy0 = grid_ymin + 8;
    int gx1 = grid_xmax - 8;
    int gy1 = grid_ymax - 8;

    if (gx0 < 0) gx0 = 0;
    if (gy0 < 0) gy0 = 0;
    if (gx1 >= W) gx1 = W-1;
    if (gy1 >= H) gy1 = H-1;
    if (gx0 >= gx1) { gx0 = grid_xmin; gx1 = grid_xmax; }
    if (gy0 >= gy1) { gy0 = grid_ymin; gy1 = grid_ymax; }

    // Dessine la zone affinée en MAGENTA (juste pour debug visuel)
    draw_rect_thick(disp,
                    gx0, gy0,
                    gx1, gy1,
                    255,0,255,
                    2); // magenta épais

    
    //    Détecter les lettres dans la GRILLE
    //    -> même principe que pour les mots mais appliqué à gx0..gx1,gy0..gy1
    //    Cette fonction va dessiner en BLEU ÉPAIS autour de chaque lettre.
  
    detect_letters_in_grid_like_words(img, disp,gx0, gy0,gx1, gy1,
        128,   // seuil noir
        0.05,  // line_fill_thresh (5% encre => ligne de texte)
        3,     // gap_line autorisé
        0.10,  // col_fill_thresh (10% encre => colonne de lettre)
        2,     // gap_col autorisé
        0,0,255 // bleu
    );

    //    Détecter les lettres dans la zone des MOTS (gauche)
    //    Même approche mais optimisée pour la colonne de mots.
    //    Dessine aussi en BLEU autour de chaque lettre.
    detect_and_draw_letters_in_words(img, disp, words_xmin, words_xmax, words_ymin, words_ymax,
        128,   // black_thresh
        0.05,  // line_fill_thresh
        3,     // gap_line
        0.10,  // col_fill_thresh
        2,     // gap_col
        0,0,255 // BLEU
    );

    SDL_UnlockSurface(disp);


    // Affichage SDL
    
    SDL_Window *win = SDL_CreateWindow( "Detection zones et lettres",SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H,0);
    if (!win) 
    {
        printf("SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_FreeSurface(disp);
        SDL_FreeSurface(img);
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren) 
    {
        printf("SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_FreeSurface(disp);
        SDL_FreeSurface(img);
        SDL_Quit();
        return 1;
    }

    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, disp);
    if (!tex) 
    {
        printf("SDL_CreateTextureFromSurface error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_FreeSurface(disp);
        SDL_FreeSurface(img);
        SDL_Quit();
        return 1;
    }

    int running = 1;
    while (running) 
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
       	{
            if (e.type == SDL_QUIT || e.type == SDL_KEYDOWN)
                running = 0;
        }

        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

    SDL_FreeSurface(disp);
    SDL_FreeSurface(img);

    SDL_Quit();
    return 0;
}


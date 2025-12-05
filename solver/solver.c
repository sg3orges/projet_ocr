#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "solver.h"

int CreaMatrice(const char *Fichier , char matrice[SOLVER_MAX][SOLVER_MAX])
{
    FILE *f = fopen(Fichier, "r");
    if(f == NULL)
    {
        printf("impossible d ouvrire le fichier");
        return 0;
    } 
    int ligne = 0;
    char ligneM[SOLVER_MAX];

    while (fgets(ligneM, sizeof(ligneM), f))//select line  in document f 
    {
        ligneM[strcspn(ligneM,"\n")] = '\0'; //supp \n in the line 
        if(strlen(ligneM)==0)
        {
            continue;
        }
        strcpy(matrice[ligne], ligneM); //copy the line in mat 
        ligne++;
    }
    
    fclose(f);

    return ligne;

}

int ChercheMot(const char *mot, char matrice[SOLVER_MAX][SOLVER_MAX], int nbLignes, int nbColonnes,
               int *ligneDebut, int *colDebut, int *ligneFin, int *colFin)
{
    int len = strlen(mot);

    
    int directions[8][2] = {
        {0, 1},   
        {0, -1},  
        {1, 0},   
        {-1, 0},  
        {1, 1},   
        {1, -1},  
        {-1, 1},  
        {-1, -1}  
    }; // all direction in mat

    for (int i = 0; i < nbLignes; i++)
    {
        for (int j = 0; j < nbColonnes; j++)
        {
            if (matrice[i][j] == mot[0])
            {
                for (int d = 0; d < 8; d++)
                {
                    int dx = directions[d][0];
                    int dy = directions[d][1];
                    int x = i, y = j;
                    int k;

                    for (k = 1; k < len; k++)
                    {
                        x += dx;
                        y += dy;

                        if (x < 0 || x >= nbLignes || y < 0 || y >= nbColonnes)
                            break;

                        if (matrice[x][y] != mot[k])
                            break;
                    }

                    if (k == len)
                    {
                        *ligneDebut = i;
                        *colDebut = j;
                        *ligneFin = x;
                        *colFin = y;
                        return 1;
                    }
                }
            }
        }
    }

    return 0; // word not found
}

void ConvertirMajuscules(char *mot)//toUP
{
    for (int i = 0; mot[i] != '\0'; i++)
    {
        if (mot[i] >= 'a' && mot[i] <= 'z')
        {
            mot[i] = mot[i] - 32;
        }
    }
}


// Parcourt toutes les lignes de words_file et affiche les coordonnées pour chaque mot
void solver_run_words(const char *grid_file, const char *words_file)
{
    char matrice[SOLVER_MAX][SOLVER_MAX];
    int nbLignes = CreaMatrice(grid_file, matrice);
    if (nbLignes <= 0) {
        printf("[solver] Grille vide ou introuvable (%s)\n", grid_file);
        return;
    }
    int nbColonnes = strlen(matrice[0]);

    FILE *f = fopen(words_file, "r");
    if (!f) {
        printf("[solver] Impossible d'ouvrir %s\n", words_file);
        return;
    }

    char ligne[SOLVER_MAX];
    while (fgets(ligne, sizeof(ligne), f)) {
        ligne[strcspn(ligne, "\n")] = '\0';
        if (ligne[0] == '\0')
            continue;
        ConvertirMajuscules(ligne);

        int li1 = -1, li2 = -1, co1 = -1, co2 = -1;
        if (ChercheMot(ligne, matrice, nbLignes, nbColonnes, &li1, &co1, &li2, &co2)) {
            printf("%s: (%d,%d)(%d,%d)\n", ligne, co1, li1, co2, li2);
        } else {
            printf("%s: Not found\n", ligne);
        }
    }
    fclose(f);
}

// Colorie les cases trouvées et sauvegarde l'image annotée
void highlight_words_on_image(const char *image_path,
                              const char *grid_file,
                              const char *words_file,
                              const char *bbox_file,
                              const char *output_path)
{
    // Lecture des bbox de lettres si disponibles
    typedef struct { int row, col, x0, y0, x1, y1; } CellPos;
    CellPos *cells = NULL;
    size_t cells_count = 0, cells_cap = 0;
    FILE *cfile = fopen("cells_coords.txt", "r");
    if (cfile) {
        char line[256];
        while (fgets(line, sizeof(line), cfile)) {
            if (line[0] == '#') continue;
            int r, c, x0, y0, x1, y1;
            if (sscanf(line, "%d %d %d %d %d %d", &r, &c, &x0, &y0, &x1, &y1) == 6) {
                if (cells_count >= cells_cap) {
                    cells_cap = cells_cap ? cells_cap * 2 : 128;
                    cells = realloc(cells, cells_cap * sizeof(CellPos));
                    if (!cells) break;
                }
                cells[cells_count++] = (CellPos){r, c, x0, y0, x1, y1};
            }
        }
        fclose(cfile);
    }

    char matrice[SOLVER_MAX][SOLVER_MAX];
    int nbLignes = CreaMatrice(grid_file, matrice);
    if (nbLignes <= 0) {
        printf("[highlight] Grille vide ou introuvable (%s)\n", grid_file);
        free(cells);
        return;
    }
    int nbColonnes = strlen(matrice[0]);

    int gx0=0, gy0=0, gx1=0, gy1=0;
    FILE *bbox = fopen(bbox_file, "r");
    if (bbox) {
        if (fscanf(bbox, "%d %d %d %d", &gx0, &gy0, &gx1, &gy1) != 4) {
            gx0 = gy0 = 0; gx1 = gy1 = 0;
        }
        fclose(bbox);
    } else {
        printf("[highlight] bbox introuvable (%s), annotation annulée.\n", bbox_file);
        free(cells);
        return;
    }

    GError *err = NULL;
    GdkPixbuf *pix = gdk_pixbuf_new_from_file(image_path, &err);
    if (!pix) {
        printf("[highlight] Impossible de charger l'image %s: %s\n", image_path, err ? err->message : "inconnue");
        if (err) g_error_free(err);
        free(cells);
        return;
    }

    int img_w = gdk_pixbuf_get_width(pix);
    int img_h = gdk_pixbuf_get_height(pix);
    if (gx1 <= gx0 || gy1 <= gy0 || gx1 >= img_w || gy1 >= img_h) {
        gx0 = 0; gy0 = 0; gx1 = img_w - 1; gy1 = img_h - 1;
    }

    double cell_w = (double)(gx1 - gx0 + 1) / nbColonnes;
    double cell_h = (double)(gy1 - gy0 + 1) / nbLignes;
    if (cell_w < 1.0) cell_w = 1.0;
    if (cell_h < 1.0) cell_h = 1.0;

    GdkPixbuf *dest = gdk_pixbuf_copy(pix);
    g_object_unref(pix);

    FILE *wf = fopen(words_file, "r");
    if (!wf) {
        printf("[highlight] Impossible d'ouvrir %s\n", words_file);
        g_object_unref(dest);
        return;
    }

    guchar *pixels = gdk_pixbuf_get_pixels(dest);
    int n_channels = gdk_pixbuf_get_n_channels(dest);
    int rowstride = gdk_pixbuf_get_rowstride(dest);

    char ligne[SOLVER_MAX];
    while (fgets(ligne, sizeof(ligne), wf)) {
        ligne[strcspn(ligne, "\n")] = '\0';
        if (ligne[0] == '\0')
            continue;
        ConvertirMajuscules(ligne);

        int li1 = -1, li2 = -1, co1 = -1, co2 = -1;
        if (!ChercheMot(ligne, matrice, nbLignes, nbColonnes, &li1, &co1, &li2, &co2))
            continue;

        int dx = (co2 > co1) ? 1 : (co2 < co1 ? -1 : 0);
        int dy = (li2 > li1) ? 1 : (li2 < li1 ? -1 : 0);
        int len = (int)strlen(ligne);

        int cx = co1, cy = li1;
        for (int k = 0; k < len; k++) {
            int x0, y0, x1, y1;
            // Si coords de lettres disponibles, utiliser celles-ci
            int found_cell = 0;
            if (cells) {
                for (size_t ci = 0; ci < cells_count; ci++) {
                    if (cells[ci].row == cy && cells[ci].col == cx) {
                        x0 = cells[ci].x0;
                        y0 = cells[ci].y0;
                        x1 = cells[ci].x1;
                        y1 = cells[ci].y1;
                        found_cell = 1;
                        break;
                    }
                }
            }
            if (!found_cell) {
                double cx_pix = gx0 + (cx + 0.5) * cell_w;
                double cy_pix = gy0 + (cy + 0.5) * cell_h;
                double half_w = cell_w * 0.4;
                double half_h = cell_h * 0.4;
                x0 = (int)(cx_pix - half_w);
                y0 = (int)(cy_pix - half_h);
                x1 = (int)(cx_pix + half_w);
                y1 = (int)(cy_pix + half_h);
            }
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 >= img_w) x1 = img_w - 1;
            if (y1 >= img_h) y1 = img_h - 1;

            for (int x = x0; x <= x1; x++) {
                guchar *p_top = pixels + y0 * rowstride + x * n_channels;
                guchar *p_bot = pixels + y1 * rowstride + x * n_channels;
                p_top[0] = 255; p_top[1] = 0; p_top[2] = 0;
                p_bot[0] = 255; p_bot[1] = 0; p_bot[2] = 0;
            }
            for (int y = y0; y <= y1; y++) {
                guchar *p_left = pixels + y * rowstride + x0 * n_channels;
                guchar *p_right = pixels + y * rowstride + x1 * n_channels;
                p_left[0] = 255; p_left[1] = 0; p_left[2] = 0;
                p_right[0] = 255; p_right[1] = 0; p_right[2] = 0;
            }

            cx += dx;
            cy += dy;
        }
    }
    fclose(wf);

    if (!gdk_pixbuf_save(dest, output_path, "png", &err, NULL)) {
        printf("[highlight] Sauvegarde échouée %s: %s\n", output_path, err ? err->message : "inconnue");
        if (err) g_error_free(err);
    } else {
        printf("[highlight] Image annotée : %s\n", output_path);
    }

    g_object_unref(dest);
    free(cells);
}

// Compatibilité: version sans arguments, lance sur GRID / GRID_Word
void solver_test(void)
{
    solver_run_words("GRID", "GRID_Word");
}

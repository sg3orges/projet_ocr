#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include "detection.h"
#include "networks.h"

// Gestion des conflits de macro MAX avec solver.h / glib
#ifdef MAX
#undef MAX
#endif
#include "solver.h"

static char *g_last_image_path = NULL;
static int g_grid_x0 = 0, g_grid_y0 = 0, g_grid_x1 = 0, g_grid_y1 = 0;
static int g_grid_bbox_set = 0;
static GtkWidget *g_detect_window = NULL;

static void on_detect_destroy(GtkWidget *widget, gpointer user_data)
{
    (void)user_data;
    if (g_detect_window == widget) g_detect_window = NULL;
}

static void on_overlay_close(GtkWidget *widget, gpointer user_data)
{
    (void)widget; (void)user_data;
    g_print("[Info] Fermeture demandee par l'utilisateur. Arret du processus.\n");
    exit(0);
}

typedef struct
{
    int col, row;
    int x0, y0, x1, y1;
} CellBBox;

void detect_letters_in_grid(GdkPixbuf *img, GdkPixbuf *disp,
                            int gx0,int gx1,int gy0,int gy1,
                            guint8 black_thr, guint8 R,guint8 G,guint8 B);
void detect_letters_in_words(GdkPixbuf *img, GdkPixbuf *disp,
                             int wx0,int wx1,int wy0,int wy1,
                             guint8 black_thr, guint8 R,guint8 G,guint8 B);

static inline int clampi(int v,int lo,int hi)
{
    return v<lo?lo:(v>hi?hi:v);
}

static inline guint8 get_gray_local(GdkPixbuf *pix,int x,int y)
{
    int n=gdk_pixbuf_get_n_channels(pix);
    guchar *p=gdk_pixbuf_get_pixels(pix)+y*gdk_pixbuf_get_rowstride(pix)+x*n;
    return (p[0]+p[1]+p[2])/3;
}

static void draw_rect(GdkPixbuf *pix,int x0,int y0,int x1,int y1,
                      guint8 R,guint8 G,guint8 B)
{
    int W=gdk_pixbuf_get_width(pix),H=gdk_pixbuf_get_height(pix);
    int n=gdk_pixbuf_get_n_channels(pix),rs=gdk_pixbuf_get_rowstride(pix);
    guchar *px=gdk_pixbuf_get_pixels(pix);
    x0=clampi(x0,0,W-1); x1=clampi(x1,0,W-1);
    y0=clampi(y0,0,H-1); y1=clampi(y1,0,H-1);

    for(int x=x0;x<=x1;x++)
    {
        guchar *t=px+y0*rs+x*n, *b=px+y1*rs+x*n;
        t[0]=R; t[1]=G; t[2]=B;
        b[0]=R; b[1]=G; b[2]=B;
    }
    for(int y=y0;y<=y1;y++)
    {
        guchar *l=px+y*rs+x0*n, *r=px+y*rs+x1*n;
        l[0]=R; l[1]=G; l[2]=B;
        r[0]=R; r[1]=G; r[2]=B;
    }
}

static void draw_line(GdkPixbuf *pix, int x0, int y0, int x1, int y1, int thickness, guint8 r, guint8 g, guint8 b)
{
    int w = gdk_pixbuf_get_width(pix);
    int h = gdk_pixbuf_get_height(pix);
    int n_channels = gdk_pixbuf_get_n_channels(pix);
    int rowstride = gdk_pixbuf_get_rowstride(pix);
    guchar *pixels = gdk_pixbuf_get_pixels(pix);

    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    for (;;) {
        for (int ty = y0 - thickness / 2; ty <= y0 + thickness / 2; ty++) {
            for (int tx = x0 - thickness / 2; tx <= x0 + thickness / 2; tx++) {
                if (tx >= 0 && tx < w && ty >= 0 && ty < h) {
                    guchar *p = pixels + ty * rowstride + tx * n_channels;
                    p[0] = r;
                    p[1] = g;
                    p[2] = b;
                }
            }
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Check if point P(x,y) is inside rectangle defined by corners p1, p2, p4 (p3 is opposite p1)
// Order from main: p1(TL), p3(TR), p4(BR), p2(BL) - wait, let's look at generation order:
// p1(TL), p2(BL), p3(TR), p4(BR) ? 
// In generation: 
// p1 = Start - ext + perp  (Top Left relative to direction)
// p3 = End + ext + perp    (Top Right)
// p4 = End + ext - perp    (Bottom Right)
// p2 = Start - ext - perp  (Bottom Left)
// So order 1 -> 3 -> 4 -> 2 is cyclic.
static int is_inside_quad(int x, int y, int p1x, int p1y, int p3x, int p3y, int p4x, int p4y, int p2x, int p2y)
{
    // Simple cross product check for convex quad
    // Edge 1-3
    if ((p3x - p1x)*(y - p1y) - (p3y - p1y)*(x - p1x) < 0) return 0;
    // Edge 3-4
    if ((p4x - p3x)*(y - p3y) - (p4y - p3y)*(x - p3x) < 0) return 0;
    // Edge 4-2
    if ((p2x - p4x)*(y - p4y) - (p2y - p4y)*(x - p4x) < 0) return 0;
    // Edge 2-1
    if ((p1x - p2x)*(y - p2y) - (p1y - p2y)*(x - p2x) < 0) return 0;
    return 1;
}

static void refine_box_position(GdkPixbuf *pix, int *p1x, int *p1y, int *p3x, int *p3y, int *p4x, int *p4y, int *p2x, int *p2y)
{
    int min_x = *p1x, max_x = *p1x;
    int min_y = *p1y, max_y = *p1y;
    int pts_x[] = {*p3x, *p4x, *p2x};
    int pts_y[] = {*p3y, *p4y, *p2y};

    for(int i=0; i<3; i++) {
        if(pts_x[i] < min_x) min_x = pts_x[i];
        if(pts_x[i] > max_x) max_x = pts_x[i];
        if(pts_y[i] < min_y) min_y = pts_y[i];
        if(pts_y[i] > max_y) max_y = pts_y[i];
    }
    
    // Clamp to image
    int W = gdk_pixbuf_get_width(pix);
    int H = gdk_pixbuf_get_height(pix);
    min_x = (min_x < 0) ? 0 : min_x;
    max_x = (max_x >= W) ? W-1 : max_x;
    min_y = (min_y < 0) ? 0 : min_y;
    max_y = (max_y >= H) ? H-1 : max_y;

    long long sum_x = 0, sum_y = 0;
    int count = 0;
    int n_chan = gdk_pixbuf_get_n_channels(pix);
    int rs = gdk_pixbuf_get_rowstride(pix);
    guchar *pixels = gdk_pixbuf_get_pixels(pix);

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            // fast check bounds roughly? No, use accurate.
            if (is_inside_quad(x, y, *p1x, *p1y, *p3x, *p3y, *p4x, *p4y, *p2x, *p2y)) {
                guchar *p = pixels + y * rs + x * n_chan;
                int gray = (p[0] + p[1] + p[2]) / 3;
                if (gray < 160) { // Dark pixel (ink)
                    sum_x += x;
                    sum_y += y;
                    count++;
                }
            }
        }
    }

    if (count > 20) { // Only if we found enough ink
        double center_x = (double)sum_x / count;
        double center_y = (double)sum_y / count;
        
        // Theoretical center
        double th_cx = (*p1x + *p4x) / 2.0; 
        double th_cy = (*p1y + *p4y) / 2.0; // Diagonals midpoint

        double dx = center_x - th_cx;
        double dy = center_y - th_cy;
        
        // Clamp shift to avoid jumping too far (e.g. max 50% of the box size)
        // Hardcoded clamp for safety: +/- 15 pixels is usually enough for alignment jitter
        if(dx > 20) dx = 20; if(dx < -20) dx = -20;
        if(dy > 20) dy = 20; if(dy < -20) dy = -20;

        *p1x += (int)dx; *p1y += (int)dy;
        *p2x += (int)dx; *p2y += (int)dy;
        *p3x += (int)dx; *p3y += (int)dy;
        *p4x += (int)dx; *p4y += (int)dy;
    }
}

static double *col_black_ratio_zone(GdkPixbuf *pix, guint8 thr,
                                     int x0, int x1)
{
    int W = gdk_pixbuf_get_width(pix);
    int H = gdk_pixbuf_get_height(pix);
    x0 = clampi(x0, 0, W-1);
    x1 = clampi(x1, 0, W-1);
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }

    int n = x1 - x0 + 1;
    double *r = calloc(n, sizeof(double));
    if (!r) return NULL;

    for (int i = 0; i < n; i++)
    {
        int x = x0 + i;
        int black = 0;
        for (int y = 0; y < H; y++)
            if (get_gray_local(pix, x, y) < thr)
                black++;
        r[i] = (double)black / (double)H;
    }
    return r;
}

static double autocorr_strength(const double *p, int n,
                                 int lag_min, int lag_max)
{
    if (n <= 0) return 0.0;
    if (lag_max >= n) lag_max = n-1;
    if (lag_min >= lag_max) return 0.0;

    double mean = 0.0; for (int i = 0; i < n; i++) mean += p[i]; mean /= (double)n;
    double var = 0.0;
    for (int i = 0; i < n; i++) { double d = p[i] - mean; var += d*d; }
    if (var <= 1e-12) return 0.0;

    double best = 0.0;
    for (int k = lag_min; k <= lag_max; k++)
    {
        double acc = 0.0;
        int cnt = 0;
        for (int i = 0; i + k < n; i++)
        {
            double a = p[i] - mean;
            double b = p[i+k] - mean;
            acc += a*b; cnt++;
        }
        if (cnt > 0)
        {
            double score = acc / (var * cnt);
            if (score > best) best = score;
        }
    }
    if (best < 0.0) best = 0.0;
    return best;
}

static double periodicity_score(const double *p, int n)
{
    if (n < 8) return 0.0;
    int lag_min = 3;
    int lag_max = n / 4;
    if (lag_max <= lag_min) return 0.0;
    return autocorr_strength(p, n, lag_min, lag_max);
}

// --------------------------------------------------
// Cell position persistence
// --------------------------------------------------
static void write_cell_positions(const char *root_dir, int nb_rows, int nb_cols)
{
    if (!root_dir || !g_grid_bbox_set || nb_rows <= 0 || nb_cols <= 0) return;
    double stepX = (double)(g_grid_x1 - g_grid_x0 + 1) / (double)nb_cols;
    double stepY = (double)(g_grid_y1 - g_grid_y0 + 1) / (double)nb_rows;

    char *pos_path = g_build_filename(root_dir, "CELLPOS", NULL);
    GString *out = g_string_new("");
    for (int r = 0; r < nb_rows; r++) {
        for (int c = 0; c < nb_cols; c++) {
            int x0 = g_grid_x0 + (int)floor(c * stepX);
            int y0 = g_grid_y0 + (int)floor(r * stepY);
            int x1 = g_grid_x0 + (int)floor((c + 1) * stepX) - 1;
            int y1 = g_grid_y0 + (int)floor((r + 1) * stepY) - 1;
            g_string_append_printf(out, "%d %d %d %d %d %d\n", c, r, x0, y0, x1, y1);
        }
    }
    GError *err = NULL;
    if (!g_file_set_contents(pos_path, out->str, -1, &err)) {
        g_printerr("[Warn] Impossible d'ecrire CELLPOS (%s): %s\n", pos_path, err->message);
        g_clear_error(&err);
    } else {
        g_print("[Info] CELLPOS genere -> %s\n", pos_path);
    }
    g_string_free(out, TRUE);
    g_free(pos_path);
}

static GPtrArray *load_cell_positions(const char *root_dir)
{
    if (!root_dir) return NULL;
    char *pos_path = g_build_filename(root_dir, "CELLPOS", NULL);
    gchar *data = NULL;
    gsize len = 0;
    GError *err = NULL;
    if (!g_file_get_contents(pos_path, &data, &len, &err)) {
        g_printerr("[Warn] Impossible de lire CELLPOS (%s): %s\n", pos_path, err->message);
        g_clear_error(&err);
        g_free(pos_path);
        return NULL;
    }
    g_free(pos_path);

    GPtrArray *arr = g_ptr_array_new_with_free_func(g_free);
    gchar **lines = g_strsplit(data, "\n", -1);
    for (int i = 0; lines[i]; i++) {
        int c, r, x0, y0, x1, y1;
        if (sscanf(lines[i], "%d %d %d %d %d %d", &c, &r, &x0, &y0, &x1, &y1) == 6) {
            CellBBox *cb = g_malloc(sizeof(CellBBox));
            cb->col = c; cb->row = r; cb->x0 = x0; cb->y0 = y0; cb->x1 = x1; cb->y1 = y1;
            g_ptr_array_add(arr, cb);
        }
    }
    g_strfreev(lines);
    g_free(data);
    return arr;
}

static CellBBox *lookup_cell_bbox(const GPtrArray *pos, int col, int row)
{
    if (!pos) return NULL;
    for (guint i = 0; i < pos->len; i++) {
        CellBBox *cb = g_ptr_array_index((GPtrArray *)pos, i);
        if (cb->col == col && cb->row == row) return cb;
    }
    return NULL;
}

typedef struct
{
    int col;
    int row;
    char letter;
} CellPrediction;

typedef struct
{
    char *word;
    int c1, r1, c2, r2;
    int found;
} SolveResult;

typedef struct { guint8 r, g, b; } WordColor;

static const WordColor WORD_COLORS[] = {
    {255, 0, 0},
    {0, 140, 255},
    {0, 180, 80},
    {255, 140, 0},
    {170, 0, 255},
    {0, 200, 200},
    {255, 0, 150},
    {120, 70, 0},
    {255, 200, 0},
    {0, 100, 0},
    {0, 0, 0},
};
static const int WORD_COLORS_LEN = (int)(sizeof(WORD_COLORS) / sizeof(WORD_COLORS[0]));

static inline WordColor word_color_for_index(int idx)
{
    if (idx < 0 || WORD_COLORS_LEN == 0) return (WordColor){255, 0, 0};
    return WORD_COLORS[idx % WORD_COLORS_LEN];
}

static int cmp_str_ptr(const void *a, const void *b)
{
    const char *sa = *(const char * const *)a;
    const char *sb = *(const char * const *)b;
    return g_strcmp0(sa, sb);
}

static int compare_cells_row_major(const void *a, const void *b)
{
    const CellPrediction *ca = *(const CellPrediction * const *)a;
    const CellPrediction *cb = *(const CellPrediction * const *)b;
    if (ca->row != cb->row) return ca->row - cb->row;
    return ca->col - cb->col;
}

static char predict_letter_for_cell(NeuralNetwork *net, const char *filepath)
{
    double conf = 0.0;
    char res = predict(net, filepath, &conf);
    if (res >= 'a' && res <= 'z') res = res - 32;
    return res;
}

static char *find_project_root(void)
{
    char *cwd = g_get_current_dir();
    if (!cwd) return NULL;
    for (int i = 0; i < 5; i++) {
        char *cells_path = g_build_filename(cwd, "cells", NULL);
        gboolean ok = g_file_test(cells_path, G_FILE_TEST_IS_DIR);
        g_free(cells_path);
        if (ok) return cwd;

        char *parent = g_path_get_dirname(cwd);
        if (!parent || g_strcmp0(parent, cwd) == 0) {
            g_free(parent);
            break;
        }
        g_free(cwd);
        cwd = parent;
    }
    g_free(cwd);
    return NULL;
}

static void write_gridl_file(const char *root_dir, const GPtrArray *cells, int max_row, int max_col)
{
    if (max_row <= 0 || max_col <= 0) return;
    char *grid = g_malloc0((size_t)max_row * (size_t)max_col);
    for (int i = 0; i < max_row * max_col; i++) grid[i] = '-';

    for (guint i = 0; i < cells->len; i++) {
        CellPrediction *cp = g_ptr_array_index((GPtrArray *)cells, i);
        int r = cp->row - 1;
        int c = cp->col - 1;
        if (r >= 0 && r < max_row && c >= 0 && c < max_col)
            grid[(size_t)r * (size_t)max_col + (size_t)c] = cp->letter;
    }

    GString *out = g_string_new("");
    for (int r = 0; r < max_row; r++) {
        g_string_append_len(out, grid + (size_t)r * (size_t)max_col, max_col);
        g_string_append_c(out, '\n');
    }

    char *gridl_path = root_dir ? g_build_filename(root_dir, "GRIDL", NULL) : g_strdup("GRIDL");
    GError *err = NULL;
    if (g_file_set_contents(gridl_path, out->str, -1, &err))
        g_print("[Info] Fichier GRIDL mis a jour (%dx%d) -> %s\n", max_col, max_row, gridl_path);
    else {
        g_printerr("[Error] Ecriture GRIDL echouee (%s): %s\n", gridl_path, err->message);
        g_clear_error(&err);
    }
    g_free(gridl_path);

    g_string_free(out, TRUE);
    g_free(grid);
}

static int detect_grid_bbox(GdkPixbuf *img, int *x0, int *y0, int *x1, int *y1);

static void show_solver_overlay(const GPtrArray *results, int nb_rows, int nb_cols)
{
    if (!g_last_image_path) return;
    
    // 1. Chargement de l'image
    GError *err = NULL;
    GdkPixbuf *img = gdk_pixbuf_new_from_file(g_last_image_path, &err);
    if (!img) { 
        g_printerr("[Error] Impossible de charger l'image pour overlay: %s\n", err->message);
        g_clear_error(&err); 
        return; 
    }
    
    // On travaille sur une copie pour ne pas altérer l'original en mémoire si besoin
    GdkPixbuf *disp = gdk_pixbuf_copy(img);

    // 2. Définition de la zone de la grille (BBox Globale)
    // On récupère les coordonnées globales de la grille détectée plus tôt
    int gx0 = 0, gy0 = 0, gx1 = 0, gy1 = 0;
    
    if (g_grid_bbox_set) {
        gx0 = g_grid_x0; 
        gy0 = g_grid_y0; 
        gx1 = g_grid_x1; 
        gy1 = g_grid_y1;
    } else {
        // Fallback : Si pas de grille détectée, on prend toute l'image (peu probable)
        gx0 = 0; gy0 = 0;
        gx1 = gdk_pixbuf_get_width(img) - 1;
        gy1 = gdk_pixbuf_get_height(img) - 1;
    }

    // 3. Calcul de la "Grille Mathématique"
    // C'est ici que tout se joue. On divise la zone globale par le nombre de cases.
    double grid_width  = (double)(gx1 - gx0);
    double grid_height = (double)(gy1 - gy0);

    // Sécurité division par zéro
    if (nb_cols < 1) nb_cols = 1;
    if (nb_rows < 1) nb_rows = 1;

    double cell_w = grid_width / (double)nb_cols;
    double cell_h = grid_height / (double)nb_rows;

    // --- PARAMETRES DE STYLE ---
    // 0.80 = Le rectangle occupe 80% de la largeur de la case (laisse une marge)
    // Augmentez à 0.90 si vous voulez que ça se touche presque.
    const double BOX_SCALE = 0.80; 
    const int LINE_WIDTH = 3; // Epaisseur du trait de contour

    // On calcule la "demi-taille" de référence pour dessiner les boites
    // On prend le min pour que les rectangles ne débordent pas si les cases sont rectangulaires
    double half_size_ref = (cell_w < cell_h ? cell_w : cell_h) * 0.5 * BOX_SCALE;
    
    // Pour la longueur, on veut aller un peu plus loin pour bien englober la lettre
    double half_long_ref = (cell_w > cell_h ? cell_w : cell_h) * 0.5 * 0.95; 

    for (guint i = 0; i < results->len; i++) {
        SolveResult *sr = g_ptr_array_index((GPtrArray *)results, i);
        if (!sr->found) continue;

        WordColor col = word_color_for_index((int)i);

        // Coordonnées Logiques (Col, Row)
        int c1 = sr->c1; int r1 = sr->r1;
        int c2 = sr->c2; int r2 = sr->r2;

        // 4. Calcul des Centres Géométriques (Pixel)
        // Le centre est : DébutGrille + (IndexColonne + 0.5) * LargeurCase
        double cx_start = gx0 + ((double)c1 + 0.5) * cell_w;
        double cy_start = gy0 + ((double)r1 + 0.5) * cell_h;

        double cx_end   = gx0 + ((double)c2 + 0.5) * cell_w;
        double cy_end   = gy0 + ((double)r2 + 0.5) * cell_h;

        // 5. Vecteurs de direction (pour l'orientation du rectangle)
        double vx = cx_end - cx_start;
        double vy = cy_end - cy_start;
        double len = sqrt(vx*vx + vy*vy);

        // Gestion du cas mot d'une lettre (rare mais crash possible)
        if (len < 0.1) { vx = 1.0; vy = 0.0; len = 1.0; }

        double ux = vx / len; // Vecteur Unitaire Direction
        double uy = vy / len; 
        double px = -uy;      // Vecteur Unitaire Perpendiculaire
        double py = ux;

        // 6. Définir les dimensions du rectangle
        // "Rayon" longitudinal (dans le sens du mot)
        // On rajoute la moitié d'une case au début et à la fin pour englober les lettres extrêmes
        double rad_long = (len / 2.0) + half_long_ref;
        
        // "Rayon" latéral (épaisseur du mot)
        double rad_wide = half_size_ref;

        // Centre absolu du mot entier
        double mid_x = (cx_start + cx_end) / 2.0;
        double mid_y = (cy_start + cy_end) / 2.0;

        // 7. Calcul des 4 sommets
        // P1 ----- P3
        //  |       |
        // P2 ----- P4
        
        int p1x = (int)(mid_x - ux * rad_long + px * rad_wide);
        int p1y = (int)(mid_y - uy * rad_long + py * rad_wide);

        int p3x = (int)(mid_x + ux * rad_long + px * rad_wide);
        int p3y = (int)(mid_y + uy * rad_long + py * rad_wide);

        int p4x = (int)(mid_x + ux * rad_long - px * rad_wide);
        int p4y = (int)(mid_y + uy * rad_long - py * rad_wide);
        
        int p2x = (int)(mid_x - ux * rad_long - px * rad_wide);
        int p2y = (int)(mid_y - uy * rad_long - py * rad_wide);

        // 8. Dessin
        draw_line(disp, p1x, p1y, p3x, p3y, LINE_WIDTH, col.r, col.g, col.b);
        draw_line(disp, p3x, p3y, p4x, p4y, LINE_WIDTH, col.r, col.g, col.b);
        draw_line(disp, p4x, p4y, p2x, p2y, LINE_WIDTH, col.r, col.g, col.b);
        draw_line(disp, p2x, p2y, p1x, p1y, LINE_WIDTH, col.r, col.g, col.b);
    }

    // Affichage Fenêtre
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Solution (Grille Théorique)");
    // On limite la taille par défaut pour éviter les fenêtres géantes
    gtk_window_set_default_size(GTK_WINDOW(win), 
                                MIN(gdk_pixbuf_get_width(disp), 1200), 
                                MIN(gdk_pixbuf_get_height(disp), 900));
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(win), box);

    // Image dans un ScrolledWindow au cas où elle est très grande
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);
    
    GtkWidget *imgw = gtk_image_new_from_pixbuf(disp);
    gtk_container_add(GTK_CONTAINER(scroll), imgw);

    GtkWidget *btn_close = gtk_button_new_with_label("Fermer");
    gtk_box_pack_start(GTK_BOX(box), btn_close, FALSE, FALSE, 5);
    g_signal_connect(btn_close, "clicked", G_CALLBACK(on_overlay_close), NULL);

    gtk_widget_show_all(win);

    g_object_unref(img);
    g_object_unref(disp);
    // Note: on ne libère plus cell_positions ni root_dir ici car on ne les utilise plus
    char *rd = find_project_root(); // Juste pour clean propre si besoin, sinon inutile
    if(rd) g_free(rd);
}
static int detect_grid_bbox(GdkPixbuf *img, int *x0, int *y0, int *x1, int *y1)
{
    int W = gdk_pixbuf_get_width(img);
    int H = gdk_pixbuf_get_height(img);
    int n = gdk_pixbuf_get_n_channels(img);
    int rs = gdk_pixbuf_get_rowstride(img);
    guchar *px = gdk_pixbuf_get_pixels(img);

    double *col = g_malloc0(sizeof(double) * (size_t)W);
    double *row = g_malloc0(sizeof(double) * (size_t)H);

    for (int y = 0; y < H; y++) {
        guchar *line = px + y * rs;
        for (int x = 0; x < W; x++) {
            guchar *p = line + x * n;
            int gray = (p[0] + p[1] + p[2]) / 3;
            if (gray < 140) {
                col[x] += 1.0;
                row[y] += 1.0;
            }
        }
    }

    // 1. HORIZONTAL SEGMENTATION (Find the widest block of dense columns)
    double maxc = 0.0;
    for (int x = 0; x < W; x++) if (col[x] > maxc) maxc = col[x];
    double tc = maxc * 0.20; // Lower threshold to capture the full grid width but ignore noise

    int best_lx = 0, best_rx = W - 1;
    int max_width = 0;
    
    int cur_start = -1;
    for (int x = 0; x < W; x++) {
        if (col[x] >= tc) {
            if (cur_start == -1) cur_start = x;
        } else {
            if (cur_start != -1) {
                int width = x - cur_start;
                if (width > max_width) {
                    max_width = width;
                    best_lx = cur_start;
                    best_rx = x - 1;
                }
                cur_start = -1;
            }
        }
    }
    // Check last horizontal segment
    if (cur_start != -1) {
        int width = W - cur_start;
         if (width > max_width) {
            max_width = width;
            best_lx = cur_start;
            best_rx = W - 1;
        }
    }
    
    // 2. VERTICAL SEGMENTATION (Same logic for rows)
    double maxr = 0.0;
    for (int y = 0; y < H; y++) if (row[y] > maxr) maxr = row[y];
    double tr = maxr * 0.20;

    int best_ty = 0, best_by = H - 1;
    int max_height = 0;
    int cur_start_y = -1;
    
    for (int y = 0; y < H; y++) {
        if (row[y] >= tr) {
             if (cur_start_y == -1) cur_start_y = y;
        } else {
            if (cur_start_y != -1) {
                int h_seg = y - cur_start_y;
                if (h_seg > max_height) {
                    max_height = h_seg;
                    best_ty = cur_start_y;
                    best_by = y - 1;
                }
                cur_start_y = -1;
            }
        }
    }
    // Check last vertical segment
    if (cur_start_y != -1) {
        int h_seg = H - cur_start_y;
        if (h_seg > max_height) {
             max_height = h_seg;
             best_ty = cur_start_y;
             best_by = H - 1;
        }
    }

    g_free(col);
    g_free(row);

    // Apply results
    *x0 = best_lx; *x1 = best_rx; 
    *y0 = best_ty; *y1 = best_by;
    
    if (*x1 <= *x0 || *y1 <= *y0) return 0;
    return 1;
}

static int solve_words_in_grid(const char *root_dir,
                               GPtrArray **out_results, int *out_rows, int *out_cols)
{
    if (out_results) *out_results = NULL;
    if (out_rows) *out_rows = 0;
    if (out_cols) *out_cols = 0;
    if (!root_dir) return 0;

    char *grid_path = g_build_filename(root_dir, "GRIDL", NULL);
    char *words_path = g_build_filename(root_dir, "GRIDWO", NULL);

    char matrice[MAX_MAT][MAX_MAT]; // Updated to MAX_MAT
    int nbLignes = CreaMatrice(grid_path, matrice);
    if (nbLignes <= 0) {
        g_printerr("[Error] Lecture GRIDL echouee (%s)\n", grid_path);
        g_free(grid_path);
        g_free(words_path);
        return 0;
    }
    int nbColonnes = (int)strlen(matrice[0]);
    if (out_rows) *out_rows = nbLignes;
    if (out_cols) *out_cols = nbColonnes;

    FILE *fw = fopen(words_path, "r");
    if (!fw) {
        g_printerr("[Error] Lecture GRIDWO echouee (%s)\n", words_path);
        g_free(grid_path);
        g_free(words_path);
        return 0;
    }

    GPtrArray *results = g_ptr_array_new_with_free_func(g_free);
    char line[256];
    while (fgets(line, sizeof(line), fw)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        ConvertirMajuscules(line);

        int li1=-1, li2=-1, co1=-1, co2=-1;
        int found = ChercheMot(line, matrice, nbLignes, nbColonnes, &li1, &co1, &li2, &co2);
        if (found)
            g_print("[SOLVE] %s -> (%d,%d)(%d,%d)\n", line, co1, li1, co2, li2);
        else
            g_print("[SOLVE] %s -> Not found\n", line);

        SolveResult *sr = g_malloc(sizeof(SolveResult));
        sr->word = g_strdup(line);
        sr->c1 = co1; sr->r1 = li1; sr->c2 = co2; sr->r2 = li2;
        sr->found = found;
        g_ptr_array_add(results, sr);
    }

    fclose(fw);
    if (out_results) *out_results = results;
    else g_ptr_array_free(results, TRUE);

    g_free(grid_path);
    g_free(words_path);
    return 1;
}

static void run_solver_pipeline(void)
{
    char *root_dir = find_project_root();
    if (!root_dir) {
        g_printerr("[Error] Impossible de localiser le dossier 'cells'. Abandon.\n");
        return;
    }

    NeuralNetwork net;
    init_network(&net);

    const char *save_file = "neuronne/brain.bin";
    if (!load_network(&net, save_file)) {
        g_printerr("[Warn] Aucune sauvegarde du reseau. Entrainement en cours...\n");
        train_network(&net, "neuronne/dataset");
        save_network(&net, save_file);
    }

    GPtrArray *cells = g_ptr_array_new_with_free_func(g_free);
    char *cells_path = g_build_filename(root_dir, "cells", NULL);
    GDir *dir = g_dir_open(cells_path, 0, NULL);
    if (!dir) {
        g_printerr("[Error] Impossible d'ouvrir le dossier cells (%s)\n", cells_path);
        g_free(cells_path);
        g_free(root_dir);
        g_ptr_array_free(cells, TRUE);
        cleanup(&net);
        return;
    }

    int max_row = 0, max_col = 0;
    int cell_w = 0, cell_h = 0;
    const char *name = NULL;
    while ((name = g_dir_read_name(dir)) != NULL) {
        int col = 0, row = 0;
        if (sscanf(name, "%d_%d", &col, &row) != 2)
            continue;

        char *fullpath = g_build_filename(cells_path, name, NULL);
        if (cell_w == 0 || cell_h == 0) {
            int w = 0, h = 0;
            if (gdk_pixbuf_get_file_info(fullpath, &w, &h)) {
                cell_w = w;
                cell_h = h;
            }
        }

        char letter = predict_letter_for_cell(&net, fullpath);

        CellPrediction *cp = g_malloc(sizeof(CellPrediction));
        cp->col = col;
        cp->row = row;
        cp->letter = letter;
        g_ptr_array_add(cells, cp);

        if (row > max_row) max_row = row;
        if (col > max_col) max_col = col;

        g_free(fullpath);
    }
    g_dir_close(dir);
    g_free(cells_path);

    if (cells->len == 0) {
        g_printerr("[Error] Aucun fichier dans cells/*\n");
        g_ptr_array_free(cells, TRUE);
        cleanup(&net);
        g_free(root_dir);
        return;
    }

    qsort(cells->pdata, cells->len, sizeof(gpointer), compare_cells_row_major);
    write_gridl_file(root_dir, cells, max_row, max_col);
    write_cell_positions(root_dir, max_row, max_col);

    g_ptr_array_free(cells, TRUE);

    char *words_dir = g_build_filename(root_dir, "letterInWord", NULL);
    GDir *words = g_dir_open(words_dir, 0, NULL);
    if (!words) {
        g_printerr("[Warn] Dossier letterInWord introuvable (%s), GRIDWO non genere.\n", words_dir);
    } else {
        GPtrArray *word_names = g_ptr_array_new_with_free_func(g_free);
        const char *wname = NULL;
        while ((wname = g_dir_read_name(words)) != NULL) {
            if (g_str_has_prefix(wname, "word_"))
                g_ptr_array_add(word_names, g_strdup(wname));
        }
        g_dir_close(words);

        if (word_names->len == 0) {
            g_printerr("[Warn] Aucun dossier word_* dans letterInWord, GRIDWO vide.\n");
        } else {
            qsort(word_names->pdata, word_names->len, sizeof(gpointer), cmp_str_ptr);
            GString *out_words = g_string_new("");

            for (guint wi = 0; wi < word_names->len; wi++) {
                char *word_path = g_build_filename(words_dir, (char *)g_ptr_array_index(word_names, wi), NULL);
                GDir *letters_dir = g_dir_open(word_path, 0, NULL);
                if (!letters_dir) {
                    g_printerr("[Warn] Impossible d'ouvrir %s\n", word_path);
                    g_free(word_path);
                    continue;
                }

                GPtrArray *letters = g_ptr_array_new_with_free_func(g_free);
                const char *lname = NULL;
                while ((lname = g_dir_read_name(letters_dir)) != NULL) {
                    if (g_str_has_prefix(lname, "letter_"))
                        g_ptr_array_add(letters, g_strdup(lname));
                }
                g_dir_close(letters_dir);

                qsort(letters->pdata, letters->len, sizeof(gpointer), cmp_str_ptr);
                GString *word_line = g_string_new("");
                for (guint li = 0; li < letters->len; li++) {
                    char *lpath = g_build_filename(word_path, (char *)g_ptr_array_index(letters, li), NULL);
                    char letter = predict_letter_for_cell(&net, lpath);
                    g_string_append_c(word_line, letter);
                    g_free(lpath);
                }
                g_ptr_array_free(letters, TRUE);
                g_free(word_path);

                g_string_append(out_words, word_line->str);
                g_string_append_c(out_words, '\n');
                g_string_free(word_line, TRUE);
            }

            char *gridwo_path = g_build_filename(root_dir, "GRIDWO", NULL);
            GError *err = NULL;
            if (g_file_set_contents(gridwo_path, out_words->str, -1, &err))
                g_print("[Info] Fichier GRIDWO genere -> %s\n", gridwo_path);
            else {
                g_printerr("[Error] Ecriture GRIDWO echouee (%s): %s\n", gridwo_path, err->message);
                g_clear_error(&err);
            }
            g_free(gridwo_path);
            g_string_free(out_words, TRUE);
        }
        g_ptr_array_free(word_names, TRUE);
    }
    g_free(words_dir);

    GPtrArray *results = NULL;
    int nb_rows = 0, nb_cols = 0;
    if (solve_words_in_grid(root_dir, &results, &nb_rows, &nb_cols)) {
        if (results) {
            show_solver_overlay(results, nb_rows, nb_cols);
            g_ptr_array_free(results, TRUE);
        }
    }

    cleanup(&net);
    g_free(root_dir);
}

typedef struct
{
    int start, end;
    double avg;
} Segment;

static void trim_zone(GdkPixbuf *pix, int *x0, int *x1, int *y0, int *y1, guint8 thr)
{
    int W = gdk_pixbuf_get_width(pix);
    int H = gdk_pixbuf_get_height(pix);
    int local_x0 = *x0, local_x1 = *x1, local_y0 = *y0, local_y1 = *y1;
    int found_top = local_y0;
    for (int y = local_y0; y <= local_y1; y++) {
        int black_count = 0;
        for (int x = local_x0; x <= local_x1; x++) {
            if (get_gray_local(pix, x, y) < thr) black_count++;
        }
        if (black_count > (local_x1 - local_x0) * 0.02) {
            found_top = y;
            break;
        }
    }
    
    int found_bot = local_y1;
    for (int y = local_y1; y >= found_top; y--) {
        int black_count = 0;
        for (int x = local_x0; x <= local_x1; x++) {
            if (get_gray_local(pix, x, y) < thr) black_count++;
        }
        if (black_count > (local_x1 - local_x0) * 0.02) {
            found_bot = y;
            break;
        }
    }

    int found_left = local_x0;
    for (int x = local_x0; x <= local_x1; x++) {
        int black_count = 0;
        for (int y = found_top; y <= found_bot; y++) {
            if (get_gray_local(pix, x, y) < thr) black_count++;
        }
        if (black_count > 2) { 
            found_left = x;
            break;
        }
    }

    int found_right = local_x1;
    for (int x = local_x1; x >= found_left; x--) {
        int black_count = 0;
        for (int y = found_top; y <= found_bot; y++) {
            if (get_gray_local(pix, x, y) < thr) black_count++;
        }
        if (black_count > 2) {
            found_right = x;
            break;
        }
    }

    if (found_left < found_right && found_top < found_bot) {
        *x0 = found_left;
        *x1 = found_right;
        *y0 = found_top;
        *y1 = found_bot;
    }
}

static void find_zones(GdkPixbuf *pix,
                       int *gx0, int *gx1, int *gy0, int *gy1,
                       int *wx0, int *wx1, int *wy0, int *wy1)
{
    const guint8 thr = 160;
    int W = gdk_pixbuf_get_width(pix);
    int H = gdk_pixbuf_get_height(pix);

    *gx0 = 0; *gx1 = W-1; *gy0 = 0; *gy1 = H-1;
    *wx0 = 0; *wx1 = 0;   *wy0 = 0; *wy1 = 0;

    if (W < 10 || H < 10) return;

    double *proj_x = calloc(W, sizeof(double));
    for (int x = 0; x < W; x++) {
        int cnt = 0;
        for (int y = 0; y < H; y++) {
            if (get_gray_local(pix, x, y) < thr) cnt++;
        }
        proj_x[x] = (double)cnt / H;
    }

    int smooth_rad = 10; 
    double *smooth_x = calloc(W, sizeof(double));
    double glob_avg = 0;
    for (int x = 0; x < W; x++) {
        double sum = 0;
        int c = 0;
        for (int k = -smooth_rad; k <= smooth_rad; k++) {
            if (x+k >= 0 && x+k < W) {
                sum += proj_x[x+k];
                c++;
            }
        }
        smooth_x[x] = (c > 0) ? sum/c : 0;
        glob_avg += smooth_x[x];
    }
    glob_avg /= W;
    double threshold = glob_avg * 0.5; 
    if (threshold < 0.02) threshold = 0.02;

    typedef struct { int start, end; double score; } Block;
    Block blocks[10]; 
    int nb_blocks = 0;

    int in_block = 0;
    int start = 0;
    double score_acc = 0;

    for (int x = 0; x < W; x++) {
        if (smooth_x[x] > threshold) {
            if (!in_block) { in_block = 1; start = x; score_acc = smooth_x[x]; }
            else { score_acc += smooth_x[x]; }
        } else {
            if (in_block) {
                if (nb_blocks < 10 && (x - start) > W/20) {
                    blocks[nb_blocks++] = (Block){start, x-1, score_acc};
                }
                in_block = 0;
            }
        }
    }
    if (in_block && nb_blocks < 10 && (W - start) > W/20) {
        blocks[nb_blocks++] = (Block){start, W-1, score_acc};
    }

    free(proj_x);
    free(smooth_x);
    int grid_idx = -1;
    double max_score = -1.0;

    for (int i = 0; i < nb_blocks; i++) {
        if (blocks[i].score > max_score) {
            max_score = blocks[i].score;
            grid_idx = i;
        }
    }

    if (grid_idx != -1) {
        *gx0 = blocks[grid_idx].start;
        *gx1 = blocks[grid_idx].end;
        
        int *proj_y = calloc(H, sizeof(int));
        for (int y = 0; y < H; y++) {
            for (int x = *gx0; x <= *gx1; x++) {
                if (get_gray_local(pix, x, y) < thr) proj_y[y]++;
            }
        }
        
        int top = 0, bot = H-1;
        while (top < H && proj_y[top] < (*gx1 - *gx0)*0.05) top++;
        while (bot > 0 && proj_y[bot] < (*gx1 - *gx0)*0.05) bot--;
        
        if (top < bot) { *gy0 = top; *gy1 = bot; }
        free(proj_y);
        trim_zone(pix, gx0, gx1, gy0, gy1, thr);

        int word_idx = -1;
        max_score = -1.0;
        for (int i = 0; i < nb_blocks; i++) {
            if (i == grid_idx) continue;
            if (blocks[i].start > *gx0 && blocks[i].end < *gx1) continue; 
            
            if (blocks[i].score > max_score) {
                max_score = blocks[i].score;
                word_idx = i;
            }
        }

        if (word_idx != -1) {
            *wx0 = blocks[word_idx].start;
            *wx1 = blocks[word_idx].end;
            *wy0 = 0; *wy1 = H-1;

        }
    } else {
        *gx0 = W/4; *gx1 = 3*W/4; *gy0 = H/4; *gy1 = 3*H/4;
    }
}

static void on_solve_clicked(GtkWidget *widget, gpointer user_data)
{
    GtkWidget *win = GTK_WIDGET(user_data);
    (void)widget;
    g_print("[Info] Bouton Resoudre clique : generation de GRIDL via reseau de neurones...\n");
    run_solver_pipeline();
    if (win) gtk_widget_hide(win);
}

static void run_detection(GtkWidget *win,const char *path)
{
    g_detect_window = win;
    g_signal_connect(win, "destroy", G_CALLBACK(on_detect_destroy), NULL);

    if (g_last_image_path) {
        g_free(g_last_image_path);
        g_last_image_path = NULL;
    }
    g_last_image_path = g_strdup(path);

    GError *err=NULL;
    GdkPixbuf *img=gdk_pixbuf_new_from_file(path,&err);
    if(!img)
    {
        g_printerr("Erreur: %s\n",err->message);
        g_clear_error(&err);
        return;
    }

    GdkPixbuf *disp=gdk_pixbuf_copy(img);
    int gx0,gx1,gy0,gy1, wx0,wx1,wy0,wy1;
    find_zones(img,&gx0,&gx1,&gy0,&gy1,&wx0,&wx1,&wy0,&wy1);

    g_grid_x0 = gx0; g_grid_y0 = gy0; g_grid_x1 = gx1; g_grid_y1 = gy1;
    g_grid_bbox_set = 1;

    printf("ZONE GRILLE: x=[%d,%d], y=[%d,%d]\n",gx0,gx1,gy0,gy1);
    printf("ZONE MOTS  : x=[%d,%d], y=[%d,%d]\n",wx0,wx1,wy0,wy1);

    draw_rect(disp,gx0,gy0,gx1,gy1,255,0,0);
    draw_rect(disp,wx0,wy0,wx1,wy1,0,255,0);

    const guint8 BLACK_T=160;
    detect_letters_in_grid(img,disp,gx0,gx1,gy0,gy1,BLACK_T,0,128,255);
    detect_letters_in_words(img,disp,wx0,wx1,wy0,wy1,BLACK_T,0,128,255);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(win), box);

    GtkWidget *imgw=gtk_image_new_from_pixbuf(disp);
    gtk_box_pack_start(GTK_BOX(box), imgw, TRUE, TRUE, 0);

    GtkWidget *btn_solve = gtk_button_new_with_label("Resoudre");
    g_signal_connect(btn_solve, "clicked", G_CALLBACK(on_solve_clicked), win);
    gtk_box_pack_start(GTK_BOX(box), btn_solve, FALSE, FALSE, 4);

    gtk_widget_show_all(win);

    g_object_unref(img);
    g_object_unref(disp);
}

static void on_open(GApplication *app,GFile **files,int n,const char *hint)
{
    (void)n; (void)hint;
    GtkWidget *win=gtk_application_window_new(GTK_APPLICATION(app));
    gtk_window_set_title(GTK_WINDOW(win),"Détection grille et mots (GTK3)");
    gtk_window_set_default_size(GTK_WINDOW(win),1100,800);
    char *path=g_file_get_path(files[0]);
    run_detection(win,path);
    g_free(path);
}

int detection_run_app(int argc,char **argv)
{
    GtkApplication *app=gtk_application_new("com.detect.auto",G_APPLICATION_HANDLES_OPEN);
    g_signal_connect(app,"open",G_CALLBACK(on_open),NULL);
    int status=g_application_run(G_APPLICATION(app),argc,argv);
    g_object_unref(app);
    return status;
}
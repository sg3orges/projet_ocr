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

static void draw_capsule(GdkPixbuf *pix, int x1, int y1, int x2, int y2, int r,
                         guint8 R, guint8 G, guint8 B)
{
    int W = gdk_pixbuf_get_width(pix);
    int H = gdk_pixbuf_get_height(pix);
    int n = gdk_pixbuf_get_n_channels(pix);
    int rs = gdk_pixbuf_get_rowstride(pix);
    guchar *pixels = gdk_pixbuf_get_pixels(pix);

    int min_x = clampi(fmin(x1, x2) - r, 0, W - 1);
    int max_x = clampi(fmax(x1, x2) + r, 0, W - 1);
    int min_y = clampi(fmin(y1, y2) - r, 0, H - 1);
    int max_y = clampi(fmax(y1, y2) + r, 0, H - 1);

    float dx = x2 - x1;
    float dy = y2 - y1;
    float len_sq = dx*dx + dy*dy;
    float len = sqrtf(len_sq);

    // Normalized segment vector
    float ux = (len > 0) ? dx / len : 0;
    float uy = (len > 0) ? dy / len : 0;

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            float px = x - x1;
            float py = y - y1;

            // Project point onto line (clamped to segment)
            float t = px*ux + py*uy;
            t = fmaxf(0.0f, fminf(len, t));

            float closest_x = x1 + t*ux;
            float closest_y = y1 + t*uy;

            float dist_sq = (x - closest_x)*(x - closest_x) + (y - closest_y)*(y - closest_y);

            if (dist_sq <= r*r) {
                // Determine alpha for anti-aliasing (simple edge blending)
                // Using 1.0 thickness for AA ring
                // float dist = sqrtf(dist_sq);
                // float alpha = 1.0f - fmaxf(0.0f, fminf(1.0f, dist - (r - 1.0f)));
                
                // For simplicity, just solid color first or simple alpha blend 
                // if we want it to look nice (but user asked for pure surround)
                // Let's do simple alpha blending to look correct on top of image
                // Or just solid overwrite? The prototype asked for "surround", usually means outline or solid highlight?
                // "entoure tout les mot" -> circle/capsule around. 
                // Let's do a hollow capsule? Or semi-transparent?
                // "entoure" usually means outline.
                // If I draw a filled capsule it acts like a highlighter marker.
                // If I draw outline, it's a boundary.
                // Let's try Outline Capsule logic.
                
                // Outline only:
                // float inner_r = r - 3.0f; 
                // if (dist_sq <= r*r && dist_sq >= inner_r*inner_r) { ... }
                
                // User said "entoure" (encircle/surround). 
                // A hollow capsule of thickness ~3px is best.
                
                float dist = sqrtf(dist_sq);
                if (dist >= r - 3.0f && dist <= r) {
                     guchar *p = pixels + y*rs + x*n;
                     p[0] = R; p[1] = G; p[2] = B;
                }
            }
        }
    }
}

static void write_cell_positions(const char *root_dir, int nb_rows, int nb_cols)
{
    // Legacy function - kept empty or just does nothing as we use coords.txt now
    // But detection2.c seems to rely on CELLPOS for fallback? 
    // Actually we can update this to write a dummy or just ignore.
}

static GPtrArray *load_cell_positions(const char *root_dir)
{
    if (!root_dir) return NULL;
    char *txt_path = g_build_filename(root_dir, "cells", "coords.txt", NULL);
    
    // Try reading coords.txt first
    if (g_file_test(txt_path, G_FILE_TEST_EXISTS)) {
        gchar *data = NULL;
        if (g_file_get_contents(txt_path, &data, NULL, NULL)) {
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
             g_free(txt_path);
             g_print("[Info] Loaded EXACT coords from coords.txt\n");
             return arr;
        }
    }
    g_free(txt_path);

    // Fallback to CELLPOS
    char *pos_path = g_build_filename(root_dir, "CELLPOS", NULL);
    gchar *data = NULL;
    if (!g_file_get_contents(pos_path, &data, NULL, NULL)) {
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
    if (!g_last_image_path) {
        g_printerr("[Warn] Aucune image de detection disponible pour l'overlay.\n");
        return;
    }
    GError *err = NULL;
    GdkPixbuf *img = gdk_pixbuf_new_from_file(g_last_image_path, &err);
    if (!img) {
        g_printerr("[Warn] Impossible de charger l'image pour l'overlay (%s): %s\n", g_last_image_path, err->message);
        g_clear_error(&err);
        return;
    }
    // Convert to RGB (remove alpha) to ensure drawing works simply if needed, 
    // but GdkPixbuf usually handles alpha fine. 
    // We want to draw on top.
    GdkPixbuf *disp = gdk_pixbuf_copy(img);

    char *root_dir = find_project_root();
    GPtrArray *cell_positions = load_cell_positions(root_dir);

    int x0=0, y0=0, x1=0, y1=0;
    if (g_grid_bbox_set) {
        x0 = g_grid_x0; y0 = g_grid_y0; x1 = g_grid_x1; y1 = g_grid_y1;
    } else if (!detect_grid_bbox(img, &x0, &y0, &x1, &y1)) {
        x0 = 0; y0 = 0; x1 = gdk_pixbuf_get_width(img) - 1; y1 = gdk_pixbuf_get_height(img) - 1;
    }
    int W = x1 - x0 + 1;
    int H = y1 - y0 + 1;
    double stepX = (nb_cols > 0) ? (double)W / (double)nb_cols : 48.0;
    double stepY = (nb_rows > 0) ? (double)H / (double)nb_rows : 48.0;

    for (guint i = 0; i < results->len; i++) {
        SolveResult *sr = g_ptr_array_index((GPtrArray *)results, i);
        if (!sr->found) continue;

        WordColor col = word_color_for_index((int)i);

        // Determine start and end centers
        int x1, y1, x2, y2;
        int w1 = 0, h1 = 0, w2 = 0, h2 = 0;

        // Start cell
        CellBBox *cb_start = cell_positions ? lookup_cell_bbox(cell_positions, sr->c1 + 1, sr->r1 + 1) : NULL;
        if (cb_start) {
            x1 = (cb_start->x0 + cb_start->x1) / 2;
            y1 = (cb_start->y0 + cb_start->y1) / 2;
            w1 = cb_start->x1 - cb_start->x0;
            h1 = cb_start->y1 - cb_start->y0;
        } else {
            x1 = x0 + (int)(sr->c1 * stepX + stepX/2);
            y1 = y0 + (int)(sr->r1 * stepY + stepY/2);
            w1 = (int)stepX;
            h1 = (int)stepY;
        }

        // End cell
        CellBBox *cb_end = cell_positions ? lookup_cell_bbox(cell_positions, sr->c2 + 1, sr->r2 + 1) : NULL;
        if (cb_end) {
            x2 = (cb_end->x0 + cb_end->x1) / 2;
            y2 = (cb_end->y0 + cb_end->y1) / 2;
            w2 = cb_end->x1 - cb_end->x0;
            h2 = cb_end->y1 - cb_end->y0;
        } else {
            x2 = x0 + (int)(sr->c2 * stepX + stepX/2);
            y2 = y0 + (int)(sr->r2 * stepY + stepY/2);
            w2 = (int)stepX;
            h2 = (int)stepY;
        }
        
        // Average cell size (diameter)
        double cell_size = (double)(w1 + w2 + h1 + h2) / 4.0;
        double radius = cell_size * 0.5;

        // Vector D = P2 - P1
        double dx = (double)(x2 - x1);
        double dy = (double)(y2 - y1);
        double len = sqrt(dx*dx + dy*dy);
        
        // Normalized direction
        double ux = (len > 0.001) ? dx / len : 0.0;
        double uy = (len > 0.001) ? dy / len : 0.0;
        
        // Orthogonal vector (Normal)
        double nx = -uy;
        double ny = ux;

        double pad_len = radius * 0.8;  // Extend a bit more past the center (was 0.5)
        double pad_width = radius * 1.1; // Make it wider (was 0.9)

        // 4 Corners: 
        // Start side
        int ax = (int)(x1 - pad_len * ux + pad_width * nx);
        int ay = (int)(y1 - pad_len * uy + pad_width * ny);
        int bx = (int)(x1 - pad_len * ux - pad_width * nx);
        int by = (int)(y1 - pad_len * uy - pad_width * ny);

        // End side
        int cx = (int)(x2 + pad_len * ux - pad_width * nx);
        int cy = (int)(y2 + pad_len * uy - pad_width * ny);
        int dx_ = (int)(x2 + pad_len * ux + pad_width * nx);
        int dy_ = (int)(y2 + pad_len * uy + pad_width * ny);

        // Draw Frame (4 lines) using capsule for nice rounded caps on the lines
        int thick = 3;
        draw_capsule(disp, ax, ay, bx, by, thick, col.r, col.g, col.b); // Back
        draw_capsule(disp, bx, by, cx, cy, thick, col.r, col.g, col.b); // Side 1
        draw_capsule(disp, cx, cy, dx_, dy_, thick, col.r, col.g, col.b); // Front
        draw_capsule(disp, dx_, dy_, ax, ay, thick, col.r, col.g, col.b); // Side 2
    }

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Solutions (overlay)");
    // Use actual image size
    gtk_window_set_default_size(GTK_WINDOW(win), gdk_pixbuf_get_width(disp), gdk_pixbuf_get_height(disp));
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(win), box);

    GtkWidget *imgw = gtk_image_new_from_pixbuf(disp);
    gtk_box_pack_start(GTK_BOX(box), imgw, TRUE, TRUE, 0);

    GtkWidget *btn_close = gtk_button_new_with_label("Fermer");
    gtk_box_pack_start(GTK_BOX(box), btn_close, FALSE, FALSE, 4);
    g_signal_connect(btn_close, "clicked", G_CALLBACK(on_overlay_close), NULL);

    gtk_widget_show_all(win);

    g_object_unref(img);
    g_object_unref(disp);

    if (cell_positions) g_ptr_array_free(cell_positions, TRUE);
    g_free(root_dir);
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

    double maxc = 0.0, maxr = 0.0;
    for (int x = 0; x < W; x++) if (col[x] > maxc) maxc = col[x];
    for (int y = 0; y < H; y++) if (row[y] > maxr) maxr = row[y];

    double tc = maxc * 0.35;
    double tr = maxr * 0.35;

    int lx = 0, rx = W - 1, ty = 0, by = H - 1;
    while (lx < W && col[lx] < tc) lx++;
    while (rx >= 0 && col[rx] < tc) rx--;
    while (ty < H && row[ty] < tr) ty++;
    while (by >= 0 && row[by] < tr) by--;

    g_free(col);
    g_free(row);

    if (lx >= rx || ty >= by) return 0;
    *x0 = lx; *x1 = rx; *y0 = ty; *y1 = by;
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

static void find_zones(GdkPixbuf *pix,
                       int *gx0,int *gx1,int *gy0,int *gy1,
                       int *wx0,int *wx1,int *wy0,int *wy1)
{
    const guint8 thr = 180;
    int W = gdk_pixbuf_get_width(pix);
    int H = gdk_pixbuf_get_height(pix);

    int gx0_local = 0, gx1_local = 0;
    int gy0_local = 0, gy1_local = H - 1;
    int wx0_local = 0, wx1_local = 0;
    int word_idx = -1;
    int grid_idx = -1;

    if (W <= 0 || H <= 0) { goto fallback_zones; }

    double *dens = calloc((size_t)W, sizeof(double));
    if (!dens) { goto fallback_zones; }
    for (int x = 0; x < W; x++)
    {
        int black = 0;
        for (int y = 0; y < H; y++)
            if (get_gray_local(pix, x, y) < thr) black++;
        dens[x] = (double)black / (double)H;
    }

    double *sm = calloc((size_t)W, sizeof(double));
    if (!sm) { free(dens); goto fallback_zones; }

    int rad = W / 80;
    if (rad < 5) rad = 5;
    if (rad > 20) rad = 20;

    for (int x = 0; x < W; x++)
    {
        int L = clampi(x - rad, 0, W-1);
        int R = clampi(x + rad, 0, W-1);
        double acc = 0.0;
        int cnt = 0;
        for (int i = L; i <= R; i++) { acc += dens[i]; cnt++; }
        sm[x] = (cnt > 0) ? acc / (double)cnt : 0.0;
    }

    double mean_s = 0.0;
    for (int x = 0; x < W; x++) mean_s += sm[x];
    mean_s /= (double)W;

    if (mean_s < 1e-4) { free(dens); free(sm); goto fallback_zones; }

    double Tseg = 0.15 * mean_s;
    if (Tseg < 0.005) Tseg = 0.005;

    int min_width = W / 40;
    if (min_width < 6) min_width = 6;

    Segment *seg = malloc(sizeof(Segment) * (size_t)W);
    int nseg = 0;
    int inside = 0;
    int start = 0;
    double sum = 0.0;
    int cnt = 0;

    for (int x = 0; x < W; x++)
    {
        if (sm[x] >= Tseg)
        {
            if (!inside) { inside = 1; start = x; sum = sm[x]; cnt = 1; }
            else { sum += sm[x]; cnt++; }
        }
        else if (inside)
        {
            int end = x - 1;
            int width = end - start + 1;
            if (width >= min_width) { seg[nseg++] = (Segment){ start, end, sum / (double)cnt }; }
            inside = 0;
        }
    }
    if (inside)
    {
        int end = W - 1;
        int width = end - start + 1;
        if (width >= min_width) { seg[nseg++] = (Segment){ start, end, sum / (double)cnt }; }
    }

    if (nseg == 0) { free(dens); free(sm); free(seg); goto fallback_zones; }

    double best_score_size = 0.0;

    for (int i = 0; i < nseg; i++)
    {
        double current_score_size = seg[i].avg * (double)(seg[i].end - seg[i].start + 1);
        if (current_score_size > best_score_size)
        {
            best_score_size = current_score_size;
            grid_idx = i;
        }
    }

    if (grid_idx >= 0)
    {
        gx0_local = seg[grid_idx].start;
        gx1_local = seg[grid_idx].end;

        double *row_dens = calloc((size_t)H, sizeof(double));
        if (row_dens)
        {
            for (int y = 0; y < H; y++)
            {
                int black = 0;
                for (int x = gx0_local; x <= gx1_local; x++)
                    if (get_gray_local(pix, x, y) < thr) black++;
                row_dens[y] = (double)black / (double)(gx1_local - gx0_local + 1);
            }

            double maxr = 0.0;
            for (int y = 0; y < H; y++) if (row_dens[y] > maxr) maxr = row_dens[y];

            double Ty = maxr * 0.35;
            int top = 0, bot = H - 1;
            while (top < H && row_dens[top] < Ty) top++;
            while (bot >= 0 && row_dens[bot] < Ty) bot--;
            if (top < bot) { gy0_local = top; gy1_local = bot; }
            free(row_dens);
        }
    }
    else { goto fallback_zones; }

    double best_word_score = 0.0;

    for (int i = 0; i < nseg; i++)
    {
        if (i == grid_idx) continue;

        int s0 = seg[i].start;
        int s1 = seg[i].end;

        if (!(s1 < gx0_local || s0 > gx1_local))
            continue;

        double score = seg[i].avg * (double)(s1 - s0 + 1);

        if (score > best_word_score)
        {
            best_word_score = score;
            word_idx = i;
        }
    }

    if (word_idx >= 0)
    {
        wx0_local = seg[word_idx].start;
        wx1_local = seg[word_idx].end;

        if (wx0_local > gx1_local)
            gx1_local = wx0_local - 1;
    }

    free(dens); free(sm); free(seg);

    *gx0 = gx0_local;
    *gx1 = gx1_local;
    *gy0 = gy0_local;
    *gy1 = gy1_local;

    *wx0 = clampi(wx0_local, 0, W-1);
    *wx1 = clampi(wx1_local, 0, W-1);
    *wy0 = gy0_local;
    *wy1 = gy1_local;
    return;

fallback_zones:
    *gx0 = W/3; *gx1 = W-1; *gy0 = 0; *gy1 = H-1;
    *wx0 = 0;   *wx1 = W/3; *wy0 = 0; *wy1 = H-1;
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
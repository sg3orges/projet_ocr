#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include "detection.h"
#include "networks.h"
#ifdef MAX
#undef MAX
#endif
#include "solver.h"

#if defined(__GNUC__)
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

static char *g_last_image_path = NULL;
static int g_grid_x0 = 0, g_grid_y0 = 0, g_grid_x1 = 0, g_grid_y1 = 0;
static int g_grid_bbox_set = 0;
static GtkWidget *g_detect_window = NULL;

static void cleanup_generated_files(void) UNUSED;
static void reopen_detect_window(void) UNUSED;

static void cleanup_generated_files(void)
{
    const char *paths[] = { "GRIDL", "GRIDWO", "CELLPOS", "cells", "letterinword", "images", NULL };
    for (int i = 0; paths[i]; i++) {
        char *cmd = g_strdup_printf("rm -rf %s", paths[i]);
        g_spawn_command_line_async(cmd, NULL);
        g_free(cmd);
    }
}

static void reopen_detect_window(void)
{
    if (g_detect_window && GTK_IS_WIDGET(g_detect_window)) {
        gtk_widget_show_all(g_detect_window);
        if (GTK_IS_WINDOW(g_detect_window))
            gtk_window_present(GTK_WINDOW(g_detect_window));
    }
}

static void on_detect_destroy(GtkWidget *widget, gpointer user_data)
{
    (void)user_data;
    if (g_detect_window == widget)
        g_detect_window = NULL;
}

static void on_overlay_close(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    (void)user_data;
    g_print("[Info] Fermeture demandee par l'utilisateur. Arret du processus.\n");
    exit(0);
}

typedef struct
{
    int col, row;
    int x0, y0, x1, y1;
} CellBBox;

void detect_letters_in_grid(GdkPixbuf *img, GdkPixbuf *disp,
                            int gx0, int gx1, int gy0, int gy1,
                            guint8 black_thr, guint8 R, guint8 G, guint8 B);
void detect_letters_in_words(GdkPixbuf *img, GdkPixbuf *disp,
                             int wx0, int wx1, int wy0, int wy1,
                             guint8 black_thr, guint8 R, guint8 G, guint8 B);

static inline int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline guint8 get_gray_local(GdkPixbuf *pix, int x, int y)
{
    int n = gdk_pixbuf_get_n_channels(pix);
    guchar *p = gdk_pixbuf_get_pixels(pix) + y * gdk_pixbuf_get_rowstride(pix) + x * n;
    return (guint8)((p[0] + p[1] + p[2]) / 3);
}

static void draw_rect(GdkPixbuf *pix, int x0, int y0, int x1, int y1,
                      guint8 R, guint8 G, guint8 B)
{
    int W = gdk_pixbuf_get_width(pix), H = gdk_pixbuf_get_height(pix);
    int n = gdk_pixbuf_get_n_channels(pix), rs = gdk_pixbuf_get_rowstride(pix);
    guchar *px = gdk_pixbuf_get_pixels(pix);

    x0 = clampi(x0, 0, W - 1);
    x1 = clampi(x1, 0, W - 1);
    y0 = clampi(y0, 0, H - 1);
    y1 = clampi(y1, 0, H - 1);

    for (int x = x0; x <= x1; x++) {
        guchar *t = px + y0 * rs + x * n;
        guchar *b = px + y1 * rs + x * n;
        t[0] = R; t[1] = G; t[2] = B;
        b[0] = R; b[1] = G; b[2] = B;
    }
    for (int y = y0; y <= y1; y++) {
        guchar *l = px + y * rs + x0 * n;
        guchar *r = px + y * rs + x1 * n;
        l[0] = R; l[1] = G; l[2] = B;
        r[0] = R; r[1] = G; r[2] = B;
    }
}

static double *col_black_ratio_zone(GdkPixbuf *pix, guint8 thr, int x0, int x1) UNUSED;
static double *col_black_ratio_zone(GdkPixbuf *pix, guint8 thr, int x0, int x1)
{
    int W = gdk_pixbuf_get_width(pix);
    int H = gdk_pixbuf_get_height(pix);
    x0 = clampi(x0, 0, W - 1);
    x1 = clampi(x1, 0, W - 1);
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }

    int n = x1 - x0 + 1;
    double *r = calloc((size_t)n, sizeof(double));
    if (!r) return NULL;

    for (int i = 0; i < n; i++) {
        int x = x0 + i;
        int black = 0;
        for (int y = 0; y < H; y++)
            if (get_gray_local(pix, x, y) < thr)
                black++;
        r[i] = (double)black / (double)H;
    }
    return r;
}

static double autocorr_strength(const double *p, int n, int lag_min, int lag_max)
{
    if (n <= 0) return 0.0;
    if (lag_max >= n) lag_max = n - 1;
    if (lag_min >= lag_max) return 0.0;

    double mean = 0.0;
    for (int i = 0; i < n; i++) mean += p[i];
    mean /= (double)n;

    double var = 0.0;
    for (int i = 0; i < n; i++) {
        double d = p[i] - mean;
        var += d * d;
    }
    if (var <= 1e-12) return 0.0;

    double best = 0.0;
    for (int k = lag_min; k <= lag_max; k++) {
        double acc = 0.0;
        int cnt = 0;
        for (int i = 0; i + k < n; i++) {
            double a = p[i] - mean;
            double b = p[i + k] - mean;
            acc += a * b;
            cnt++;
        }
        if (cnt > 0) {
            double score = acc / (var * cnt);
            if (score > best) best = score;
        }
    }
    if (best < 0.0) best = 0.0;
    return best;
}

static double periodicity_score(const double *p, int n) UNUSED;
static double periodicity_score(const double *p, int n)
{
    if (n < 8) return 0.0;
    int lag_min = 3;
    int lag_max = n / 4;
    if (lag_max <= lag_min) return 0.0;
    return autocorr_strength(p, n, lag_min, lag_max);
}

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
    if (res >= 'a' && res <= 'z') res = (char)(res - 32);
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
            grid[r * max_col + c] = cp->letter;
    }

    GString *out = g_string_new("");
    for (int r = 0; r < max_row; r++) {
        g_string_append_len(out, grid + r * max_col, (gssize)max_col);
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
        g_printerr("[Warn] Impossible de charger l'image pour l'overlay (%s): %s\n",
                   g_last_image_path, err->message);
        g_clear_error(&err);
        return;
    }

    GdkPixbuf *disp = gdk_pixbuf_copy(img);

    char *root_dir = find_project_root();
    GPtrArray *cell_positions = load_cell_positions(root_dir);

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (g_grid_bbox_set) {
        x0 = g_grid_x0; y0 = g_grid_y0; x1 = g_grid_x1; y1 = g_grid_y1;
    } else if (!detect_grid_bbox(img, &x0, &y0, &x1, &y1)) {
        x0 = 0; y0 = 0;
        x1 = gdk_pixbuf_get_width(img) - 1;
        y1 = gdk_pixbuf_get_height(img) - 1;
    }

    int W = x1 - x0 + 1;
    int H = y1 - y0 + 1;
    double stepX = (nb_cols > 0) ? (double)W / (double)nb_cols : 48.0;
    double stepY = (nb_rows > 0) ? (double)H / (double)nb_rows : 48.0;
    double insetX = stepX * 0.20;
    double insetY = stepY * 0.20;

    for (guint i = 0; i < results->len; i++) {
        SolveResult *sr = g_ptr_array_index((GPtrArray *)results, i);
        if (!sr->found) continue;

        int len = sr->word ? (int)strlen(sr->word) : 0;
        int dc = (sr->c2 > sr->c1) ? 1 : (sr->c2 < sr->c1 ? -1 : 0);
        int dr = (sr->r2 > sr->r1) ? 1 : (sr->r2 < sr->r1 ? -1 : 0);
        if (len <= 0)
            len = abs(sr->c2 - sr->c1) + abs(sr->r2 - sr->r1) + 1;

        WordColor col = word_color_for_index((int)i);

        for (int k = 0; k < len; k++) {
            int c = sr->c1 + dc * k;
            int r = sr->r1 + dr * k;
            int rx0, ry0, rx1, ry1;

            CellBBox *cb = cell_positions ? lookup_cell_bbox(cell_positions, c, r) : NULL;
            if (cb) {
                int w = cb->x1 - cb->x0 + 1;
                int h = cb->y1 - cb->y0 + 1;
                int inset_px_x = (int)llround(w * 0.08);
                int inset_px_y = (int)llround(h * 0.08);
                rx0 = clampi(cb->x0 + inset_px_x, 0, gdk_pixbuf_get_width(disp) - 1);
                ry0 = clampi(cb->y0 + inset_px_y, 0, gdk_pixbuf_get_height(disp) - 1);
                rx1 = clampi(cb->x1 - inset_px_x, 0, gdk_pixbuf_get_width(disp) - 1);
                ry1 = clampi(cb->y1 - inset_px_y, 0, gdk_pixbuf_get_height(disp) - 1);
            } else {
                rx0 = x0 + (int)llround(c * stepX + insetX);
                ry0 = y0 + (int)llround(r * stepY + insetY);
                rx1 = x0 + (int)llround((c + 1) * stepX - insetX) - 1;
                ry1 = y0 + (int)llround((r + 1) * stepY - insetY) - 1;
            }

            draw_rect(disp, rx0, ry0, rx1, ry1, col.r, col.g, col.b);
        }
    }

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Solutions (overlay)");
    gtk_window_set_default_size(GTK_WINDOW(win),
                                gdk_pixbuf_get_width(disp),
                                gdk_pixbuf_get_height(disp));

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

static int solve_words_in_grid(const char *root_dir, GPtrArray **out_results, int *out_rows, int *out_cols)
{
    if (out_results) *out_results = NULL;
    if (out_rows) *out_rows = 0;
    if (out_cols) *out_cols = 0;
    if (!root_dir) return 0;

    char *grid_path = g_build_filename(root_dir, "GRIDL", NULL);
    char *words_path = g_build_filename(root_dir, "GRIDWO", NULL);

    char matrice[MAX][MAX];
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

        int li1 = -1, li2 = -1, co1 = -1, co2 = -1;
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
            GError *err2 = NULL;

            if (g_file_set_contents(gridwo_path, out_words->str, -1, &err2))
                g_print("[Info] Fichier GRIDWO genere -> %s\n", gridwo_path);
            else {
                g_printerr("[Error] Ecriture GRIDWO echouee (%s): %s\n", gridwo_path, err2->message);
                g_clear_error(&err2);
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
                       int *gx0, int *gx1, int *gy0, int *gy1,
                       int *wx0, int *wx1, int *wy0, int *wy1)
{
    const guint8 thr = 180;
    int W = gdk_pixbuf_get_width(pix);
    int H = gdk_pixbuf_get_height(pix);

    int gx0_local = 0, gx1_local = 0;
    int gy0_local = 0, gy1_local = H - 1;
    int wx0_local = 0, wx1_local = 0;
    int word_idx = -1;
    int grid_idx = -1;

    if (W <= 0 || H <= 0) goto fallback_zones;

    double *dens = calloc((size_t)W, sizeof(double));
    if (!dens) goto fallback_zones;

    for (int x = 0; x < W; x++) {
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

    for (int x = 0; x < W; x++) {
        int L = clampi(x - rad, 0, W - 1);
        int R = clampi(x + rad, 0, W - 1);
        double acc = 0.0;
        int cnt = 0;
        for (int i = L; i <= R; i++) { acc += dens[i]; cnt++; }
        sm[x] = cnt > 0 ? acc / (double)cnt : 0.0;
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

    for (int x = 0; x < W; x++) {
        if (sm[x] >= Tseg) {
            if (!inside) { inside = 1; start = x; sum = sm[x]; cnt = 1; }
            else { sum += sm[x]; cnt++; }
        } else if (inside) {
            int end = x - 1;
            int width = end - start + 1;
            if (width >= min_width) seg[nseg++] = (Segment){ start, end, sum / (double)cnt };
            inside = 0;
        }
    }
    if (inside) {
        int end = W - 1;
        int width = end - start + 1;
        if (width >= min_width) seg[nseg++] = (Segment){ start, end, sum / (double)cnt };
    }

    if (nseg == 0) { free(dens); free(sm); free(seg); goto fallback_zones; }

    double best_score_size = 0.0;
    for (int i = 0; i < nseg; i++) {
        double score = seg[i].avg * (double)(seg[i].end - seg[i].start + 1);
        if (score > best_score_size) { best_score_size = score; grid_idx = i; }
    }

    if (grid_idx >= 0) {
        gx0_local = seg[grid_idx].start;
        gx1_local = seg[grid_idx].end;

        double *row_dens = calloc((size_t)H, sizeof(double));
        if (row_dens) {
            for (int y = 0; y < H; y++) {
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
    } else {
        free(dens); free(sm); free(seg);
        goto fallback_zones;
    }

    double best_word_score = 0.0;
    for (int i = 0; i < nseg; i++) {
        if (i == grid_idx) continue;

        int s0 = seg[i].start;
        int s1 = seg[i].end;

        if (!(s1 < gx0_local || s0 > gx1_local)) continue;

        double score = seg[i].avg * (double)(s1 - s0 + 1);
        if (score > best_word_score) { best_word_score = score; word_idx = i; }
    }

    if (word_idx >= 0) {
        wx0_local = seg[word_idx].start;
        wx1_local = seg[word_idx].end;
        if (wx0_local > gx1_local) gx1_local = wx0_local - 1;
    }

    free(dens);
    free(sm);
    free(seg);

    *gx0 = gx0_local;
    *gx1 = gx1_local;
    *gy0 = gy0_local;
    *gy1 = gy1_local;

    *wx0 = clampi(wx0_local, 0, W - 1);
    *wx1 = clampi(wx1_local, 0, W - 1);
    *wy0 = gy0_local;
    *wy1 = gy1_local;
    return;

fallback_zones:
    *gx0 = W / 3; *gx1 = W - 1; *gy0 = 0; *gy1 = H - 1;
    *wx0 = 0; *wx1 = W / 3; *wy0 = 0; *wy1 = H - 1;
}

static void on_solve_clicked(GtkWidget *widget, gpointer user_data)
{
    GtkWidget *win = GTK_WIDGET(user_data);
    (void)widget;
    g_print("[Info] Bouton Resoudre clique : generation de GRIDL via reseau de neurones...\n");
    run_solver_pipeline();
    if (win) gtk_widget_hide(win);
}

static void run_detection(GtkWidget *win, const char *path)
{
    g_detect_window = win;
    g_signal_connect(win, "destroy", G_CALLBACK(on_detect_destroy), NULL);

    g_free(g_last_image_path);
    g_last_image_path = g_strdup(path);

    GError *err = NULL;
    GdkPixbuf *img = gdk_pixbuf_new_from_file(path, &err);
    if (!img) {
        g_printerr("Erreur: %s\n", err->message);
        g_clear_error(&err);
        return;
    }

    GdkPixbuf *disp = gdk_pixbuf_copy(img);

    int gx0, gx1, gy0, gy1, wx0, wx1, wy0, wy1;
    find_zones(img, &gx0, &gx1, &gy0, &gy1, &wx0, &wx1, &wy0, &wy1);

    g_grid_x0 = gx0; g_grid_y0 = gy0; g_grid_x1 = gx1; g_grid_y1 = gy1;
    g_grid_bbox_set = 1;

    printf("ZONE GRILLE: x=[%d,%d], y=[%d,%d]\n", gx0, gx1, gy0, gy1);
    printf("ZONE MOTS  : x=[%d,%d], y=[%d,%d]\n", wx0, wx1, wy0, wy1);

    draw_rect(disp, gx0, gy0, gx1, gy1, 255, 0, 0);
    draw_rect(disp, wx0, wy0, wx1, wy1, 0, 255, 0);

    const guint8 BLACK_T = 160;
    detect_letters_in_grid(img, disp, gx0, gx1, gy0, gy1, BLACK_T, 0, 128, 255);
    detect_letters_in_words(img, disp, wx0, wx1, wy0, wy1, BLACK_T, 0, 128, 255);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(win), box);

    GtkWidget *imgw = gtk_image_new_from_pixbuf(disp);
    gtk_box_pack_start(GTK_BOX(box), imgw, TRUE, TRUE, 0);

    GtkWidget *btn_solve = gtk_button_new_with_label("Resoudre");
    g_signal_connect(btn_solve, "clicked", G_CALLBACK(on_solve_clicked), win);
    gtk_box_pack_start(GTK_BOX(box), btn_solve, FALSE, FALSE, 4);

    gtk_widget_show_all(win);

    g_object_unref(img);
    g_object_unref(disp);
}

static void on_open(GApplication *app, GFile **files, int n, const char *hint)
{
    (void)n;
    (void)hint;

    GtkWidget *win = gtk_application_window_new(GTK_APPLICATION(app));
    gtk_window_set_title(GTK_WINDOW(win), "Détection grille et mots (GTK3)");
    gtk_window_set_default_size(GTK_WINDOW(win), 1100, 800);

    char *path = g_file_get_path(files[0]);
    run_detection(win, path);
    g_free(path);
}

int detection_run_app(int argc, char **argv)
{
    GtkApplication *app = gtk_application_new("com.detect.auto", G_APPLICATION_HANDLES_OPEN);
    g_signal_connect(app, "open", G_CALLBACK(on_open), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}

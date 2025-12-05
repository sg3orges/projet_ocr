#include <gtk/gtk.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#define LETTER_TARGET_W 48
#define LETTER_TARGET_H 48

// =======================================================
// FONCTIONS UTILITAIRES LOCALES (STATIC)
// =======================================================

static inline int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline guint8 get_gray(GdkPixbuf *pix, int x, int y)
{
    int W = gdk_pixbuf_get_width(pix);
    int H = gdk_pixbuf_get_height(pix);
    x = clampi(x, 0, W - 1);
    y = clampi(y, 0, H - 1);

    int n = gdk_pixbuf_get_n_channels(pix), rs = gdk_pixbuf_get_rowstride(pix);
    guchar *p = gdk_pixbuf_get_pixels(pix) + y * rs + x * n;
    return (p[0] + p[1] + p[2]) / 3;
}

static void put_rgb(GdkPixbuf *pix, int x, int y, guint8 R, guint8 G, guint8 B)
{
    int W = gdk_pixbuf_get_width(pix), H = gdk_pixbuf_get_height(pix);
    if (x < 0 || y < 0 || x >= W || y >= H)
        return;
    int n = gdk_pixbuf_get_n_channels(pix), rs = gdk_pixbuf_get_rowstride(pix);
    guchar *p = gdk_pixbuf_get_pixels(pix) + y * rs + x * n;
    p[0] = R;
    p[1] = G;
    p[2] = B;
}

static void draw_rect_thick(GdkPixbuf *pix, int x0, int y0, int x1, int y1,
                            guint8 R, guint8 G, guint8 B, int thick)
{
    for (int t_offset = 0; t_offset < thick; t_offset++)
    {
        int cx0 = clampi(x0 + t_offset, 0, gdk_pixbuf_get_width(pix) - 1);
        int cx1 = clampi(x1 - t_offset, 0, gdk_pixbuf_get_width(pix) - 1);
        int cy0 = clampi(y0 + t_offset, 0, gdk_pixbuf_get_height(pix) - 1);
        int cy1 = clampi(y1 - t_offset, 0, gdk_pixbuf_get_height(pix) - 1);

        for (int x = cx0; x <= cx1; x++)
        {
            put_rgb(pix, x, cy0, R, G, B);
            put_rgb(pix, x, cy1, R, G, B);
        }
        for (int y = cy0; y <= cy1; y++)
        {
            put_rgb(pix, cx0, y, R, G, B);
            put_rgb(pix, cx1, y, R, G, B);
        }
    }
}

static int ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0)
        return S_ISDIR(st.st_mode) ? 0 : -1;
    return mkdir(path, 0755);
}

static inline gboolean is_black_pixel(GdkPixbuf *img, int x, int y, guint8 black_thr)
{
    int W = gdk_pixbuf_get_width(img);
    int H = gdk_pixbuf_get_height(img);
    if (x < 0 || x >= W || y < 0 || y >= H)
        return FALSE;
    return get_gray(img, x, y) < black_thr;
}

typedef struct
{
    int x, y;
} Point;

static void flood_fill_component(GdkPixbuf *img, guint8 black_thr, int start_x, int start_y,
                                 int *min_x, int *max_x, int *min_y, int *max_y,
                                 gboolean **visited, int img_width, int img_height)
{
    Point *stack = g_malloc(img_width * img_height * sizeof(Point));
    int stack_idx = 0;

    stack[stack_idx++] = (Point){start_x, start_y};
    visited[start_y][start_x] = TRUE;

    *min_x = start_x;
    *max_x = start_x;
    *min_y = start_y;
    *max_y = start_y;

    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};

    while (stack_idx > 0)
    {
        Point current = stack[--stack_idx];

        if (current.x < *min_x) *min_x = current.x;
        if (current.x > *max_x) *max_x = current.x;
        if (current.y < *min_y) *min_y = current.y;
        if (current.y > *max_y) *max_y = current.y;

        for (int i = 0; i < 4; i++)
        {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];

            if (nx >= 0 && nx < img_width && ny >= 0 && ny < img_height &&
                !visited[ny][nx] && is_black_pixel(img, nx, ny, black_thr))
            {
                if (stack_idx < img_width * img_height)
                {
                    visited[ny][nx] = TRUE;
                    stack[stack_idx++] = (Point){nx, ny};
                }
            }
        }
    }
    g_free(stack);
}

// =======================================================
// Sauvegarde d'une lettre avec marge (et indices de case)
// =======================================================
static int save_letter_with_margin(GdkPixbuf *img, GdkPixbuf *disp,
                                   int min_x, int min_y,
                                   int max_x, int max_y,
                                   guint8 R, guint8 G, guint8 B,
                                   int col_idx, int row_idx,
                                   int letter_idx,
                                   int margin)
{
    int W = gdk_pixbuf_get_width(img);
    int H = gdk_pixbuf_get_height(img);

    int x0 = clampi(min_x - margin, 0, W - 1);
    int y0 = clampi(min_y - margin, 0, H - 1);
    int x1 = clampi(max_x + margin, 0, W - 1);
    int y1 = clampi(max_y + margin, 0, H - 1);

    int width  = x1 - x0 + 1;
    int height = y1 - y0 + 1;
    if (width <= 0 || height <= 0)
        return letter_idx;

    draw_rect_thick(disp, x0, y0, x1, y1, R, G, B, 1);

    GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(img, x0, y0, width, height);
    if (sub)
    {
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple(
            sub,
            LETTER_TARGET_W,
            LETTER_TARGET_H,
            GDK_INTERP_BILINEAR
        );

        if (scaled)
        {
            char path[512];
            snprintf(path, sizeof(path),
                     "cells/%03d_%03d.png",
                     col_idx, row_idx);

            gdk_pixbuf_save(scaled, path, "png", NULL, NULL);
            g_object_unref(scaled);
            letter_idx++;
        }

        g_object_unref(sub);
    }

    return letter_idx;
}

// =======================================================
// FONCTION PRINCIPALE DE DÉTECTION
// =======================================================

void detect_letters_in_grid(GdkPixbuf *img, GdkPixbuf *disp,
                            int gx0, int gx1, int gy0, int gy1,
                            guint8 black_thr,
                            guint8 R, guint8 G, guint8 B)
{
    (void)black_thr; // on utilise nos propres seuils internes

    int W = gdk_pixbuf_get_width(img);
    int H = gdk_pixbuf_get_height(img);

    // Clamp de la zone "grille"
    gx0 = clampi(gx0, 0, W - 1);
    gx1 = clampi(gx1, 0, W - 1);
    gy0 = clampi(gy0, 0, H - 1);
    gy1 = clampi(gy1, 0, H - 1);

    if (gx0 > gx1) { int t = gx0; gx0 = gx1; gx1 = t; }
    if (gy0 > gy1) { int t = gy0; gy0 = gy1; gy1 = t; }

    if (gx1 - gx0 < 5 || gy1 - gy0 < 5)
        return;

    ensure_dir("cells");

    // --- Constantes de filtrage ---
    const int    MIN_AREA        = 3;
    const int    MIN_WIDTH       = 1;
    const int    MIN_HEIGHT      = 3;
    const int    MIN_HEIGHT_I    = 12;   // hauteur min pour considérer un "I"
    const int    MAX_WIDTH_I     = 7;    // largeur max pour un "I" (plus large qu'avant)
    const double ASPECT_MAX      = 4.0;
    const int    MAX_DIM_FACTOR  = 20;
    const int    MIN_STRONG_AREA = 50;

    // Seuils pour grille / lettres
    const guint8 GRID_BLACK_THR   = 100; // noir bien foncé = grille
    const guint8 LETTER_BLACK_THR = 160; // un peu plus tolérant pour les lettres

    int grid_w = gx1 - gx0 + 1;
    int grid_h = gy1 - gy0 + 1;

    // --------------------------------------------------
    // 1) Copie de l'image de travail
    // --------------------------------------------------
    GdkPixbuf *work = gdk_pixbuf_copy(img);
    if (!work)
        return;

    // --------------------------------------------------
    // 2) Détection et blanchiment des traits de grille
    // --------------------------------------------------
    int      *col_sum = g_malloc0(grid_w * sizeof(int));
    int      *row_sum = g_malloc0(grid_h * sizeof(int));
    gboolean *is_vline = g_malloc0(W * sizeof(gboolean));
    gboolean *is_hline = g_malloc0(H * sizeof(gboolean));

    // Projection verticale
    for (int x = gx0; x <= gx1; x++)
    {
        int idx_x = x - gx0;
        int cnt = 0;
        for (int y = gy0; y <= gy1; y++)
            if (get_gray(img, x, y) < GRID_BLACK_THR)
                cnt++;
        col_sum[idx_x] = cnt;
    }

    // Projection horizontale
    for (int y = gy0; y <= gy1; y++)
    {
        int idx_y = y - gy0;
        int cnt = 0;
        for (int x = gx0; x <= gx1; x++)
            if (get_gray(img, x, y) < GRID_BLACK_THR)
                cnt++;
        row_sum[idx_y] = cnt;
    }

    double COL_DENSITY_THR = 0.7;
    double ROW_DENSITY_THR = 0.7;

    // Colonnes de grille
    for (int x = gx0; x <= gx1; x++)
    {
        int idx_x = x - gx0;
        if (col_sum[idx_x] >= (int)(COL_DENSITY_THR * grid_h))
        {
            is_vline[x] = TRUE;
            for (int y = gy0; y <= gy1; y++)
                put_rgb(work, x, y, 255, 255, 255);
        }
    }

    // Lignes de grille
    for (int y = gy0; y <= gy1; y++)
    {
        int idx_y = y - gy0;
        if (row_sum[idx_y] >= (int)(ROW_DENSITY_THR * grid_w))
        {
            is_hline[y] = TRUE;
            for (int x = gx0; x <= gx1; x++)
                put_rgb(work, x, y, 255, 255, 255);
        }
    }

    g_free(col_sum);
    g_free(row_sum);

    // --------------------------------------------------
    // 3) Flood-fill sur l'image de travail (grille blanchie)
    // --------------------------------------------------
    gboolean **visited = g_malloc(H * sizeof(gboolean *));
    for (int y = 0; y < H; y++)
    {
        visited[y] = g_malloc(W * sizeof(gboolean));
        memset(visited[y], 0, W * sizeof(gboolean));
    }

    int letter_idx = 0;

    for (int y = gy0; y <= gy1; y++)
    {
        for (int x = gx0; x <= gx1; x++)
        {
            if (is_black_pixel(work, x, y, LETTER_BLACK_THR) && !visited[y][x])
            {
                int min_x, max_x, min_y, max_y;

                flood_fill_component(work, LETTER_BLACK_THR, x, y,
                                     &min_x, &max_x, &min_y, &max_y,
                                     visited, W, H);

                int width  = max_x - min_x + 1;
                int height = max_y - min_y + 1;
                int area   = width * height;

                if (area < MIN_AREA)
                    continue;

                // Candidat "I" : haut et relativement fin
                gboolean looks_like_I = (height >= MIN_HEIGHT_I && width <= MAX_WIDTH_I);

                // Aire minimale (sauf pour les I)
                if (area < MIN_STRONG_AREA && !looks_like_I)
                    continue;

                if (!looks_like_I &&
                    (width < MIN_WIDTH || height < MIN_HEIGHT))
                    continue;

                if (width > W / MAX_DIM_FACTOR || height > H / MAX_DIM_FACTOR)
                    continue;

                if (width <= 4 && height <= 4 && !looks_like_I)
                    continue;

                // ---- Vérification de contact direct avec la grille ----
                gboolean touches_grid = FALSE;

                // Colonnes de grille
                for (int xx = min_x; xx <= max_x && !touches_grid; ++xx)
                {
                    if (xx >= 0 && xx < W && is_vline[xx])
                        touches_grid = TRUE;
                }

                // Lignes de grille
                for (int yy = min_y; yy <= max_y && !touches_grid; ++yy)
                {
                    if (yy >= 0 && yy < H && is_hline[yy])
                        touches_grid = TRUE;
                }

                // Un I qui colle à la grille -> c'est un bout de trait, on le jette
                if (looks_like_I && touches_grid)
                    continue;

                // Si ça touche la grille et que c'est petit, on rejette (intersection, bout de trait…)
                if (touches_grid && (width < 10 || height < 10 || area < 100) && !looks_like_I)
                    continue;

                // filtres de finesse extrême (sauf pour I)
                if (!looks_like_I && (width <= 3 || height <= 3))
                    continue;

                double aspect = (double)width / (double)height;
                if (!looks_like_I &&
                    (aspect > ASPECT_MAX || aspect < 1.0 / ASPECT_MAX))
                    continue;

                // ---- Calcul col/row dans la grille ----
                int cx = (min_x + max_x) / 2;
                int cy = (min_y + max_y) / 2;

                int col_idx = 0;
                for (int xx = gx0; xx <= cx; ++xx)
                {
                    if (is_vline[xx])
                        col_idx++;
                }

                int row_idx = 0;
                for (int yy = gy0; yy <= cy; ++yy)
                {
                    if (is_hline[yy])
                        row_idx++;
                }

                int margin = 3;
                letter_idx = save_letter_with_margin(img, disp,
                                                     min_x, min_y, max_x, max_y,
                                                     R, G, B,
                                                     col_idx, row_idx,
                                                     letter_idx, margin);
            }
        }
    }

    // Libération
    for (int y = 0; y < H; y++)
        g_free(visited[y]);
    g_free(visited);

    g_free(is_vline);
    g_free(is_hline);

    g_object_unref(work);
}

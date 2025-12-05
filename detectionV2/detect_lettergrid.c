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
// (Incluant la logique de Flood Fill pour la détection de blobs)
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

        if (current.x < *min_x)
            *min_x = current.x;
        if (current.x > *max_x)
            *max_x = current.x;
        if (current.y < *min_y)
            *min_y = current.y;
        if (current.y > *max_y)
            *max_y = current.y;

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

// Sauvegarde une lettre en élargissant un peu la bbox, en la nommant par coordonnées ligne_colonne
static int save_letter_with_margin(GdkPixbuf *img, GdkPixbuf *disp,
                                   int min_x, int min_y,
                                   int max_x, int max_y,
                                   guint8 R, guint8 G, guint8 B,
                                   int letter_idx,
                                   int margin,
                                   int row_idx, int col_idx,
                                   FILE *coords_file)
{
    int W = gdk_pixbuf_get_width(img);
    int H = gdk_pixbuf_get_height(img);

    // On élargit le rectangle de "margin" pixels tout autour
    int x0 = clampi(min_x - margin, 0, W - 1);
    int y0 = clampi(min_y - margin, 0, H - 1);
    int x1 = clampi(max_x + margin, 0, W - 1);
    int y1 = clampi(max_y + margin, 0, H - 1);

    int width  = x1 - x0 + 1;
    int height = y1 - y0 + 1;
    if (width <= 0 || height <= 0)
        return letter_idx;

    // Dessin du rectangle élargi sur l'image d'affichage
    draw_rect_thick(disp, x0, y0, x1, y1, R, G, B, 1);

    // Crop élargi depuis l'image source
    GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(img, x0, y0, width, height);
    if (sub)
    {
        // Redimensionnement à une taille fixe
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple(
            sub,
            LETTER_TARGET_W,
            LETTER_TARGET_H,
            GDK_INTERP_BILINEAR
        );

        if (scaled)
        {
            char path[512];
            snprintf(path, sizeof(path), "cells/letter_%02d_%02d.png", row_idx, col_idx);
            gdk_pixbuf_save(scaled, path, "png", NULL, NULL);
            if (coords_file) {
                fprintf(coords_file, "%02d %02d %d %d %d %d\n", row_idx, col_idx, x0, y0, x1, y1);
            }
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
    (void)black_thr; // on utilise STRICT_BLACK_THR à la place

    int W = gdk_pixbuf_get_width(img);
    int H = gdk_pixbuf_get_height(img);

    // Clamp de la zone grille
    gx0 = clampi(gx0, 0, W - 1);
    gx1 = clampi(gx1, 0, W - 1);
    gy0 = clampi(gy0, 0, H - 1);
    gy1 = clampi(gy1, 0, H - 1);
    if (gx0 > gx1)
    {
        int t = gx0;
        gx0 = gx1;
        gx1 = t;
    }
    if (gy0 > gy1)
    {
        int t = gy0;
        gy0 = gy1;
        gy1 = t;
    }

    if (gx1 - gx0 < 5 || gy1 - gy0 < 5)
        return;

    ensure_dir("cells");

    // --- Constantes de filtrage ---
    const int MIN_AREA = 3;
    const int MIN_WIDTH = 1;   // permet des lettres très fines
    const int MIN_HEIGHT = 3;
    const int MIN_HEIGHT_I = 10; // hauteur minimum pour "I" très fin
    const double ASPECT_MAX = 4.0;
    const int MAX_DIM_FACTOR = 20;
    const guint8 STRICT_BLACK_THR = 120;

    int grid_w = gx1 - gx0 + 1;
    int grid_h = gy1 - gy0 + 1;

    FILE *coords_file = fopen("cells_coords.txt", "w");
    if (coords_file) {
        fprintf(coords_file, "#row col x0 y0 x1 y1\n");
    }

    // Sauvegarde la bbox de la grille pour un post-traitement ultérieur
    FILE *bbox = fopen("grid_bbox.txt", "w");
    if (bbox) {
        fprintf(bbox, "%d %d %d %d\n", gx0, gy0, gx1, gy1);
        fclose(bbox);
    }

    // --------------------------------------------------
    // 1) Copie de l'image pour travailler dessus
    // --------------------------------------------------
    GdkPixbuf *work = gdk_pixbuf_copy(img);
    if (!work)
        return;

    // --------------------------------------------------
    // 2) Détection des traits de grille par projections
    // --------------------------------------------------
    int *col_sum = g_malloc0(grid_w * sizeof(int));
    int *row_sum = g_malloc0(grid_h * sizeof(int));

    // Pour mémoriser quelles colonnes/lignes sont des traits de grille
    gboolean *is_vline = g_malloc0(W * sizeof(gboolean));
    gboolean *is_hline = g_malloc0(H * sizeof(gboolean));

    // Densité verticale (colonnes)
    for (int x = gx0; x <= gx1; x++)
    {
        int idx_x = x - gx0;
        int cnt = 0;
        for (int y = gy0; y <= gy1; y++)
        {
            if (get_gray(img, x, y) < STRICT_BLACK_THR)
                cnt++;
        }
        col_sum[idx_x] = cnt;
    }

    // Densité horizontale (lignes)
    for (int y = gy0; y <= gy1; y++)
    {
        int idx_y = y - gy0;
        int cnt = 0;
        for (int x = gx0; x <= gx1; x++)
        {
            if (get_gray(img, x, y) < STRICT_BLACK_THR)
                cnt++;
        }
        row_sum[idx_y] = cnt;
    }

    // Seuils : > 70% de noir sur la hauteur/largeur => trait de grille
    double COL_DENSITY_THR = 0.7;
    double ROW_DENSITY_THR = 0.7;

    // On blanchit les traits VERTICAUX dans work
    for (int x = gx0; x <= gx1; x++)
    {
        int idx_x = x - gx0;
        if (col_sum[idx_x] >= (int)(COL_DENSITY_THR * grid_h))
        {
            is_vline[x] = TRUE; // colonne de grille
            for (int y = gy0; y <= gy1; y++)
                put_rgb(work, x, y, 255, 255, 255);
        }
    }

    // On blanchit les traits HORIZONTAUX dans work
    for (int y = gy0; y <= gy1; y++)
    {
        int idx_y = y - gy0;
        if (row_sum[idx_y] >= (int)(ROW_DENSITY_THR * grid_w))
        {
            is_hline[y] = TRUE; // ligne de grille
            for (int x = gx0; x <= gx1; x++)
                put_rgb(work, x, y, 255, 255, 255);
        }
    }

    g_free(col_sum);
    g_free(row_sum);

    // --------------------------------------------------
    // 3) Liste des lignes/colonnes de grille pour déterminer (row,col)
    // --------------------------------------------------
    int *vlines = g_malloc0((grid_w + 2) * sizeof(int));
    int *hlines = g_malloc0((grid_h + 2) * sizeof(int));
    int vcount = 0, hcount = 0;

    for (int x = gx0; x <= gx1; x++)
    {
        if (is_vline[x] && (x == gx0 || !is_vline[x - 1]))
        {
            vlines[vcount++] = x;
            while (x <= gx1 && is_vline[x]) x++;
        }
    }
    for (int y = gy0; y <= gy1; y++)
    {
        if (is_hline[y] && (y == gy0 || !is_hline[y - 1]))
        {
            hlines[hcount++] = y;
            while (y <= gy1 && is_hline[y]) y++;
        }
    }

    // --------------------------------------------------
    // 4) Flood-fill sur l'image "work" SANS les traits
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
            if (is_black_pixel(work, x, y, STRICT_BLACK_THR) && !visited[y][x])
            {
                int min_x, max_x, min_y, max_y;

                flood_fill_component(work, STRICT_BLACK_THR, x, y,
                                     &min_x, &max_x, &min_y, &max_y,
                                     visited, W, H);

                int width  = max_x - min_x + 1;
                int height = max_y - min_y + 1;
                int area   = width * height;

                // 1) Vraiment trop petit (filtre fort sur l'aire)
                const int MIN_STRONG_AREA = 50;  // à ajuster si besoin
                if (area < MIN_STRONG_AREA)
                    continue;

                if (width < MIN_WIDTH || height < MIN_HEIGHT) continue;
                if (width > W / MAX_DIM_FACTOR || height > H / MAX_DIM_FACTOR) continue;

                // Filtre micro-blobs très compacts
                if (width <= 4 && height <= 4)
                    continue;

                // 2) Vérifier si ça touche (ou frôle) une ligne / colonne de grille
                gboolean touches_grid = FALSE;

                // colonnes et voisines
                for (int xx = min_x; xx <= max_x && !touches_grid; ++xx)
                {
                    for (int k = -1; k <= 1 && !touches_grid; ++k)
                    {
                        int cx = xx + k;
                        if (cx >= 0 && cx < W && is_vline[cx])
                            touches_grid = TRUE;
                    }
                }
                // lignes et voisines
                for (int yy = min_y; yy <= max_y && !touches_grid; ++yy)
                {
                    for (int k = -1; k <= 1 && !touches_grid; ++k)
                    {
                        int cy = yy + k;
                        if (cy >= 0 && cy < H && is_hline[cy])
                            touches_grid = TRUE;
                    }
                }

                // Si collé à la grille et que ce n'est pas un gros bloc,
                // on jette directement (bout de trait, intersection, etc.)
                if (touches_grid && (width < 10 || height < 10 || area < 100))
                    continue;

                // 3) Forme de la composante

                // Cas particulier : lettre très haute et très fine (ex: I)
                // -> on l'accepte seulement si elle ne touche pas la grille
                gboolean looks_like_I = (!touches_grid &&
                                         height >= MIN_HEIGHT_I &&
                                         width  <= 4);

                // Si ce n'est PAS un I et que c'est très fin, on jette
                if (!looks_like_I && (width <= 3 || height <= 3))
                    continue;

                // Ratio largeur/hauteur pour le reste
                double aspect = (double)width / (double)height;
                if (!looks_like_I &&
                    (aspect > ASPECT_MAX || aspect < 1.0 / ASPECT_MAX))
                {
                    continue;
                }

                // Lettre validée -> helper qui élargit la bbox et sauvegarde
                // Indices de cellule via le centre du bounding box
                int cx = (min_x + max_x) / 2;
                int cy = (min_y + max_y) / 2;
                // Index de cellule: nombre de lignes/colonnes de grille strictement à gauche/au-dessus moins 1
                int col_idx = -1;
                for (int i = 0; i < vcount; i++) {
                    if (cx > vlines[i]) col_idx++;
                    else break;
                }
                if (col_idx < 0) col_idx = 0;

                int row_idx = -1;
                for (int i = 0; i < hcount; i++) {
                    if (cy > hlines[i]) row_idx++;
                    else break;
                }
                if (row_idx < 0) row_idx = 0;

                int margin = 3; // ajuste si tu veux plus ou moins de bord
                letter_idx = save_letter_with_margin(img, disp,
                                                     min_x, min_y, max_x, max_y,
                                                     R, G, B,
                                                     letter_idx, margin,
                                                     row_idx, col_idx,
                                                     coords_file);
            }
        }
    }

    // Libération
    for (int y = 0; y < H; y++)
        g_free(visited[y]);
    g_free(visited);

    g_free(is_vline);
    g_free(is_hline);
    g_free(vlines);
    g_free(hlines);
    if (coords_file) fclose(coords_file);

    g_object_unref(work);
}

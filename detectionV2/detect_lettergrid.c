#include <gtk/gtk.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#define LETTER_TARGET_W 48
#define LETTER_TARGET_H 48

#define MAX_GRID_COLS 64
#define MAX_GRID_ROWS 64

#if defined(__GNUC__)
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

typedef struct
{
    int min_x, max_x;
    int min_y, max_y;
    int row_idx, col_idx;
    int width, height, area;
    gboolean looks_like_I;
} LetterCand;

typedef struct { int x, y; } Point;



static void binarize_pixbuf(GdkPixbuf *pix, guint8 thr)
{
    int W = gdk_pixbuf_get_width(pix);
    int H = gdk_pixbuf_get_height(pix);
    int rs = gdk_pixbuf_get_rowstride(pix);
    int n  = gdk_pixbuf_get_n_channels(pix);
    guchar *p = gdk_pixbuf_get_pixels(pix);

    for (int y = 0; y < H; ++y)
    {
        guchar *row = p + y * rs;
        for (int x = 0; x < W; ++x)
        {
            guchar *px = row + x * n;
            guint8 g = (guint8)((px[0] + px[1] + px[2]) / 3);
            guint8 v = (g < thr) ? 0 : 255;
            px[0] = px[1] = px[2] = v;
        }
    }
}

static void despeckle_binary_black_islands(GdkPixbuf *pix, int min_black_neighbors)
{
    int W = gdk_pixbuf_get_width(pix);
    int H = gdk_pixbuf_get_height(pix);
    int rs = gdk_pixbuf_get_rowstride(pix);
    int n  = gdk_pixbuf_get_n_channels(pix);
    guchar *src = gdk_pixbuf_get_pixels(pix);

    gsize sz = (gsize)rs * (gsize)H;
    guchar *tmp = (guchar*)g_malloc(sz);
    if (!tmp) return;
    memcpy(tmp, src, (size_t)sz);

    for (int y = 1; y < H - 1; ++y)
    {
        for (int x = 1; x < W - 1; ++x)
        {
            guchar *p = tmp + y * rs + x * n;
            if (p[0] != 0) continue;

            int nb = 0;
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (dx == 0 && dy == 0) continue;
                    guchar *q = tmp + (y + dy) * rs + (x + dx) * n;
                    if (q[0] == 0) nb++;
                }
            }

            if (nb < min_black_neighbors)
            {
                guchar *d = src + y * rs + x * n;
                d[0] = d[1] = d[2] = 255;
            }
        }
    }

    g_free(tmp);
}


static int parse_cell_filename(const char *name, int *col, int *row, int *idx)
{
    int c, r, i;
    if (sscanf(name, "%d_%d_%d.png", &c, &r, &i) == 3)
    {
        *col = c; *row = r; *idx = i;
        return 1;
    }
    return 0;
}

static void cleanup_cells_dir(const char *dirpath, int max_cols, int max_rows)
{
    int best_idx[65][65];
    char best_name[65][65][256];

    for (int c = 0; c < 65; ++c)
        for (int r = 0; r < 65; ++r)
        {
            best_idx[c][r] = -1;
            best_name[c][r][0] = '\0';
        }

    DIR *d = opendir(dirpath);
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d)) != NULL)
    {
        if (de->d_name[0] == '.') continue;

        int col, row, idx;
        if (!parse_cell_filename(de->d_name, &col, &row, &idx))
            continue;

        if (col < 1 || col > max_cols || row < 1 || row > max_rows)
            continue;

        if (idx > best_idx[col][row])
        {
            best_idx[col][row] = idx;
            snprintf(best_name[col][row], sizeof(best_name[col][row]), "%s", de->d_name);
        }
    }
    closedir(d);

    d = opendir(dirpath);
    if (!d) return;

    while ((de = readdir(d)) != NULL)
    {
        if (de->d_name[0] == '.') continue;

        int col, row, idx;
        if (!parse_cell_filename(de->d_name, &col, &row, &idx))
            continue;

        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, de->d_name);

        if (col < 1 || col > max_cols || row < 1 || row > max_rows)
        {
            unlink(fullpath);
            continue;
        }

        if (best_idx[col][row] < 0 ||
            strcmp(de->d_name, best_name[col][row]) != 0)
        {
            unlink(fullpath);
            continue;
        }
    }

    closedir(d);
}

static int parse_name(const char *s, int *col, int *row, int *idx)
{
    int c, r, i;
    if (sscanf(s, "%d_%d_%d.png", &c, &r, &i) == 3)
    {
        *col = c; *row = r; *idx = i;
        return 1;
    }
    return 0;
}

static int copy_file(const char *src, const char *dst)
{
    FILE *fs = fopen(src, "rb");
    if (!fs) return -1;
    FILE *fd = fopen(dst, "wb");
    if (!fd) { fclose(fs); return -1; }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fs)) > 0)
        (void)fwrite(buf, 1, n, fd);

    fclose(fs);
    fclose(fd);
    return 0;
}

static void finalize_cells(const char *tmp_dir, const char *out_dir,
                           int max_cols, int max_rows) UNUSED;
static void finalize_cells(const char *tmp_dir, const char *out_dir,
                           int max_cols, int max_rows)
{
    int best_idx[65][65];
    char best_name[65][65][256];

    for (int c = 0; c < 65; ++c)
        for (int r = 0; r < 65; ++r)
        {
            best_idx[c][r] = -1;
            best_name[c][r][0] = '\0';
        }

    DIR *d = opendir(tmp_dir);
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d)) != NULL)
    {
        if (de->d_name[0] == '.') continue;

        int col, row, idx;
        if (!parse_name(de->d_name, &col, &row, &idx))
            continue;

        if (col < 1 || col > max_cols) continue;
        if (row < 1 || row > max_rows) continue;

        if (idx > best_idx[col][row])
        {
            best_idx[col][row] = idx;
            snprintf(best_name[col][row], sizeof(best_name[col][row]),
                     "%s", de->d_name);
        }
    }
    closedir(d);

    for (int col = 1; col <= max_cols; ++col)
    {
        for (int row = 1; row <= max_rows; ++row)
        {
            if (best_idx[col][row] < 0) continue;

            char src[512], dst[512];
            snprintf(src, sizeof(src), "%s/%s", tmp_dir, best_name[col][row]);
            snprintf(dst, sizeof(dst), "%s/%s", out_dir, best_name[col][row]);

            (void)copy_file(src, dst);
        }
    }
}

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

    int n  = gdk_pixbuf_get_n_channels(pix);
    int rs = gdk_pixbuf_get_rowstride(pix);
    guchar *p = gdk_pixbuf_get_pixels(pix) + y * rs + x * n;

    return (guint8)((p[0] + p[1] + p[2]) / 3);
}

static void put_rgb(GdkPixbuf *pix, int x, int y,
                    guint8 R, guint8 G, guint8 B)
{
    int W = gdk_pixbuf_get_width(pix);
    int H = gdk_pixbuf_get_height(pix);
    if (x < 0 || y < 0 || x >= W || y >= H)
        return;
    int n  = gdk_pixbuf_get_n_channels(pix);
    int rs = gdk_pixbuf_get_rowstride(pix);
    guchar *p = gdk_pixbuf_get_pixels(pix) + y * rs + x * n;
    p[0] = R; p[1] = G; p[2] = B;
}

static void draw_rect_thick(GdkPixbuf *pix, int x0, int y0, int x1, int y1,
                            guint8 R, guint8 G, guint8 B, int thick)
{
    for (int t = 0; t < thick; ++t)
    {
        int cx0 = clampi(x0 + t, 0, gdk_pixbuf_get_width(pix) - 1);
        int cx1 = clampi(x1 - t, 0, gdk_pixbuf_get_width(pix) - 1);
        int cy0 = clampi(y0 + t, 0, gdk_pixbuf_get_height(pix) - 1);
        int cy1 = clampi(y1 - t, 0, gdk_pixbuf_get_height(pix) - 1);

        for (int x = cx0; x <= cx1; ++x)
        {
            put_rgb(pix, x, cy0, R, G, B);
            put_rgb(pix, x, cy1, R, G, B);
        }
        for (int y = cy0; y <= cy1; ++y)
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

static inline gboolean is_black_pixel(GdkPixbuf *img, int x, int y, guint8 thr)
{
    int W = gdk_pixbuf_get_width(img);
    int H = gdk_pixbuf_get_height(img);
    if (x < 0 || x >= W || y < 0 || y >= H)
        return FALSE;
    return get_gray(img, x, y) < thr;
}

static void flood_fill_component(GdkPixbuf *img, guint8 thr, int sx, int sy,
                                 int *min_x, int *max_x,
                                 int *min_y, int *max_y,
                                 gboolean **visited,
                                 int W, int H)
{
    Point *stack = g_malloc((gsize)W * (gsize)H * sizeof(Point));
    int top = 0;

    stack[top++] = (Point){ sx, sy };
    visited[sy][sx] = TRUE;

    *min_x = sx; *max_x = sx;
    *min_y = sy; *max_y = sy;

    static const int dx[4] = { -1, 1, 0, 0 };
    static const int dy[4] = { 0, 0, -1, 1 };

    while (top > 0)
    {
        Point p = stack[--top];

        if (p.x < *min_x) *min_x = p.x;
        if (p.x > *max_x) *max_x = p.x;
        if (p.y < *min_y) *min_y = p.y;
        if (p.y > *max_y) *max_y = p.y;

        for (int k = 0; k < 4; ++k)
        {
            int nx = p.x + dx[k];
            int ny = p.y + dy[k];

            if (nx >= 0 && nx < W && ny >= 0 && ny < H &&
                !visited[ny][nx] && is_black_pixel(img, nx, ny, thr))
            {
                visited[ny][nx] = TRUE;
                stack[top++] = (Point){ nx, ny };
            }
        }
    }

    g_free(stack);
}

static int save_letter_simple(GdkPixbuf *img, GdkPixbuf *disp,
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

    int w = x1 - x0 + 1;
    int h = y1 - y0 + 1;
    if (w <= 0 || h <= 0)
        return letter_idx;

    draw_rect_thick(disp, x0, y0, x1, y1, R, G, B, 1);

    GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(img, x0, y0, w, h);
    if (!sub)
        return letter_idx;

    GdkPixbuf *scaled = gdk_pixbuf_scale_simple(
        sub, LETTER_TARGET_W, LETTER_TARGET_H, GDK_INTERP_BILINEAR);
    
    binarize_pixbuf(scaled, 180);
despeckle_binary_black_islands(scaled, 2);


    if (scaled)
    {
        char path[512];
        snprintf(path, sizeof(path),
                 "cells/%03d_%03d_%04d.png",
                 col_idx, row_idx, letter_idx);

        (void)gdk_pixbuf_save(scaled, path, "png", NULL, NULL);
        g_object_unref(scaled);
        letter_idx++;
    }

    g_object_unref(sub);
    return letter_idx;
}

static int save_letter_normalized(GdkPixbuf *img, GdkPixbuf *disp,
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

    int src_w = x1 - x0 + 1;
    int src_h = y1 - y0 + 1;
    if (src_w <= 0 || src_h <= 0)
        return letter_idx;

    draw_rect_thick(disp, x0, y0, x1, y1, R, G, B, 1);

    GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(img, x0, y0, src_w, src_h);
    if (!sub)
        return letter_idx;

    GdkPixbuf *out = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                    LETTER_TARGET_W, LETTER_TARGET_H);
    if (!out)
    {
        g_object_unref(sub);
        return letter_idx;
    }

    int out_rs = gdk_pixbuf_get_rowstride(out);
    int out_n  = gdk_pixbuf_get_n_channels(out);
    guchar *out_pixels = gdk_pixbuf_get_pixels(out);
    for (int y = 0; y < LETTER_TARGET_H; ++y)
    {
        guchar *row = out_pixels + y * out_rs;
        for (int x = 0; x < LETTER_TARGET_W; ++x)
        {
            row[x * out_n + 0] = 255;
            row[x * out_n + 1] = 255;
            row[x * out_n + 2] = 255;
        }
    }

    double sx = (double)LETTER_TARGET_W  / (double)src_w;
    double sy = (double)LETTER_TARGET_H / (double)src_h;

    gdk_pixbuf_scale(sub, out, 0, 0,
                     LETTER_TARGET_W, LETTER_TARGET_H,
                     0.0, 0.0, sx, sy, GDK_INTERP_BILINEAR);

    int rs  = gdk_pixbuf_get_rowstride(out);
    int nch = gdk_pixbuf_get_n_channels(out);
    guchar *pix = gdk_pixbuf_get_pixels(out);

    const guint8 THR = 180;
    for (int y = 0; y < LETTER_TARGET_H; ++y)
    {
        guchar *row = pix + y * rs;
        for (int x = 0; x < LETTER_TARGET_W; ++x)
        {
            guchar *p = row + x * nch;
            guint8 g = (guint8)((p[0] + p[1] + p[2]) / 3);
            guint8 v = (g < THR) ? 0 : 255;
            p[0] = p[1] = p[2] = v;
        }
    }

    int black = 0;
    for (int y = 0; y < LETTER_TARGET_H; ++y)
    {
        guchar *row = pix + y * rs;
        for (int x = 0; x < LETTER_TARGET_W; ++x)
        {
            guchar *p = row + x * nch;
            if (p[0] == 0) black++;
        }
    }

    double ratio = (double)black / (double)(LETTER_TARGET_W * LETTER_TARGET_H);
    if (ratio < 0.01 || ratio > 0.60)
    {
        g_object_unref(out);
        g_object_unref(sub);
        return letter_idx;
    }

    char path[512];
    snprintf(path, sizeof(path), "cells/%03d_%03d_%04d.png",
             col_idx, row_idx, letter_idx);

    (void)gdk_pixbuf_save(out, path, "png", NULL, NULL);

    g_object_unref(out);
    g_object_unref(sub);

    letter_idx++;
    return letter_idx;
}

static int detect_letters_by_cells(GdkPixbuf *img, GdkPixbuf *disp,
                                   int gx0, int gx1, int gy0, int gy1,
                                   guint8 R, guint8 G, guint8 B,
                                   int *out_nb_cells)
{
    if (out_nb_cells)
        *out_nb_cells = 0;

    int W = gdk_pixbuf_get_width(img);
    int H = gdk_pixbuf_get_height(img);

    const guint8 GRID_THR   = 120;
    const guint8 LETTER_THR = 170;

    int grid_w = gx1 - gx0 + 1;
    int grid_h = gy1 - gy0 + 1;

    int *col_sum  = g_malloc0((gsize)grid_w * sizeof(int));
    int *row_sum  = g_malloc0((gsize)grid_h * sizeof(int));
    gboolean *is_vline = g_malloc0((gsize)W * sizeof(gboolean));
    gboolean *is_hline = g_malloc0((gsize)H * sizeof(gboolean));

    int max_col_sum = 0;
    for (int x = gx0; x <= gx1; ++x)
    {
        int idx = x - gx0;
        int cnt = 0;
        for (int y = gy0; y <= gy1; ++y)
            if (get_gray(img, x, y) < GRID_THR)
                cnt++;
        col_sum[idx] = cnt;
        if (cnt > max_col_sum) max_col_sum = cnt;
    }

    int max_row_sum = 0;
    for (int y = gy0; y <= gy1; ++y)
    {
        int idx = y - gy0;
        int cnt = 0;
        for (int x = gx0; x <= gx1; ++x)
            if (get_gray(img, x, y) < GRID_THR)
                cnt++;
        row_sum[idx] = cnt;
        if (cnt > max_row_sum) max_row_sum = cnt;
    }

    if (max_col_sum == 0 || max_row_sum == 0)
    {
        g_free(col_sum); g_free(row_sum);
        g_free(is_vline); g_free(is_hline);
        return 0;
    }

    int col_thr = (int)(0.6 * (double)max_col_sum);
    int row_thr = (int)(0.6 * (double)max_row_sum);

    for (int x = gx0; x <= gx1; ++x)
    {
        int idx = x - gx0;
        if (col_sum[idx] >= col_thr)
            is_vline[x] = TRUE;
    }
    for (int y = gy0; y <= gy1; ++y)
    {
        int idx = y - gy0;
        if (row_sum[idx] >= row_thr)
            is_hline[y] = TRUE;
    }

    g_free(col_sum);
    g_free(row_sum);

    int max_lines_x = gx1 - gx0 + 4;
    int max_lines_y = gy1 - gy0 + 4;

    int *vx = g_malloc0((gsize)max_lines_x * sizeof(int));
    int *vy = g_malloc0((gsize)max_lines_y * sizeof(int));
    int nv = 0, nh = 0;

    int in_run = 0, run_start = 0;

    for (int x = gx0; x <= gx1; ++x)
    {
        if (is_vline[x] && !in_run)
        {
            in_run = 1;
            run_start = x;
        }
        else if (!is_vline[x] && in_run)
        {
            int run_end = x - 1;
            vx[nv++] = (run_start + run_end) / 2;
            in_run = 0;
        }
    }
    if (in_run)
    {
        int run_end = gx1;
        vx[nv++] = (run_start + run_end) / 2;
    }

    in_run = 0;
    for (int y = gy0; y <= gy1; ++y)
    {
        if (is_hline[y] && !in_run)
        {
            in_run = 1;
            run_start = y;
        }
        else if (!is_hline[y] && in_run)
        {
            int run_end = y - 1;
            vy[nh++] = (run_start + run_end) / 2;
            in_run = 0;
        }
    }
    if (in_run)
    {
        int run_end = gy1;
        vy[nh++] = (run_start + run_end) / 2;
    }

    g_free(is_vline);
    g_free(is_hline);

    if (nv < 2 || nh < 2)
    {
        g_free(vx); g_free(vy);
        return 0;
    }

    int nb_cells = (nv - 1) * (nh - 1);
    if (out_nb_cells)
        *out_nb_cells = nb_cells;

    const int MIN_PIXELS_IN_CELL = 25;
    const int CELL_INNER_MARGIN  = 4;

    int letter_idx = 0;

    for (int r = 0; r < nh - 1; ++r)
    {
        int y_top = vy[r];
        int y_bot = vy[r + 1];

        int cy0 = clampi(y_top + CELL_INNER_MARGIN, 0, H - 1);
        int cy1 = clampi(y_bot - CELL_INNER_MARGIN, 0, H - 1);
        if (cy1 <= cy0) continue;

        for (int c = 0; c < nv - 1; ++c)
        {
            int x_left  = vx[c];
            int x_right = vx[c + 1];

            int cx0 = clampi(x_left + CELL_INNER_MARGIN, 0, W - 1);
            int cx1 = clampi(x_right - CELL_INNER_MARGIN, 0, W - 1);
            if (cx1 <= cx0) continue;

            int min_x = W, max_x = -1;
            int min_y = H, max_y = -1;
            int dark_count = 0;

            for (int y = cy0; y <= cy1; ++y)
            {
                for (int x = cx0; x <= cx1; ++x)
                {
                    if (is_black_pixel(img, x, y, LETTER_THR))
                    {
                        if (x < min_x) min_x = x;
                        if (x > max_x) max_x = x;
                        if (y < min_y) min_y = y;
                        if (y > max_y) max_y = y;
                        dark_count++;
                    }
                }
            }

            if (dark_count < MIN_PIXELS_IN_CELL)
                continue;
            if (min_x > max_x || min_y > max_y)
                continue;

            int col_idx = c + 1;
            int row_idx = r + 1;

            letter_idx = save_letter_simple(img, disp,
                                            min_x, min_y, max_x, max_y,
                                            R, G, B,
                                            col_idx, row_idx,
                                            letter_idx,
                                            3);
        }
    }

    g_free(vx);
    g_free(vy);

    return letter_idx;
}

static int extract_line_centers(const gboolean *line, int a, int b,
                                int *out_centers, int max_out)
{
    int n = 0;
    int in_run = 0, run_start = 0;

    for (int i = a; i <= b; ++i)
    {
        if (line[i] && !in_run)
        {
            in_run = 1;
            run_start = i;
        }
        else if (!line[i] && in_run)
        {
            int run_end = i - 1;
            if (n < max_out)
                out_centers[n++] = (run_start + run_end) / 2;
            in_run = 0;
        }
    }
    if (in_run)
    {
        int run_end = b;
        if (n < max_out)
            out_centers[n++] = (run_start + run_end) / 2;
    }
    return n;
}

static int count_centers_before(const int *centers, int n, int v)
{
    int c = 0;
    for (int i = 0; i < n; ++i)
        if (centers[i] <= v)
            c++;
    return c;
}

static int cmp_cy(const void *a, const void *b)
{
    const LetterCand *A = (const LetterCand*)a;
    const LetterCand *B = (const LetterCand*)b;
    int cyA = (A->min_y + A->max_y) / 2;
    int cyB = (B->min_y + B->max_y) / 2;
    return (cyA > cyB) - (cyA < cyB);
}

static int cmp_cx(const void *a, const void *b)
{
    const LetterCand *A = (const LetterCand*)a;
    const LetterCand *B = (const LetterCand*)b;
    int cxA = (A->min_x + A->max_x) / 2;
    int cxB = (B->min_x + B->max_x) / 2;
    return (cxA > cxB) - (cxA < cxB);
}

static void detect_letters_legacy(GdkPixbuf *img, GdkPixbuf *disp,
                                  int gx0, int gx1, int gy0, int gy1,
                                  guint8 R, guint8 G, guint8 B)
{
    int W = gdk_pixbuf_get_width(img);
    int H = gdk_pixbuf_get_height(img);

    const int    MIN_AREA        = 3;
    const int    MIN_WIDTH       = 1;
    const int    MIN_HEIGHT      = 3;
    const int    MIN_HEIGHT_I    = 15;
    const int    MAX_WIDTH_I     = 7;
    const double ASPECT_MAX      = 4.0;
    const int    MAX_DIM_FACTOR  = 20;
    const int    MIN_STRONG_AREA = 50;

    const int    MAX_AREA_ABS    = 1500;
    const int    MAX_W_ABS       = 80;
    const int    MAX_H_ABS       = 80;

    const guint8 GRID_BLACK_THR   = 100;
    const guint8 LETTER_BLACK_THR = 160;

    int grid_w = gx1 - gx0 + 1;
    int grid_h = gy1 - gy0 + 1;

    GdkPixbuf *work = gdk_pixbuf_copy(img);
    if (!work)
        return;

    int      *col_sum  = g_malloc0((gsize)grid_w * sizeof(int));
    int      *row_sum  = g_malloc0((gsize)grid_h * sizeof(int));
    gboolean *is_vline = g_malloc0((gsize)W * sizeof(gboolean));
    gboolean *is_hline = g_malloc0((gsize)H * sizeof(gboolean));

    for (int x = gx0; x <= gx1; ++x)
    {
        int idx_x = x - gx0;
        int cnt = 0;
        for (int y = gy0; y <= gy1; ++y)
            if (get_gray(img, x, y) < GRID_BLACK_THR)
                cnt++;
        col_sum[idx_x] = cnt;
    }

    for (int y = gy0; y <= gy1; ++y)
    {
        int idx_y = y - gy0;
        int cnt = 0;
        for (int x = gx0; x <= gx1; ++x)
            if (get_gray(img, x, y) < GRID_BLACK_THR)
                cnt++;
        row_sum[idx_y] = cnt;
    }

    double COL_DENSITY_THR = 0.7;
    double ROW_DENSITY_THR = 0.7;

    for (int x = gx0; x <= gx1; ++x)
    {
        int idx_x = x - gx0;
        if (col_sum[idx_x] >= (int)(COL_DENSITY_THR * (double)grid_h))
        {
            is_vline[x] = TRUE;
            for (int y = gy0; y <= gy1; ++y)
                put_rgb(work, x, y, 255, 255, 255);
        }
    }

    for (int y = gy0; y <= gy1; ++y)
    {
        int idx_y = y - gy0;
        if (row_sum[idx_y] >= (int)(ROW_DENSITY_THR * (double)grid_w))
        {
            is_hline[y] = TRUE;
            for (int x = gx0; x <= gx1; ++x)
                put_rgb(work, x, y, 255, 255, 255);
        }
    }

    g_free(col_sum);
    g_free(row_sum);

    int vx[512], vy[512];
    int nv = extract_line_centers(is_vline, gx0, gx1, vx, 512);
    int nh = extract_line_centers(is_hline, gy0, gy1, vy, 512);

    gboolean **visited = g_malloc((gsize)H * sizeof(gboolean *));
    for (int y = 0; y < H; ++y)
    {
        visited[y] = g_malloc((gsize)W * sizeof(gboolean));
        memset(visited[y], 0, (size_t)W * sizeof(gboolean));
    }

    GArray *cands = g_array_new(FALSE, FALSE, sizeof(LetterCand));

    for (int y = gy0; y <= gy1; ++y)
    {
        for (int x = gx0; x <= gx1; ++x)
        {
            if (!is_black_pixel(work, x, y, LETTER_BLACK_THR) || visited[y][x])
                continue;

            int min_x, max_x, min_y, max_y;
            flood_fill_component(work, LETTER_BLACK_THR, x, y,
                                 &min_x, &max_x, &min_y, &max_y,
                                 visited, W, H);

            int width  = max_x - min_x + 1;
            int height = max_y - min_y + 1;
            int area   = width * height;

            if (width <= 6 && height <= 6 && area <= 40) continue;
            if (width >= 30 || height >= 30) continue;

            if (area < MIN_AREA) continue;
            if (area > MAX_AREA_ABS) continue;
            if (width > MAX_W_ABS) continue;
            if (height > MAX_H_ABS) continue;

            gboolean looks_like_I = (height >= MIN_HEIGHT_I && width <= MAX_WIDTH_I);

            if (area < MIN_STRONG_AREA && !looks_like_I) continue;
            if (!looks_like_I && (width < MIN_WIDTH || height < MIN_HEIGHT)) continue;
            if (width > W / MAX_DIM_FACTOR || height > H / MAX_DIM_FACTOR) continue;
            if (width <= 4 && height <= 4 && !looks_like_I) continue;

            double aspect = (double)width / (double)height;
            if (!looks_like_I && (aspect > ASPECT_MAX || aspect < 1.0 / ASPECT_MAX))
                continue;

            gboolean touches_grid = FALSE;
            for (int xx = min_x; xx <= max_x && !touches_grid; ++xx)
                if (xx >= 0 && xx < W && is_vline[xx]) touches_grid = TRUE;

            for (int yy = min_y; yy <= max_y && !touches_grid; ++yy)
                if (yy >= 0 && yy < H && is_hline[yy]) touches_grid = TRUE;

            if (looks_like_I && touches_grid) continue;
            if (touches_grid && (width < 10 || height < 10 || area < 100) && !looks_like_I)
                continue;

            LetterCand lc = {0};
            lc.min_x = min_x; lc.max_x = max_x;
            lc.min_y = min_y; lc.max_y = max_y;
            lc.width = width; lc.height = height; lc.area = area;
            lc.looks_like_I = looks_like_I;
            lc.row_idx = 0;
            lc.col_idx = 0;

            int cx = (min_x + max_x) / 2;
            int cy = (min_y + max_y) / 2;

            if (nv >= 2 && nh >= 2)
            {
                lc.col_idx = count_centers_before(vx, nv, cx);
                lc.row_idx = count_centers_before(vy, nh, cy);
            }

            g_array_append_val(cands, lc);
        }
    }

    for (int y = 0; y < H; ++y)
        g_free(visited[y]);
    g_free(visited);

    int letter_idx = 0;

    if (nv < 2 || nh < 2)
    {
        g_array_sort(cands, cmp_cy);

        int n = (int)cands->len;
        int *heights = g_malloc0((gsize)n * sizeof(int));
        for (int i = 0; i < n; ++i)
            heights[i] = g_array_index(cands, LetterCand, i).height;

        long sumh = 0;
        for (int i = 0; i < n; ++i) sumh += heights[i];
        double avg_h = (n > 0) ? (double)sumh / (double)n : 20.0;
        g_free(heights);

        int row = 1;
        int start = 0;

        while (start < n)
        {
            LetterCand first = g_array_index(cands, LetterCand, start);
            int ref_cy = (first.min_y + first.max_y) / 2;

            int tol = (int)(0.6 * avg_h);
            if (tol < 8) tol = 8;

            int end = start + 1;
            while (end < n)
            {
                LetterCand cur = g_array_index(cands, LetterCand, end);
                int cy = (cur.min_y + cur.max_y) / 2;
                if (abs(cy - ref_cy) <= tol)
                    end++;
                else
                    break;
            }

            int len = end - start;
            LetterCand *linebuf = g_malloc((gsize)len * sizeof(LetterCand));
            for (int i = 0; i < len; ++i)
                linebuf[i] = g_array_index(cands, LetterCand, start + i);

            qsort(linebuf, (size_t)len, sizeof(LetterCand), cmp_cx);

            for (int i = 0; i < len; ++i)
            {
                int col = i + 1;
                letter_idx = save_letter_normalized(img, disp,
                                                    linebuf[i].min_x, linebuf[i].min_y,
                                                    linebuf[i].max_x, linebuf[i].max_y,
                                                    R, G, B,
                                                    col, row,
                                                    letter_idx,
                                                    3);
            }

            g_free(linebuf);

            row++;
            start = end;
        }
    }
    else
    {
        for (guint i = 0; i < cands->len; ++i)
        {
            LetterCand lc = g_array_index(cands, LetterCand, i);

            if (lc.col_idx <= 0 || lc.row_idx <= 0)
                continue;

            letter_idx = save_letter_normalized(img, disp,
                                                lc.min_x, lc.min_y,
                                                lc.max_x, lc.max_y,
                                                R, G, B,
                                                lc.col_idx, lc.row_idx,
                                                letter_idx,
                                                3);
        }
    }

    g_array_free(cands, TRUE);
    g_free(is_vline);
    g_free(is_hline);
    g_object_unref(work);
}

void detect_letters_in_grid(GdkPixbuf *img, GdkPixbuf *disp,
                            int gx0, int gx1, int gy0, int gy1,
                            guint8 black_thr,
                            guint8 R, guint8 G, guint8 B)
{
    (void)black_thr;

    int W = gdk_pixbuf_get_width(img);
    int H = gdk_pixbuf_get_height(img);

    gx0 = clampi(gx0, 0, W - 1);
    gx1 = clampi(gx1, 0, W - 1);
    gy0 = clampi(gy0, 0, H - 1);
    gy1 = clampi(gy1, 0, H - 1);

    if (gx0 > gx1) { int t = gx0; gx0 = gx1; gx1 = t; }
    if (gy0 > gy1) { int t = gy0; gy0 = gy1; gy1 = t; }

    if (gx1 - gx0 < 5 || gy1 - gy0 < 5)
        return;

    (void)ensure_dir("cells");

    int nb_cells   = 0;
    int nb_letters = detect_letters_by_cells(img, disp,
                                             gx0, gx1, gy0, gy1,
                                             R, G, B,
                                             &nb_cells);

    if (nb_letters > 0 && nb_cells > 0)
    {
        double ratio = (double)nb_letters / (double)nb_cells;
        if (ratio > 0.5 && ratio < 1.5)
            return;
    }

    gdk_pixbuf_copy_area(img, 0, 0, W, H, disp, 0, 0);

    detect_letters_legacy(img, disp,
                          gx0, gx1, gy0, gy1,
                          R, G, B);

    cleanup_cells_dir("cells", 14, 14);
}

#include <gtk/gtk.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h> 

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
    if(x < 0 || y < 0 || x >= W || y >= H) return;
    int n = gdk_pixbuf_get_n_channels(pix), rs = gdk_pixbuf_get_rowstride(pix);
    guchar *p = gdk_pixbuf_get_pixels(pix) + y * rs + x * n;
    p[0] = R; p[1] = G; p[2] = B;
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
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode) ? 0 : -1;
    return mkdir(path, 0755);
}

static inline gboolean is_black_pixel(GdkPixbuf *img, int x, int y, guint8 black_thr) {
    int W = gdk_pixbuf_get_width(img);
    int H = gdk_pixbuf_get_height(img);
    if (x < 0 || x >= W || y < 0 || y >= H) return FALSE;
    return get_gray(img, x, y) < black_thr;
}

typedef struct { int x, y; } Point;

static void flood_fill_component(GdkPixbuf *img, guint8 black_thr, int start_x, int start_y,
                                 int *min_x, int *max_x, int *min_y, int *max_y,
                                 gboolean **visited, int img_width, int img_height)
{
    Point *stack = g_malloc(img_width * img_height * sizeof(Point));
    int stack_idx = 0;

    stack[stack_idx++] = (Point){start_x, start_y};
    visited[start_y][start_x] = TRUE;

    *min_x = start_x; *max_x = start_x;
    *min_y = start_y; *max_y = start_y;

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
                if (stack_idx < img_width * img_height) {
                    visited[ny][nx] = TRUE;
                    stack[stack_idx++] = (Point){nx, ny};
                }
            }
        }
    }
    g_free(stack);
}

// =======================================================
// FONCTION PRINCIPALE DE DÉTECTION
// =======================================================

void detect_letters_in_grid(GdkPixbuf *img, GdkPixbuf *disp,
                            int gx0, int gx1, int gy0, int gy1,
                            guint8 black_thr,
                            guint8 R, guint8 G, guint8 B)
{
    int W = gdk_pixbuf_get_width(img);
    int H = gdk_pixbuf_get_height(img);

    // Clamping et vérification de la zone
    gx0 = clampi(gx0, 0, W - 1); gx1 = clampi(gx1, 0, W - 1);
    gy0 = clampi(gy0, 0, H - 1); gy1 = clampi(gy1, 0, H - 1);
    
    if (gx0 > gx1) { int t = gx0; gx0 = gx1; gx1 = t; }
    if (gy0 > gy1) { int t = gy0; gy0 = gy1; gy1 = t; }

    if (gx1 - gx0 < 5 || gy1 - gy0 < 5) return;

    ensure_dir("cells");

    // --- Définition des filtres de robustesse finals ---
    const int MIN_AREA = 6;        // Aire minimale (pour éliminer les fragments de bruit)
    const int MIN_DIM = 4;          // Dimension minimale (pour éliminer les lignes très fines)
    const int MIN_HEIGHT_I = 10;    // Hauteur minimale pour accepter les 'I' minces
    const double ASPECT_MAX = 3.0;  // Rapport max d'aspect
    const int MAX_DIM_FACTOR = 29;   // 1/8 de la hauteur/largeur max (contre la fusion)
    
    // NOUVEAU SEUIL DE NOIR STRICT (Coupe les ponts de bruit gris et les lignes faibles)
    const guint8 STRICT_BLACK_THR = 120; // 120 est plus restrictif que le standard 160
    
    // Allocation du tableau visited
    gboolean **visited = g_malloc(H * sizeof(gboolean*));
    for (int y = 0; y < H; y++) {
        visited[y] = g_malloc(W * sizeof(gboolean));
        memset(visited[y], 0, W * sizeof(gboolean)); 
    }

    int letter_idx = 0;
    
    // Parcourir chaque pixel de la zone GRILLE
    for (int y = gy0; y <= gy1; y++)
    {
        for (int x = gx0; x <= gx1; x++)
        {
            // Utiliser le seuil strict pour la détection initiale du pixel noir
            if (is_black_pixel(img, x, y, STRICT_BLACK_THR) && !visited[y][x])
            {
                int min_x, max_x, min_y, max_y;
                
                // Utiliser le seuil strict pour le Flood Fill aussi
                flood_fill_component(img, STRICT_BLACK_THR, x, y,
                                     &min_x, &max_x, &min_y, &max_y,
                                     visited, W, H);

                int width = max_x - min_x + 1;
                int height = max_y - min_y + 1;
                int area = width * height; 

                // --- FILTRAGE DES COMPOSANTES CONNEXES ---
                
                // 1. FILTRE D'AIRE MINIMALE (Contre le bruit)
                if (area < MIN_AREA) continue; 
                
                // 2. FILTRE DE DIMENSION MINIMALE
                if (width < MIN_DIM || height < MIN_DIM) continue; 
                
                // 3. FILTRE D'EXCEPTION 'I' (Autorise les petits blobs s'ils sont très hauts)
                if (area < MIN_AREA * 2 && height < MIN_HEIGHT_I) continue;

                // 4. FILTRE DE TAILLE MAXIMALE (Contre les fusions qui dépassent une case)
                if (width > W/MAX_DIM_FACTOR || height > H/MAX_DIM_FACTOR) continue; 
                
                // 5. FILTRE D'ASPECT (Contre les structures allongées)
                double aspect_ratio = (double)width / (double)height;
                if (aspect_ratio > ASPECT_MAX || aspect_ratio < (1.0 / ASPECT_MAX)) continue; 
                

                // Si la composante passe tous les filtres, c'est une lettre
                draw_rect_thick(disp, min_x, min_y, max_x, max_y, R, G, B, 1); 

                // Sauvegarde de la sous-image de la lettre
                GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(img, min_x, min_y, width, height);
                if (sub) {
                    char path[512];
                    snprintf(path, sizeof(path), "cells/letter_%04d.png", letter_idx++);
                    gdk_pixbuf_save(sub, path, "png", NULL, NULL);
                    g_object_unref(sub);
                }
            }
        }
    }

    // Libérer la mémoire
    for (int y = 0; y < H; y++) {
        g_free(visited[y]);
    }
    g_free(visited);
}
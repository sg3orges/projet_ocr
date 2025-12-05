// gui.c
// GTK interface
// Workflow: Load -> Auto Rotate -> Clean (Auto 35k + Smart Frame) -> Save (Force B/W)

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <SDL2/SDL.h>
#include "networks.h"
#include "../detectionV2/detection.h"
#include "../solver/solver.h"

static char selected_image_path[512] = {0};
static GtkWidget *image_widget = NULL;
static GtkWidget *scale_widget = NULL;

// Buffers
static GdkPixbuf *original_pixbuf = NULL;
static GdkPixbuf *current_display_pixbuf = NULL;

static double current_angle = 0.0;
static const char *startup_image_path = NULL;
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --------------------------------------------------
// Helper: Memory cleaning
// --------------------------------------------------
static void free_pixbuf_data(guchar *data, gpointer user_data)
{
    (void)user_data;
    g_free(data);
}

// --------------------------------------------------
// Utilitaires GRID / Réseau de neurones
// --------------------------------------------------
static int cmp_filenames(const void *a, const void *b)
{
    const char *fa = *(const char * const *)a;
    const char *fb = *(const char * const *)b;
    return strcmp(fa, fb);
}

static gboolean is_directory(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

// Parcourt cells/letter_XXXX.png, applique le réseau et écrit GRID
static void generate_grid_from_cells(void)
{
    DIR *dir = opendir("cells");
    if (!dir) {
        printf("[Error] Dossier cells introuvable, impossible de construire GRID.\n");
        return;
    }

    size_t cap = 32, count = 0;
    char **files = malloc(cap * sizeof(char *));
    if (!files) { closedir(dir); return; }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "letter_", 7) == 0 && strstr(ent->d_name, ".png")) {
            if (count >= cap) {
                cap *= 2;
                char **tmp = realloc(files, cap * sizeof(char *));
                if (!tmp) { free(files); closedir(dir); return; }
                files = tmp;
            }
            files[count++] = strdup(ent->d_name);
        }
    }
    closedir(dir);

    if (count == 0) {
        printf("[Info] Aucun fichier letter_XXXX.png trouvé dans cells/.\n");
        free(files);
        return;
    }

    qsort(files, count, sizeof(char *), cmp_filenames);

    // Déduit la taille de la grille via les indices: 0000 => (row=0, col=0), 0001 => (0,1)
    int max_row = -1;
    int max_col = -1;
    int *rows = malloc(count * sizeof(int));
    int *cols = malloc(count * sizeof(int));
    if (!rows || !cols) {
        free(rows); free(cols);
        for (size_t i = 0; i < count; i++) free(files[i]);
        free(files);
        return;
    }

    for (size_t i = 0; i < count; i++) {
        int r = -1, c = -1;
        if (sscanf(files[i], "letter_%d_%d", &r, &c) == 2) {
            rows[i] = r;
            cols[i] = c;
            if (rows[i] > max_row) max_row = rows[i];
            if (cols[i] > max_col) max_col = cols[i];
        } else {
            rows[i] = cols[i] = -1;
        }
    }

    int grid_rows = max_row + 1;
    int grid_cols = max_col + 1;
    if (grid_rows <= 0 || grid_cols <= 0) {
        printf("[Error] Dimensions de grille invalides.\n");
        free(rows); free(cols);
        for (size_t i = 0; i < count; i++) free(files[i]);
        free(files);
        return;
    }

    // Initialise GRID avec des ?
    char *grid_chars = malloc((size_t)grid_rows * (size_t)grid_cols);
    if (!grid_chars) {
        free(rows); free(cols);
        for (size_t i = 0; i < count; i++) free(files[i]);
        free(files);
        return;
    }
    memset(grid_chars, '?', (size_t)grid_rows * (size_t)grid_cols);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("[Error] SDL init failed: %s\n", SDL_GetError());
        free(grid_chars);
        free(rows); free(cols);
        for (size_t i = 0; i < count; i++) free(files[i]);
        free(files);
        return;
    }

    NeuralNetwork net;
    init_network(&net);
    if (!load_network(&net, "neuronne/brain.bin")) {
        printf("[Warn] Aucun brain.bin, entraînement requis. Abandon génération GRID.\n");
        cleanup(&net);
        SDL_Quit();
        free(grid_chars);
        free(rows); free(cols);
        for (size_t i = 0; i < count; i++) free(files[i]);
        free(files);
        return;
    }

    // Création/troncature du fichier GRID à la racine
    FILE *grid = fopen("GRID", "w");
    if (!grid) {
        printf("[Error] Impossible d'ouvrir GRID en écriture.\n");
        cleanup(&net);
        SDL_Quit();
        free(grid_chars);
        free(rows); free(cols);
        for (size_t i = 0; i < count; i++) free(files[i]);
        free(files);
        return;
    }

    // Remplit la grille avec les prédictions aux bonnes coordonnées
    for (size_t i = 0; i < count; i++) {
        int r = rows[i];
        int c = cols[i];
        if (r >= 0 && c >= 0) {
            char path[256];
            snprintf(path, sizeof(path), "cells/%s", files[i]);
            double conf = 0.0;
            char letter = predict(&net, path, &conf);
            if (r < grid_rows && c < grid_cols)
                grid_chars[r * grid_cols + c] = letter;
        }
    }

    // Écriture dans GRID
    
    for (int r = 0; r < grid_rows; r++) {
        for (int c = 0; c < grid_cols; c++)
            fputc(grid_chars[r * grid_cols + c], grid);
        fputc('\n', grid);
    }

    fclose(grid);
    cleanup(&net);
    SDL_Quit();

    free(grid_chars);
    free(rows); free(cols);
    for (size_t i = 0; i < count; i++) free(files[i]);
    free(files);
    printf("[OK] GRID généré à la racine (./GRID) en %dx%d.\n", grid_rows, grid_cols);
}

// Parcourt letterInWord/word_xxx/letter_yyy.png et écrit GRID_Word (1 ligne par mot)
static void generate_words_from_letterInWord(void)
{
    DIR *dir = opendir("letterInWord");
    if (!dir) {
        printf("[Info] Dossier letterInWord introuvable, saut de GRID_Word.\n");
        return;
    }

    size_t wcap = 16, wcount = 0;
    char **words = malloc(wcap * sizeof(char *));
    if (!words) { closedir(dir); return; }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "word_", 5) == 0) {
            char path[256];
            snprintf(path, sizeof(path), "letterInWord/%s", ent->d_name);
            if (is_directory(path)) {
                if (wcount >= wcap) {
                    wcap *= 2;
                    char **tmp = realloc(words, wcap * sizeof(char *));
                    if (!tmp) { free(words); closedir(dir); return; }
                    words = tmp;
                }
                words[wcount++] = strdup(path);
            }
        }
    }
    closedir(dir);

    if (wcount == 0) {
        printf("[Info] Aucun mot trouvé dans letterInWord/.\n");
        free(words);
        return;
    }

    qsort(words, wcount, sizeof(char *), cmp_filenames);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("[Error] SDL init failed: %s\n", SDL_GetError());
        for (size_t i = 0; i < wcount; i++) free(words[i]);
        free(words);
        return;
    }

    NeuralNetwork net;
    init_network(&net);
    if (!load_network(&net, "neuronne/brain.bin")) {
        printf("[Warn] Aucun brain.bin, génération GRID_Word abandonnée.\n");
        cleanup(&net);
        SDL_Quit();
        for (size_t i = 0; i < wcount; i++) free(words[i]);
        free(words);
        return;
    }

    FILE *f = fopen("GRID_Word", "w");
    if (!f) {
        printf("[Error] Impossible d'ouvrir GRID_Word en écriture.\n");
        cleanup(&net);
        SDL_Quit();
        for (size_t i = 0; i < wcount; i++) free(words[i]);
        free(words);
        return;
    }

    for (size_t wi = 0; wi < wcount; wi++) {
        DIR *wdir = opendir(words[wi]);
        if (!wdir) continue;

        size_t lcap = 16, lcount = 0;
        char **letters = malloc(lcap * sizeof(char *));
        if (!letters) { closedir(wdir); continue; }

        struct dirent *lent;
        while ((lent = readdir(wdir)) != NULL) {
            if (strncmp(lent->d_name, "letter_", 7) == 0 && strstr(lent->d_name, ".png")) {
                if (lcount >= lcap) {
                    lcap *= 2;
                    char **tmp = realloc(letters, lcap * sizeof(char *));
                    if (!tmp) { free(letters); closedir(wdir); goto next_word; }
                    letters = tmp;
                }
                letters[lcount++] = strdup(lent->d_name);
            }
        }
        closedir(wdir);

        if (lcount == 0) {
            free(letters);
            goto next_word;
        }

        qsort(letters, lcount, sizeof(char *), cmp_filenames);

        char *line = malloc(lcount + 1);
        if (!line) {
            for (size_t i = 0; i < lcount; i++) free(letters[i]);
            free(letters);
            goto next_word;
        }

        for (size_t li = 0; li < lcount; li++) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", words[wi], letters[li]);
            double conf = 0.0;
            char letter = predict(&net, path, &conf);
            line[li] = letter;
            free(letters[li]);
        }
        line[lcount] = '\0';
        free(letters);

        fprintf(f, "%s\n", line);
        free(line);

    next_word:
        ;
    }

    fclose(f);
    cleanup(&net);
    SDL_Quit();
    for (size_t i = 0; i < wcount; i++) free(words[i]);
    free(words);
    printf("[OK] GRID_Word généré à la racine (./GRID_Word).\n");
}

// --------------------------------------------------
// Rotation Logic
// --------------------------------------------------
static GdkPixbuf *rotate_pixbuf_any_angle(GdkPixbuf *src, double angle_deg)
{
    int src_w = gdk_pixbuf_get_width(src);
    int src_h = gdk_pixbuf_get_height(src);
    int channels = gdk_pixbuf_get_n_channels(src);
    int rowstride = gdk_pixbuf_get_rowstride(src);
    guchar *src_pixels = gdk_pixbuf_get_pixels(src);

    double angle = angle_deg * M_PI / 180.0;
    double cos_t = cos(angle);
    double sin_t = sin(angle);

    int dest_w = src_w;
    int dest_h = src_h;
    int dest_rowstride = dest_w * channels;
    guchar *dest_pixels = g_malloc0(dest_rowstride * dest_h);

    double cx = (src_w - 1) / 2.0;
    double cy = (src_h - 1) / 2.0;

    for (int y = 0; y < dest_h; y++)
    {
        for (int x = 0; x < dest_w; x++)
        {
            double dx = x - cx;
            double dy = y - cy;
            double sx = cos_t * dx + sin_t * dy + cx;
            double sy = -sin_t * dx + cos_t * dy + cy;

            int isx = (int)floor(sx);
            int isy = (int)floor(sy);

            for (int c = 0; c < channels; c++)
            {
                guchar val = 255;
                if (isx >= 0 && isy >= 0 && isx < src_w && isy < src_h)
                    val = src_pixels[isy * rowstride + isx * channels + c];

                dest_pixels[y * dest_rowstride + x * channels + c] = val;
            }
        }
    }

    return gdk_pixbuf_new_from_data(dest_pixels, GDK_COLORSPACE_RGB, channels == 4, 8, dest_w, dest_h, dest_rowstride, free_pixbuf_data, NULL);
}

static void update_image_rotation(double angle)
{
    if (!original_pixbuf) return;
    if (current_display_pixbuf) g_object_unref(current_display_pixbuf);
    current_display_pixbuf = rotate_pixbuf_any_angle(original_pixbuf, angle);
    gtk_image_set_from_pixbuf(GTK_IMAGE(image_widget), current_display_pixbuf);
}

static void on_rotation_changed(GtkRange *range, gpointer user_data)
{
    (void)user_data;
    current_angle = gtk_range_get_value(range);
    update_image_rotation(current_angle);
}

// --------------------------------------------------
// Auto-Rotation
// --------------------------------------------------
static double detect_skew_angle(GdkPixbuf *pixbuf)
{
    int w = gdk_pixbuf_get_width(pixbuf);
    int h = gdk_pixbuf_get_height(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    unsigned char *edges = g_malloc0(w * h);
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            guchar *p = pixels + y * rowstride + x * channels;
            int gray = (p[0] + p[1] + p[2]) / 3;
            guchar *p_down = pixels + (y + 1) * rowstride + x * channels;
            int grad = abs((int)(p_down[0] + p_down[1] + p_down[2]) / 3 - gray);
            if (grad > 50) edges[y * w + x] = 255;
        }
    }

    int num_thetas = 180; 
    double theta_step = 1.0; 
    double start_theta = -90.0;
    int max_rho = (int)sqrt(w * w + h * h);
    int num_rhos = max_rho * 2;
    int *accumulator = g_malloc0(sizeof(int) * num_thetas * num_rhos);
    
    for (int y = 0; y < h; y += 2) {
        for (int x = 0; x < w; x += 2) {
            if (edges[y * w + x] > 0) {
                for (int t = 0; t < num_thetas; t++) {
                    double theta = (start_theta + t * theta_step) * M_PI / 180.0;
                    double rho = x * cos(theta) + y * sin(theta);
                    int rho_idx = (int)rho + max_rho;
                    if (rho_idx >= 0 && rho_idx < num_rhos) {
                        accumulator[t * num_rhos + rho_idx]++;
                    }
                }
            }
        }
    }

    int max_votes = 0;
    int best_theta_idx = 0;
    for (int t = 0; t < num_thetas; t++) {
        for (int r = 0; r < num_rhos; r++) {
            if (accumulator[t * num_rhos + r] > max_votes) {
                max_votes = accumulator[t * num_rhos + r];
                best_theta_idx = t;
            }
        }
    }

    double best_theta = start_theta + best_theta_idx * theta_step;
    double correction = 0;

    if (fabs(best_theta - 90) < 45) correction = 90 - best_theta;
    else if (fabs(best_theta + 90) < 45) correction = -90 - best_theta;
    else correction = -best_theta;

    g_free(edges);
    g_free(accumulator);
    return correction;
}

static void on_auto_rotate_clicked(GtkWidget *widget, gpointer user_data)
{
    (void)widget; (void)user_data;
    if (!original_pixbuf) return;
    double skew = detect_skew_angle(original_pixbuf);
    gtk_range_set_value(GTK_RANGE(scale_widget), skew);
}

static void load_image_from_file(const char *f)
{
    if (!f) return;

    if (original_pixbuf) g_object_unref(original_pixbuf);
    if (current_display_pixbuf) g_object_unref(current_display_pixbuf);

    original_pixbuf = gdk_pixbuf_new_from_file(f, NULL);
    if (original_pixbuf) {
        current_display_pixbuf = gdk_pixbuf_copy(original_pixbuf);
        gtk_image_set_from_pixbuf(GTK_IMAGE(image_widget), current_display_pixbuf);
        g_snprintf(selected_image_path, sizeof(selected_image_path), "%s", f);

        gtk_range_set_value(GTK_RANGE(scale_widget), 0.0);
    }
}

// --------------------------------------------------
// Binarization (Noir & Blanc)
// --------------------------------------------------
static void apply_black_and_white(GdkPixbuf *pixbuf)
{
    int w = gdk_pixbuf_get_width(pixbuf);
    int h = gdk_pixbuf_get_height(pixbuf);
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    double mean = 0.0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *p = pixels + y * rowstride + x * channels;
            double gray = 0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2];
            mean += gray;
        }
    }
    mean /= (w * h);
    double threshold = mean * 0.85; 

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *p = pixels + y * rowstride + x * channels;
            double gray = 0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2];
            guchar val = (gray > threshold) ? 255 : 0;
            p[0] = p[1] = p[2] = val;
            if (channels == 4) p[3] = 255;
        }
    }
}

// --------------------------------------------------
// Suppression BRUTE des paquets
// Seuil FIXÉ À 35000
// --------------------------------------------------
static void remove_large_blobs_fixed(GdkPixbuf *pixbuf)
{
    int w = gdk_pixbuf_get_width(pixbuf);
    int h = gdk_pixbuf_get_height(pixbuf);
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    unsigned char *visited = g_malloc0(w * h);
    int *stack = NULL;
    int stack_cap = 0;

    int min_area = 35000; // FIXE

    // 15% de l'image (pour protéger la grille si jamais elle est géante)
    int safe_size = (w * h) * 0.15; 

    printf("\n--- CLEANING (Seuil FIXE: %d) ---\n", min_area);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            if (visited[idx]) continue;
            guchar *p = pixels + y * rowstride + x * channels;
            if (p[0] != 0) { visited[idx] = 1; continue; }

            int sp = 0;
            if (stack_cap == 0) {
                stack_cap = 65536;
                stack = (int *)g_malloc(sizeof(int) * stack_cap);
            }

            stack[sp++] = idx;
            visited[idx] = 1;
            int area = 0;
            int min_x = x, max_x = x, min_y = y, max_y = y;

            // SCAN
            while (sp > 0) {
                int cur = stack[--sp];
                area++;
                int cy = cur / w; int cx = cur % w;

                if (cx < min_x) min_x = cx; 
                if (cx > max_x) max_x = cx;
                if (cy < min_y) min_y = cy; 
                if (cy > max_y) max_y = cy;

                const int nx[4] = {-1, 1, 0, 0}; const int ny[4] = {0, 0, -1, 1};
                for (int k = 0; k < 4; k++) {
                    int nx_x = cx + nx[k]; int ny_y = cy + ny[k];
                    if (nx_x < 0 || nx_x >= w || ny_y < 0 || ny_y >= h) continue;
                    int nidx = ny_y * w + nx_x;
                    if (visited[nidx]) continue;
                    guchar *pn = pixels + ny_y * rowstride + nx_x * channels;
                    if (pn[0] == 0) {
                         if (sp >= stack_cap) {
                            stack_cap *= 2;
                            stack = (int *)g_realloc(stack, sizeof(int) * stack_cap);
                        }
                        stack[sp++] = nidx;
                        visited[nidx] = 1;
                    }
                }
            }

            // SUPPRESSION (Si > 35000 et pas gigantesque comme la grille)
            if (area >= min_area) {
                if (area > safe_size) {
                    printf("  [PROTECTED] Main Grid/Frame detected (Area: %d)\n", area);
                } else {
                    printf("  [DELETED]   Large Blob (Area: %d)\n", area);
                    
                    sp = 0; stack[sp++] = y * w + x;
                    guchar *seed = pixels + y * rowstride + x * channels;
                    seed[0] = seed[1] = seed[2] = 255; 
                    
                    while (sp > 0) {
                        int cur = stack[--sp];
                        int cy = cur / w; int cx = cur % w;
                        const int nx[4] = {-1, 1, 0, 0}; const int ny[4] = {0, 0, -1, 1};
                        for (int k = 0; k < 4; k++) {
                            int nx_x = cx + nx[k]; int ny_y = cy + ny[k];
                            if (nx_x < 0 || nx_x >= w || ny_y < 0 || ny_y >= h) continue;
                            guchar *pn = pixels + ny_y * rowstride + nx_x * channels;
                            if (pn[0] == 0) {
                                pn[0] = pn[1] = pn[2] = 255; if (channels == 4) pn[3] = 255;
                                if (sp >= stack_cap) {
                                    stack_cap *= 2;
                                    stack = (int *)g_realloc(stack, sizeof(int) * stack_cap);
                                }
                                stack[sp++] = ny_y * w + nx_x;
                            }
                        }
                    }
                }
            }
        }
    }
    if (stack) g_free(stack);
    g_free(visited);
}

// --------------------------------------------------
// AJOUT DE CADRE (Scan Intelligent)
// --------------------------------------------------
static void add_smart_frame_v2(GdkPixbuf *pixbuf)
{
    int w = gdk_pixbuf_get_width(pixbuf);
    int h = gdk_pixbuf_get_height(pixbuf);
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    // 1. Projection Horizontale
    int *proj_y = calloc(h, sizeof(int));
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *p = pixels + y * rowstride + x * channels;
            if (p[0] == 0) proj_y[y]++;
        }
    }

    // 2. Scan du BAS vers le HAUT
    int grid_bottom = -1;
    int grid_top = -1;
    int gap_counter = 0;
    
    for (int y = h - 1; y >= 0; y--) {
        if (proj_y[y] > 5) {
            if (grid_bottom == -1) grid_bottom = y;
        }
        
        if (grid_bottom != -1) {
            if (proj_y[y] < 5) {
                gap_counter++;
                if (gap_counter > 20) {
                    grid_top = y + gap_counter; 
                    break;
                }
            } else {
                gap_counter = 0;
            }
        }
    }
    
    if (grid_top == -1) grid_top = 0; 
    
    // 3. Scan Horizontal
    int min_x = w, max_x = 0;
    for (int y = grid_top; y <= grid_bottom; y++) {
        for (int x = 0; x < w; x++) {
            guchar *p = pixels + y * rowstride + x * channels;
            if (p[0] == 0) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
            }
        }
    }

    free(proj_y);

    if (max_x > min_x && grid_bottom > grid_top) {
        printf("[Frame] Grid Detected: X[%d-%d] Y[%d-%d]\n", min_x, max_x, grid_top, grid_bottom);
        
        int p = 15;
        int x1 = (min_x - p < 0) ? 0 : min_x - p;
        int y1 = (grid_top - p < 0) ? 0 : grid_top - p;
        int x2 = (max_x + p >= w) ? w - 1 : max_x + p;
        int y2 = (grid_bottom + p >= h) ? h - 1 : grid_bottom + p;

        int thick = 5;
        for (int y = y1; y < y1 + thick; y++) 
            for (int x = x1; x <= x2; x++) { guchar *pt=pixels+y*rowstride+x*channels; pt[0]=pt[1]=pt[2]=0; if(channels==4) pt[3]=255; }
        for (int y = y2 - thick; y <= y2; y++) 
            for (int x = x1; x <= x2; x++) { guchar *pt=pixels+y*rowstride+x*channels; pt[0]=pt[1]=pt[2]=0; if(channels==4) pt[3]=255; }
        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x < x1 + thick; x++) { guchar *pt=pixels+y*rowstride+x*channels; pt[0]=pt[1]=pt[2]=0; if(channels==4) pt[3]=255; }
            for (int x = x2 - thick; x <= x2; x++) { guchar *pt=pixels+y*rowstride+x*channels; pt[0]=pt[1]=pt[2]=0; if(channels==4) pt[3]=255; }
        }
    }
}

// --------------------------------------------------
// BOUTONS ET CALLBACKS
// --------------------------------------------------

static void on_clean_clicked(GtkWidget *widget, gpointer user_data)
{
    (void)widget; (void)user_data;
    
    if (!original_pixbuf) {
        printf("[Error] No image to clean. Load one first.\n");
        return;
    }

    if (current_display_pixbuf) g_object_unref(current_display_pixbuf);
    current_display_pixbuf = rotate_pixbuf_any_angle(original_pixbuf, current_angle);

    // 1. Noir et Blanc
    apply_black_and_white(current_display_pixbuf);
    
    // 2. Suppression Brute (Seuil fixé à 35000)
    remove_large_blobs_fixed(current_display_pixbuf);

    // 3. Ajout du cadre Intelligent (Scan Bas -> Haut)
    add_smart_frame_v2(current_display_pixbuf);

    gtk_image_set_from_pixbuf(GTK_IMAGE(image_widget), current_display_pixbuf);
}

static void on_save_clicked(GtkWidget *widget, gpointer user_data)
{
    (void)widget; (void)user_data;
    if (!current_display_pixbuf) {
        printf("[Error] No image to save.\n");
        return;
    }

    // On s'assure que ce qui est sauvegardé est en Noir & Blanc
    GdkPixbuf *to_save = gdk_pixbuf_copy(current_display_pixbuf);
    apply_black_and_white(to_save);

    char output_path[1024];
    const char *basename_str = (selected_image_path[0] != '\0') ? g_path_get_basename(selected_image_path) : "image.png";
    
    if (selected_image_path[0] != '\0') 
        g_snprintf(output_path, sizeof(output_path), "images/%s_processed.png", basename_str);
    else 
        g_snprintf(output_path, sizeof(output_path), "images/output_processed.png");

    // Crée le dossier de sortie au besoin
    g_mkdir_with_parents("images", 0755);

    GError *err = NULL;
    if (gdk_pixbuf_save(to_save, output_path, "png", &err, NULL)) {
        printf("[OK] Saved to: %s\n", output_path);
        // Lance la détection sur l'image sauvegardée dans un processus séparé
        gchar *detect_argv[] = {"./ocr_project", "detect", output_path, NULL};
        GError *spawn_err = NULL;
        gint exit_status = 0;
        if (!g_spawn_sync(NULL, detect_argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, &exit_status, &spawn_err)) {
            printf("[Error] Lancement détection échoué: %s\n", spawn_err ? spawn_err->message : "inconnu");
            if (spawn_err) g_error_free(spawn_err);
        }
        // Après détection, génère le fichier GRID à partir des cells
        generate_grid_from_cells();
        // Génère GRID_Word à partir de letterInWord
        generate_words_from_letterInWord();
        // Lance le solver sur la grille et les mots détectés
        solver_run_words("GRID", "GRID_Word");
        // Colorie les mots trouvés et affiche l'image annotée
        const char *annotated_path = "images/annotated.png";
        highlight_words_on_image(output_path, "GRID", "GRID_Word", "grid_bbox.txt", annotated_path);
        GtkWidget *img_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(img_win), "Resultat");
        gtk_window_set_default_size(GTK_WINDOW(img_win), 900, 750);
        GtkWidget *img = gtk_image_new_from_file(annotated_path);
        gtk_container_add(GTK_CONTAINER(img_win), img);
        gtk_widget_show_all(img_win);
    } else { 
        printf("[Error] Save failed: %s\n", err->message); 
        g_error_free(err); 
    }
    
    g_object_unref(to_save);
}

static void on_load_button_clicked(GtkWidget *widget, gpointer window)
{
    (void)widget;
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open Image", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *f = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        load_image_from_file(f);
        g_free(f);
    }
    gtk_widget_destroy(dialog);
}

// --------------------------------------------------
// Main UI Setup
// --------------------------------------------------
void run_gui(int argc, char *argv[])
{
    gtk_init(&argc, &argv);
    if (argc > 1) startup_image_path = argv[1];
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "OCR Cleaner (Final)");
    gtk_window_set_default_size(GTK_WINDOW(win), 900, 750);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(win), box);

    // Zone Image
    image_widget = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(box), image_widget, TRUE, TRUE, 0);

    // 1. Rotation Slider
    GtkWidget *rot_label = gtk_label_new("Rotation:");
    gtk_box_pack_start(GTK_BOX(box), rot_label, FALSE, FALSE, 0);
    scale_widget = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -45, 45, 0.1);
    gtk_box_pack_start(GTK_BOX(box), scale_widget, FALSE, FALSE, 0);
    g_signal_connect(scale_widget, "value-changed", G_CALLBACK(on_rotation_changed), NULL);

    // Boutons
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(hbox, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 10);

    GtkWidget *btn_load = gtk_button_new_with_label("Load Image");
    g_signal_connect(btn_load, "clicked", G_CALLBACK(on_load_button_clicked), win);
    gtk_box_pack_start(GTK_BOX(hbox), btn_load, FALSE, FALSE, 5);

    GtkWidget *btn_auto = gtk_button_new_with_label("Auto Rotate");
    g_signal_connect(btn_auto, "clicked", G_CALLBACK(on_auto_rotate_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), btn_auto, FALSE, FALSE, 5);

    GtkWidget *btn_clean = gtk_button_new_with_label("Clean");
    g_signal_connect(btn_clean, "clicked", G_CALLBACK(on_clean_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), btn_clean, FALSE, FALSE, 5);

    GtkWidget *btn_save = gtk_button_new_with_label("Suivant");
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_save_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), btn_save, FALSE, FALSE, 5);

    GtkWidget *btn_quit = gtk_button_new_with_label("Quit");
    g_signal_connect(btn_quit, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), btn_quit, FALSE, FALSE, 5);

    gtk_widget_show_all(win);
    if (startup_image_path) load_image_from_file(startup_image_path);
    gtk_main();
}

// gui.c
// GTK interface with AUTO rotation + manual tweak.
// Features: Hough Transform for auto-deskewing, smart blob removal, and text repair.
// Tweaked for thinner resulting text.

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static char selected_image_path[512] = {0};
static GtkWidget *image_widget = NULL;
static GtkWidget *scale_widget = NULL;
static GdkPixbuf *original_pixbuf = NULL;
static GdkPixbuf *rotated_pixbuf = NULL;
static double current_angle = 0.0;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --------------------------------------------------
// Helper: Wrapper for GdkPixbufDestroyNotify
// --------------------------------------------------
static void free_pixbuf_data(guchar *data, gpointer user_data)
{
    (void)user_data;
    g_free(data);
}

// --------------------------------------------------
// Rotation Logic (High Quality)
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
                guchar val = 255; // Default white background
                if (isx >= 0 && isy >= 0 && isx < src_w && isy < src_h)
                    val = src_pixels[isy * rowstride + isx * channels + c];

                dest_pixels[y * dest_rowstride + x * channels + c] = val;
            }
        }
    }

    return gdk_pixbuf_new_from_data(dest_pixels, GDK_COLORSPACE_RGB, channels == 4, 8, dest_w, dest_h, dest_rowstride, free_pixbuf_data, NULL);
}

// --------------------------------------------------
// Update Display
// --------------------------------------------------
static void update_image_rotation(double angle)
{
    if (!original_pixbuf) return;
    if (rotated_pixbuf) g_object_unref(rotated_pixbuf);
    
    rotated_pixbuf = rotate_pixbuf_any_angle(original_pixbuf, angle);
    gtk_image_set_from_pixbuf(GTK_IMAGE(image_widget), rotated_pixbuf);
}

static void on_rotation_changed(GtkRange *range, gpointer user_data)
{
    (void)user_data;
    current_angle = gtk_range_get_value(range);
    update_image_rotation(current_angle);
}

// --------------------------------------------------
// Auto-Rotation: Hough Transform Logic
// --------------------------------------------------
static double detect_skew_angle(GdkPixbuf *pixbuf)
{
    int w = gdk_pixbuf_get_width(pixbuf);
    int h = gdk_pixbuf_get_height(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    // 1. Edge Detection (Simple Sobel Approximation)
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

    // 2. Simple Hough Transform for Lines
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

    // Find Peak
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
    if (abs(best_theta - 90) < 45) correction = 90 - best_theta;
    else if (abs(best_theta + 90) < 45) correction = -90 - best_theta;
    else correction = -best_theta;

    printf("[Auto] Detected dominant line angle: %.2f. Correction: %.2f\n", best_theta, correction);
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

// --------------------------------------------------
// Binarization (Black & White)
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
    
    // MODIFIED: Lowered threshold multiplier from 0.90 to 0.85.
    // This makes the initial binary letters thinner/skinnier.
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
// Dilation (Gentle Repair)
// --------------------------------------------------
static void apply_dilation(GdkPixbuf *pixbuf)
{
    int w = gdk_pixbuf_get_width(pixbuf);
    int h = gdk_pixbuf_get_height(pixbuf);
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    
    guchar *src = g_malloc0(rowstride * h);
    memcpy(src, pixels, rowstride * h);

    // Uses a 4-connected kernel (cross shape) to gently thicken/repair text.
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (src[y * rowstride + x * channels] == 0) continue;
            int become_black = 0;
            if (y > 0 && src[(y - 1) * rowstride + x * channels] == 0) become_black = 1;
            else if (y < h - 1 && src[(y + 1) * rowstride + x * channels] == 0) become_black = 1;
            else if (x > 0 && src[y * rowstride + (x - 1) * channels] == 0) become_black = 1;
            else if (x < w - 1 && src[y * rowstride + (x + 1) * channels] == 0) become_black = 1;
            
            if (become_black) {
                guchar *p = pixels + y * rowstride + x * channels;
                p[0] = p[1] = p[2] = 0; 
                if (channels == 4) p[3] = 255;
            }
        }
    }
    g_free(src);
}

// --------------------------------------------------
// Blob Removal (Smart: Size + Solidity check)
// --------------------------------------------------
static void remove_large_blobs(GdkPixbuf *pixbuf, int min_area)
{
    int w = gdk_pixbuf_get_width(pixbuf);
    int h = gdk_pixbuf_get_height(pixbuf);
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    unsigned char *visited = g_malloc0(w * h);
    int *stack = NULL;
    int stack_cap = 0;

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

            while (sp > 0) {
                int cur = stack[--sp];
                area++;
                int cy = cur / w;
                int cx = cur % w;
                if (cx < min_x) min_x = cx; if (cx > max_x) max_x = cx;
                if (cy < min_y) min_y = cy; if (cy > max_y) max_y = cy;

                const int nx[4] = {-1, 1, 0, 0};
                const int ny[4] = {0, 0, -1, 1};
                for (int k = 0; k < 4; k++) {
                    int nx_x = cx + nx[k];
                    int ny_y = cy + ny[k];
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

            long long bb_area = (long long)(max_x - min_x + 1) * (max_y - min_y + 1);
            double solidity = (double)area / bb_area;

            // Remove if large AND solid (birds). Keep hollow frames.
            if (area >= min_area && solidity > 0.4) {
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
    if (stack) g_free(stack);
    g_free(visited);
}

// --------------------------------------------------
// Save Pipeline
// --------------------------------------------------
static void on_save_rotation(GtkWidget *widget, gpointer user_data)
{
    (void)widget; (void)user_data;
    if (!original_pixbuf) { printf("No image.\n"); return; }

    GdkPixbuf *src = rotated_pixbuf ? rotated_pixbuf : original_pixbuf;
    GdkPixbuf *processed = gdk_pixbuf_copy(src);

    // Pipeline steps:
    apply_black_and_white(processed); // 1. Binarize (thinner start)
    remove_large_blobs(processed, 2000); // 2. Remove solid blobs
    apply_dilation(processed); // 3. Repair/Thicken gently

    char output_path[1024];
    const char *basename_str = (selected_image_path[0] != '\0') ? g_path_get_basename(selected_image_path) : "image.png";
    if (selected_image_path[0] != '\0') g_snprintf(output_path, sizeof(output_path), "../loader/images/%s_clean.png", basename_str);
    else g_snprintf(output_path, sizeof(output_path), "../loader/images/output_clean.png");

    GError *err = NULL;
    if (gdk_pixbuf_save(processed, output_path, "png", &err, NULL)) printf("Saved: %s\n", output_path);
    else { printf("Error: %s\n", err->message); g_error_free(err); }
    g_object_unref(processed);
}

// --------------------------------------------------
// Load
// --------------------------------------------------
static void on_load_button_clicked(GtkWidget *widget, gpointer window)
{
    (void)widget;
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *f = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (original_pixbuf) g_object_unref(original_pixbuf);
        original_pixbuf = gdk_pixbuf_new_from_file(f, NULL);
        if (original_pixbuf) {
            gtk_image_set_from_pixbuf(GTK_IMAGE(image_widget), original_pixbuf);
            g_snprintf(selected_image_path, sizeof(selected_image_path), "%s", f);
        }
        g_free(f);
    }
    gtk_widget_destroy(dialog);
}

// --------------------------------------------------
// Main
// --------------------------------------------------
void run_gui(int argc, char *argv[])
{
    gtk_init(&argc, &argv);
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Auto-Level Cleaner (Thinner Text)");
    gtk_window_set_default_size(GTK_WINDOW(win), 800, 600);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(win), box);

    image_widget = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(box), image_widget, TRUE, TRUE, 0);

    scale_widget = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -45, 45, 0.1);
    gtk_box_pack_start(GTK_BOX(box), scale_widget, FALSE, FALSE, 0);
    g_signal_connect(scale_widget, "value-changed", G_CALLBACK(on_rotation_changed), NULL);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);

    GtkWidget *btn_load = gtk_button_new_with_label("Load Image");
    g_signal_connect(btn_load, "clicked", G_CALLBACK(on_load_button_clicked), win);
    gtk_box_pack_start(GTK_BOX(hbox), btn_load, TRUE, TRUE, 0);

    GtkWidget *btn_auto = gtk_button_new_with_label("Auto Rotate");
    g_signal_connect(btn_auto, "clicked", G_CALLBACK(on_auto_rotate_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), btn_auto, TRUE, TRUE, 0);

    GtkWidget *btn_save = gtk_button_new_with_label("Save & Clean");
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_save_rotation), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), btn_save, TRUE, TRUE, 0);

    gtk_widget_show_all(win);
    gtk_main();
}
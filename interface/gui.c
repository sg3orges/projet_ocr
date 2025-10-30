// gui.c
// GTK interface with display + manual rotation (smooth rotation)
// --------------------------------------------------

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
// Wrapper for GdkPixbufDestroyNotify
// --------------------------------------------------
static void free_pixbuf_data(guchar *data, gpointer user_data)
{
    (void)user_data;
    g_free(data);
}

// --------------------------------------------------
// Smooth rotation of a GdkPixbuf (any angle)
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

    return gdk_pixbuf_new_from_data(
        dest_pixels, GDK_COLORSPACE_RGB,
        channels == 4, 8,
        dest_w, dest_h,
        dest_rowstride,
        free_pixbuf_data, NULL);
}

// --------------------------------------------------
// Apply rotation to the image and update display
// --------------------------------------------------
static void update_image_rotation(double angle)
{
    if (!original_pixbuf)
        return;

    if (rotated_pixbuf)
        g_object_unref(rotated_pixbuf);

    rotated_pixbuf = rotate_pixbuf_any_angle(original_pixbuf, angle);
    gtk_image_set_from_pixbuf(GTK_IMAGE(image_widget), rotated_pixbuf);
}

// --------------------------------------------------
// Callback for the rotation slider
// --------------------------------------------------
static void on_rotation_changed(GtkRange *range, gpointer user_data)
{
    (void)user_data;
    current_angle = gtk_range_get_value(range);
    update_image_rotation(current_angle);
}

// --------------------------------------------------
// Convert image to adaptive black & white
// --------------------------------------------------
static void apply_black_and_white(GdkPixbuf *pixbuf)
{
    int w = gdk_pixbuf_get_width(pixbuf);
    int h = gdk_pixbuf_get_height(pixbuf);
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    // Calculate average brightness
    double mean = 0.0;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            guchar *p = pixels + y * rowstride + x * channels;
            double gray = 0.3*p[0] + 0.59*p[1] + 0.11*p[2];
            mean += gray;
        }
    }
    mean /= (w * h);
    double threshold = mean * 0.85;

    // B/W conversion
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            guchar *p = pixels + y * rowstride + x * channels;
            double gray = 0.3*p[0] + 0.59*p[1] + 0.11*p[2];
            guchar val = (gray > threshold) ? 255 : 0;
            p[0] = p[1] = p[2] = val;
        }
    }
}

// --------------------------------------------------
// Save the currently displayed image in B/W
// --------------------------------------------------
static void on_save_rotation(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    (void)user_data;

    if (!rotated_pixbuf)
    {
        printf("[Error] No image to save.\n");
        return;
    }

    // Clone pixbuf to avoid changing the display
    GdkPixbuf *bw_pixbuf = gdk_pixbuf_copy(rotated_pixbuf);

    // Convert to black & white
    apply_black_and_white(bw_pixbuf);

    char output_path[1024];
    g_snprintf(output_path, sizeof(output_path), "../loader/images/%s_rotated_bw.png",
               g_path_get_basename(selected_image_path));

    GError *error = NULL;
    if (!gdk_pixbuf_save(bw_pixbuf, output_path, "png", &error, NULL))
    {
        printf("[Error] Failed to save image: %s\n", error->message);
        g_error_free(error);
        g_object_unref(bw_pixbuf);
        return;
    }

    g_object_unref(bw_pixbuf);
    printf("[OK] Image saved in black & white (%.2f°) -> %s\n",
           current_angle, output_path);
}

// --------------------------------------------------
// Load an image using the file chooser
// --------------------------------------------------
static void on_load_button_clicked(GtkWidget *widget, gpointer window)
{
    (void)widget;
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new("Open an image",
                                         GTK_WINDOW(window),
                                         action,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Open", GTK_RESPONSE_ACCEPT,
                                         NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_filter_add_mime_type(filter, "image/bmp");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT)
    {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        if (original_pixbuf)
            g_object_unref(original_pixbuf);

        original_pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
        if (!original_pixbuf)
        {
            printf("[Error] Failed to load image.\n");
            g_free(filename);
            gtk_widget_destroy(dialog);
            return;
        }

        g_snprintf(selected_image_path, sizeof(selected_image_path), "%s", filename);
        gtk_image_set_from_pixbuf(GTK_IMAGE(image_widget), original_pixbuf);

        printf("[Info] Image loaded: %s\n", filename);

        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

// --------------------------------------------------
// Main window
// --------------------------------------------------
void run_gui(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Manual Image Rotation");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 700);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Image area
    image_widget = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(vbox), image_widget, TRUE, TRUE, 0);

    // Rotation slider
    scale_widget = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -180, 180, 1);
    gtk_scale_set_digits(GTK_SCALE(scale_widget), 1);
    gtk_range_set_value(GTK_RANGE(scale_widget), 0.0); // initial position at 0°
    gtk_box_pack_start(GTK_BOX(vbox), scale_widget, FALSE, FALSE, 0);

    // Buttons
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *btn_load = gtk_button_new_with_label("Load Image");
    GtkWidget *btn_save = gtk_button_new_with_label("Save Rotation");
    GtkWidget *btn_quit = gtk_button_new_with_label("Quit");

    gtk_box_pack_start(GTK_BOX(hbox), btn_load, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), btn_save, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), btn_quit, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    // Signals
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(btn_load, "clicked", G_CALLBACK(on_load_button_clicked), window);
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_save_rotation), NULL);
    g_signal_connect(scale_widget, "value-changed", G_CALLBACK(on_rotation_changed), NULL);
    g_signal_connect(btn_quit, "clicked", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);
    gtk_main();
}

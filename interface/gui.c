#include "gui.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>

static GtkWidget *main_window = NULL;

static void on_quit_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    (void)user_data;
    gtk_main_quit();
}

static void on_image_window_closed(GtkWidget *window, gpointer user_data)
{
    (void)window;
    (void)user_data;
    if (main_window)
        gtk_widget_show_all(main_window);
}

static void on_back_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    GtkWidget *image_window = GTK_WIDGET(user_data);
    gtk_widget_destroy(image_window);
}

static void on_image_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    (void)user_data;

    if (!main_window)
        return;

    gtk_widget_hide(main_window);

    GtkWidget *image_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(image_window), "Image");
    gtk_window_set_default_size(GTK_WINDOW(image_window), 1200, 800);
    g_signal_connect(image_window, "destroy", G_CALLBACK(on_image_window_closed), NULL);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(root), 12);
    gtk_container_add(GTK_CONTAINER(image_window), root);

    // Applique le même fond bleu
    GdkRGBA bg;
    gdk_rgba_parse(&bg, "#1e88e5");
    gtk_widget_override_background_color(image_window, GTK_STATE_FLAG_NORMAL, &bg);
    gtk_widget_override_background_color(root, GTK_STATE_FLAG_NORMAL, &bg);

    gtk_widget_set_halign(root, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(root, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(root, TRUE);
    gtk_widget_set_vexpand(root, TRUE);

    // Zone de défilement qui contiendra la grille d’images + boutons
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_set_size_request(scroll, 1000, 600); // zone de défilement plus grande
    gtk_container_add(GTK_CONTAINER(root), scroll);

    // Grille : chaque image occupe une ligne et chaque bouton la ligne suivante
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(grid, GTK_ALIGN_START);
    gtk_container_add(GTK_CONTAINER(scroll), grid);

    const char *images[] = {
        "level_1_image_1.png",
        "level_1_image_2.png",
        "level_2_image_1.png",
        "level_2_image_2.png",
        "level_3_image_1.png",
        "level_3_image_2.png"
    };
    const size_t img_count = sizeof(images) / sizeof(images[0]);
    const int columns = 2; // moins de colonnes pour des vignettes plus grandes

    for (size_t i = 0; i < img_count; i++) {
        char path[256];
        g_snprintf(path, sizeof(path), "Exemples_dimages/%s", images[i]);

        GError *err = NULL;
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(path, 320, 240, TRUE, &err);
        GtkWidget *img_widget = NULL;
        if (pixbuf) {
            img_widget = gtk_image_new_from_pixbuf(pixbuf);
            g_object_unref(pixbuf);
        } else {
            img_widget = gtk_label_new("Impossible de charger l'image");
            g_warning("Erreur chargement %s: %s", path, err ? err->message : "inconnue");
            if (err) g_error_free(err);
        }

        int row = (int)(i / columns) * 2; // row for image, next row for button
        int col = (int)(i % columns);
        gtk_grid_attach(GTK_GRID(grid), img_widget, col, row, 1, 1);

        GtkWidget *btn = gtk_button_new_with_label("Choisir");
        gtk_widget_set_halign(btn, GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(grid), btn, col, row + 1, 1, 1);
    }

    GtkWidget *back_btn = gtk_button_new_with_label("Retour");
    gtk_widget_set_halign(back_btn, GTK_ALIGN_CENTER);
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), image_window);
    gtk_box_pack_end(GTK_BOX(root), back_btn, FALSE, FALSE, 0);

    gtk_widget_show_all(image_window);
}

void run_interface(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    main_window = window;
    gtk_window_set_title(GTK_WINDOW(window), "Projet OCR");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    gtk_container_add(GTK_CONTAINER(window), box);
    GdkRGBA bg;
    gdk_rgba_parse(&bg, "#1e88e5");
    gtk_widget_override_background_color(window, GTK_STATE_FLAG_NORMAL, &bg);
    gtk_widget_override_background_color(box, GTK_STATE_FLAG_NORMAL, &bg);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_vexpand(box, TRUE);

    GtkWidget *title = gtk_label_new("Projet Ocr");
    gtk_label_set_xalign(GTK_LABEL(title), 0.5);
    gtk_label_set_justify(GTK_LABEL(title), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    GtkWidget *info = gtk_label_new("Ajoute ici tes widgets et callbacks.");
    gtk_label_set_xalign(GTK_LABEL(info), 0.5);
    gtk_label_set_justify(GTK_LABEL(info), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(box), info, FALSE, FALSE, 0);

    GtkWidget *image_btn = gtk_button_new_with_label("Image");
    gtk_widget_set_halign(image_btn, GTK_ALIGN_CENTER);
    g_signal_connect(image_btn, "clicked", G_CALLBACK(on_image_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box), image_btn, FALSE, FALSE, 0);

    GtkWidget *button = gtk_button_new_with_label("Quitter");
    gtk_widget_set_halign(button, GTK_ALIGN_CENTER);
    g_signal_connect(button, "clicked", G_CALLBACK(on_quit_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(box), button, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
    gtk_main();
}

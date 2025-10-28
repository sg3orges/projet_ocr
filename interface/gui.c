// interface/gui.c
// Interface GTK : sélection d'image, copie vers ../loader/images/
// et bouton "Résoudre" qui lance le module loader_test
// -------------------------------------------------------------

#include <gtk/gtk.h>
#include "gui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
  #include <direct.h> // pour _mkdir
#endif

// buffer pour conserver le nom du fichier sélectionné (base name)
static char selected_image_name[256] = {0};

// -------------------------------------------------------------
// Utilitaire : copie un fichier binaire (image) de src vers dst
// Retour : 1 si OK, 0 sinon
// -------------------------------------------------------------
static int copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in) {
        printf("[Erreur] Impossible d'ouvrir la source : %s\n", src);
        return 0;
    }

    FILE *out = fopen(dst, "wb");
    if (!out) {
        printf("[Erreur] Impossible de créer la destination : %s\n", dst);
        fclose(in);
        return 0;
    }

    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, n, out) != n) {
            fclose(in); fclose(out);
            printf("[Erreur] Écriture interrompue\n");
            return 0;
        }
    }

    fclose(in);
    fclose(out);
    return 1;
}

// -------------------------------------------------------------
// Callback : bouton "Quitter"
// -------------------------------------------------------------
static void on_quit_button_clicked(GtkWidget *widget, gpointer window)
{
    (void)widget;
    gtk_window_close(GTK_WINDOW(window));
}

// -------------------------------------------------------------
// Callback : bouton "Charger une image"
// - ouvre le file chooser
// - copie le fichier choisi dans ../loader/images/
// - met à jour selected_image_name
// Ne lance PAS le traitement.
// -------------------------------------------------------------
static void on_load_button_clicked(GtkWidget *widget, gpointer window)
{
    (void)widget;

    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new("Ouvrir une image",
                                         GTK_WINDOW(window),
                                         action,
                                         "_Annuler", GTK_RESPONSE_CANCEL,
                                         "_Ouvrir", GTK_RESPONSE_ACCEPT,
                                         NULL);

    // filtre images
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_filter_add_mime_type(filter, "image/bmp");
    gtk_file_filter_add_pattern(filter, "*.png");
    gtk_file_filter_add_pattern(filter, "*.jpg");
    gtk_file_filter_add_pattern(filter, "*.jpeg");
    gtk_file_filter_add_pattern(filter, "*.bmp");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (!filename) {
            gtk_widget_destroy(dialog);
            return;
        }

        printf("[Info] Fichier sélectionné : %s\n", filename);

        // extrait le nom de base (ex: image.png)
        const char *base = strrchr(filename, '/');
        #ifdef _WIN32
            /* sous Windows les chemins peuvent contenir '\' */
            if (!base) base = strrchr(filename, '\\');
        #endif
        base = (base) ? base + 1 : filename;

        // crée le dossier ../loader/images si nécessaire
        #ifdef _WIN32
            _mkdir("../loader");
            _mkdir("../loader/images");
        #else
            mkdir("../loader", 0777); // ignore si existe
            mkdir("../loader/images", 0777);
        #endif

        // construit chemin destination
        char dest[512];
        snprintf(dest, sizeof(dest), "../loader/images/%s", base);

        // copie
        if (!copy_file(filename, dest)) {
            GtkWidget *err = gtk_message_dialog_new(GTK_WINDOW(window),
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_OK,
                                                   "Erreur lors de la copie vers :\n%s",
                                                   dest);
            gtk_dialog_run(GTK_DIALOG(err));
            gtk_widget_destroy(err);
            g_free(filename);
            gtk_widget_destroy(dialog);
            return;
        }

        // stocke le nom de l'image (base name) pour le bouton "Résoudre"
        strncpy(selected_image_name, base, sizeof(selected_image_name) - 1);
        selected_image_name[sizeof(selected_image_name)-1] = '\0';

        // message de confirmation
        GtkWidget *info = gtk_message_dialog_new(GTK_WINDOW(window),
                                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_INFO,
                                                 GTK_BUTTONS_OK,
                                                 "Image copiée dans :\n%s\n\nPrête à être résolue.",
                                                 dest);
        gtk_dialog_run(GTK_DIALOG(info));
        gtk_widget_destroy(info);

        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

// -------------------------------------------------------------
// Callback : bouton "Résoudre"
// - vérifie qu'une image a été sélectionnée
// - écrit ../loader/last_image.txt avec le nom sélectionné
// - lance ../loader/loader_test
// -------------------------------------------------------------
static void on_solve_button_clicked(GtkWidget *widget, gpointer window)
{
    (void)widget;

    if (selected_image_name[0] == '\0') {
        GtkWidget *warn = gtk_message_dialog_new(GTK_WINDOW(window),
                                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_WARNING,
                                                 GTK_BUTTONS_OK,
                                                 "Aucune image sélectionnée.\nVeuillez d'abord charger une image.");
        gtk_dialog_run(GTK_DIALOG(warn));
        gtk_widget_destroy(warn);
        return;
    }

    // écrit last_image.txt dans ../loader/
    char last_path[512];
    snprintf(last_path, sizeof(last_path), "../loader/last_image.txt");
    FILE *f = fopen(last_path, "w");
    if (!f) {
        GtkWidget *err = gtk_message_dialog_new(GTK_WINDOW(window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_OK,
                                               "Impossible d'écrire :\n%s", last_path);
        gtk_dialog_run(GTK_DIALOG(err));
        gtk_widget_destroy(err);
        return;
    }
    fprintf(f, "%s\n", selected_image_name);
    fclose(f);

    // lance le loader_test (sous-processus)
    // change la commande si ton exécutable a un autre nom
    const char *cmd = "cd ../loader && ./loader_test";
    printf("[Info] Executing: %s\n", cmd);

    int rc = system(cmd);

    if (rc == 0) {
        GtkWidget *done = gtk_message_dialog_new(GTK_WINDOW(window),
                                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_INFO,
                                                 GTK_BUTTONS_OK,
                                                 "✅ Résolution terminée avec succès !");
        gtk_dialog_run(GTK_DIALOG(done));
        gtk_widget_destroy(done);
    } else {
        GtkWidget *err = gtk_message_dialog_new(GTK_WINDOW(window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_OK,
                                               "❌ Erreur pendant l'exécution de loader_test (code %d).", rc);
        gtk_dialog_run(GTK_DIALOG(err));
        gtk_widget_destroy(err);
    }
}

// -------------------------------------------------------------
// run_gui : construit la fenêtre, boutons et signaux
// -------------------------------------------------------------
void run_gui(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "OCR Word Search Solver");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *label = gtk_label_new("Bienvenue dans OCR Word Search Solver !");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *button_load = gtk_button_new_with_label("📁 Charger une image");
    GtkWidget *button_solve = gtk_button_new_with_label("▶ Résoudre");
    GtkWidget *button_quit = gtk_button_new_with_label("❌ Quitter");

    gtk_box_pack_start(GTK_BOX(hbox), button_load, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), button_solve, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), button_quit, FALSE, FALSE, 0);

    // connexions
    g_signal_connect(button_load, "clicked", G_CALLBACK(on_load_button_clicked), window);
    g_signal_connect(button_solve, "clicked", G_CALLBACK(on_solve_button_clicked), window);
    g_signal_connect(button_quit, "clicked", G_CALLBACK(on_quit_button_clicked), window);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);
    gtk_main();
}

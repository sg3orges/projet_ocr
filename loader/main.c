#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "loader.h"

int main(void)
{
    char image_name[128] = {0};

    // Lecture du nom de fichier à traiter
    FILE *f = fopen("last_image.txt", "r");
    if (!f)
    {
        printf("[Erreur] Impossible d’ouvrir last_image.txt\n");
        return 1;
    }

    if (!fgets(image_name, sizeof(image_name), f))
    {
        printf("[Erreur] Aucun nom d’image trouvé.\n");
        fclose(f);
        return 1;
    }
    fclose(f);

    // Supprimer le saut de ligne si présent
    image_name[strcspn(image_name, "\n")] = '\0';

    // Construire le chemin complet
    char image_path[256];
    snprintf(image_path, sizeof(image_path), "images/%s", image_name);

    printf("[Info] Chargement de l’image : %s\n", image_path);

    // Charger l'image
    Image img = load_image(image_path);
    if (img.data == NULL)
    {
        printf("[Erreur] Impossible de charger l’image.\n");
        return 1;
    }
    if (img.width > 1600 || img.height > 1600)
    resize_image(&img, 1200, 900);

// 🔹 Étape 1 : petit contraste avant le redressement
enhance_contrast_light(&img);

// 🔹 Étape 2 : blurring
blur_image(&img);

// 🔹 Étape 2 : redressement
deskew_image(&img);

// 🔹 Étape 3 : vrai contraste fort après redressement
enhance_contrast(&img);

// 🔹 Étape 4 : conversion finale noir et blanc
to_black_and_white(&img);


    // Sauvegarde du résultat
    char output_path[256];
    snprintf(output_path, sizeof(output_path), "images/%s_bw.png", image_name);
    save_image(output_path, &img);

    // Supprimer l'image originale
    if (remove(image_path) == 0)
        printf("[Info] Image originale supprimée : %s\n", image_path);
    else
        printf("[Avertissement] Impossible de supprimer l’image originale.\n");

    // Supprimer le fichier last_image.txt pour ne pas retraiter la même
    if (remove("last_image.txt") == 0)
        printf("[Info] Fichier last_image.txt supprimé.\n");

    // Libérer la mémoire
    free_image(&img);

    printf("[OK] Traitement terminé avec succès : %s\n", output_path);
    return 0;
}

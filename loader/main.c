#include <stdio.h>
#include "loader.h"

int main(void)
{
    // Nom du fichier image à traiter (doit être présent dans le dossier images/)
    const char *input_path = "images/test1.png";

    // Chargement de l'image
    Image img = load_image(input_path);
    if (img.data == NULL)
        return 1;

    // Conversion en niveaux de gris
    to_grayscale(&img);
    save_image("images/test_gray.png", &img);

    // Conversion en noir et blanc (seuil = 128)
    to_black_and_white(&img, 128);
    save_image("images/test_bw.png", &img);

    // Libération de la mémoire
    free_image(&img);

    printf("[Info] Traitement terminé avec succès.\n");
    return 0;
}

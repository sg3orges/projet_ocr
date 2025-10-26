// -------------------------------------------------------------
// loader.c
// Chargement et conversion d'images (grayscale / noir et blanc)
// -------------------------------------------------------------

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image.h"
#include "stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include "loader.h"

// -------------------------------------------------------------
// Charge une image depuis un fichier
// -------------------------------------------------------------
Image load_image(const char *filename)
{
    Image img;
    img.data = stbi_load(filename, &img.width, &img.height, &img.channels, 0);

    if (img.data == NULL)
    {
        printf("[Erreur] Impossible de charger l'image : %s\n", filename);
        img.width = img.height = img.channels = 0;
    }
    else
    {
        printf("[Info] Image chargée : %s (%d x %d, %d canaux)\n",
               filename, img.width, img.height, img.channels);
    }

    return img;
}

// -------------------------------------------------------------
// Libère la mémoire utilisée par l'image
// -------------------------------------------------------------
void free_image(Image *img)
{
    if (img->data != NULL)
    {
        stbi_image_free(img->data);
        img->data = NULL;
    }
}

// -------------------------------------------------------------
// Convertit une image couleur en niveaux de gris
// -------------------------------------------------------------
void to_grayscale(Image *img)
{
    if (img->data == NULL || img->channels < 3)
    {
        printf("[Erreur] Image non valide pour conversion en gris.\n");
        return;
    }

    for (int y = 0; y < img->height; y++)
    {
        for (int x = 0; x < img->width; x++)
        {
            unsigned char *pixel = img->data + (y * img->width + x) * img->channels;

            unsigned char gray = (unsigned char)(0.3 * pixel[0] +
                                                 0.59 * pixel[1] +
                                                 0.11 * pixel[2]);

            pixel[0] = pixel[1] = pixel[2] = gray;
        }
    }

    printf("[Info] Conversion en niveaux de gris terminée.\n");
}

// -------------------------------------------------------------
// Convertit une image en noir et blanc à partir d'un seuil
// -------------------------------------------------------------
void to_black_and_white(Image *img, unsigned char threshold)
{
    if (img->data == NULL || img->channels < 3)
    {
        printf("[Erreur] Image non valide pour conversion en N/B.\n");
        return;
    }

    for (int y = 0; y < img->height; y++)
    {
        for (int x = 0; x < img->width; x++)
        {
            unsigned char *pixel = img->data + (y * img->width + x) * img->channels;
            unsigned char gray = pixel[0];
            unsigned char value = (gray > threshold) ? 255 : 0;
            pixel[0] = pixel[1] = pixel[2] = value;
        }
    }

    printf("[Info] Conversion en noir et blanc terminée (seuil = %d).\n", threshold);
}

// -------------------------------------------------------------
// Sauvegarde une image dans un fichier PNG
// -------------------------------------------------------------
void save_image(const char *filename, Image *img)
{
    if (img->data == NULL)
    {
        printf("[Erreur] Aucun pixel à sauvegarder.\n");
        return;
    }

    int ok = stbi_write_png(filename, img->width, img->height,
                            img->channels, img->data,
                            img->width * img->channels);

    if (ok)
        printf("[Info] Image sauvegardée dans : %s\n", filename);
    else
        printf("[Erreur] Échec de la sauvegarde : %s\n", filename);
}

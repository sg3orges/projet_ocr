// -------------------------------------------------------------
// loader.h
// Header pour le module de chargement et conversion d'image
// -------------------------------------------------------------

#ifndef LOADER_H
#define LOADER_H

// Structure représentant une image chargée en mémoire
typedef struct
{
    int width;           // Largeur en pixels
    int height;          // Hauteur en pixels
    int channels;        // Nombre de canaux (ex : 3 pour RGB)
    unsigned char *data; // Données brutes des pixels
} Image;

// Prototypes des fonctions
Image load_image(const char *filename);
void free_image(Image *img);
void to_grayscale(Image *img);
void to_black_and_white(Image *img, unsigned char threshold);
void save_image(const char *filename, Image *img);

#endif

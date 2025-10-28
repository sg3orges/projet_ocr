// -------------------------------------------------------------
// loader.c
// Chargement, redressement automatique, redimensionnement et conversion en noir et blanc
// -------------------------------------------------------------

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image.h"
#include "stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "loader.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
// Redimensionne une image pour limiter la taille mémoire
// -------------------------------------------------------------
void resize_image(Image *img, int new_w, int new_h)
{
    unsigned char *new_data = malloc(new_w * new_h * img->channels);
    if (!new_data)
    {
        printf("[Erreur] Allocation pour redimensionnement.\n");
        return;
    }

    for (int y = 0; y < new_h; y++)
    {
        for (int x = 0; x < new_w; x++)
        {
            int src_x = x * img->width / new_w;
            int src_y = y * img->height / new_h;
            for (int c = 0; c < img->channels; c++)
                new_data[(y * new_w + x) * img->channels + c] =
                    img->data[(src_y * img->width + src_x) * img->channels + c];
        }
    }

    free(img->data);
    img->data = new_data;
    img->width = new_w;
    img->height = new_h;

    printf("[Info] Image redimensionnée à %dx%d\n", new_w, new_h);
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

// -------------------------------------------------------------
// Redressement automatique (deskew)
// -------------------------------------------------------------
static inline unsigned char get_gray_pixel(const Image *img, int x, int y)
{
    if (x < 0 || y < 0 || x >= img->width || y >= img->height)
        return 255;
    return img->data[(y * img->width + x) * img->channels];
}
// -------------------------------------------------------------
// Amélioration légère du contraste (pré-traitement avant redressement)
// -------------------------------------------------------------
void enhance_contrast_light(Image *img)
{
    if (!img || !img->data || img->channels < 3)
        return;

    for (int y = 0; y < img->height; y++)
    {
        for (int x = 0; x < img->width; x++)
        {
            unsigned char *p = img->data + (y * img->width + x) * img->channels;
            unsigned char gray = (unsigned char)(0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2]);

            // Ajustement doux : fonce légèrement les pixels clairs sans saturer
            double adjusted = pow(gray / 255.0, 0.9) * 255.0;
            unsigned char v = (unsigned char)adjusted;

            p[0] = p[1] = p[2] = v;
        }
    }

    printf("[Info] Pré-contraste léger appliqué avant redressement.\n");
}

// -------------------------------------------------------------
// Améliore le contraste et fonce les caractères clairs
// -------------------------------------------------------------
void enhance_contrast(Image *img)
{
    if (!img || !img->data || img->channels < 3)
    {
        printf("[Erreur] Image invalide pour amélioration du contraste.\n");
        return;
    }

    unsigned char min_val = 255, max_val = 0;

    // Trouver les niveaux minimum et maximum de luminosité
    for (int y = 0; y < img->height; y++)
    {
        for (int x = 0; x < img->width; x++)
        {
            unsigned char *pixel = img->data + (y * img->width + x) * img->channels;
            unsigned char gray = (unsigned char)(0.3 * pixel[0] +
                                                 0.59 * pixel[1] +
                                                 0.11 * pixel[2]);

            if (gray < min_val) min_val = gray;
            if (gray > max_val) max_val = gray;
        }
    }

    if (max_val <= min_val)
    {
        printf("[Avertissement] Contraste insuffisant, aucun ajustement.\n");
        return;
    }

    printf("[Info] Rehaussement du contraste (min=%d, max=%d)\n", min_val, max_val);

    // Ajuster chaque pixel
    for (int y = 0; y < img->height; y++)
    {
        for (int x = 0; x < img->width; x++)
        {
            unsigned char *pixel = img->data + (y * img->width + x) * img->channels;
            unsigned char gray = (unsigned char)(0.3 * pixel[0] +
                                                 0.59 * pixel[1] +
                                                 0.11 * pixel[2]);

            // Normalisation linéaire + correction gamma légère
            double normalized = (double)(gray - min_val) / (max_val - min_val);
            normalized = pow(normalized, 0.8); // gamma <1 → fonce les tons clairs
            unsigned char adjusted = (unsigned char)(normalized * 255.0);

            pixel[0] = pixel[1] = pixel[2] = adjusted;
        }
    }

    printf("[Info] Contraste amélioré et tons clairs renforcés.\n");
}

// -------------------------------------------------------------
// Conversion en noir et blanc avec seuillage adaptatif
// -------------------------------------------------------------
void to_black_and_white(Image *img)
{
    if (img->data == NULL || img->channels < 3)
    {
        printf("[Erreur] Image non valide pour conversion en N/B.\n");
        return;
    }

    // Calcul de la luminosité moyenne globale
    double mean = 0.0;
    int total = img->width * img->height;

    for (int y = 0; y < img->height; y += 2) // échantillonnage tous les 2 px (plus rapide)
    {
        for (int x = 0; x < img->width; x += 2)
        {
            unsigned char *pixel = img->data + (y * img->width + x) * img->channels;
            unsigned char gray = (unsigned char)(0.3 * pixel[0] +
                                                 0.59 * pixel[1] +
                                                 0.11 * pixel[2]);
            mean += gray;
        }
    }

    mean /= (total / 4.0); // car on a sauté 1 pixel sur 2
    unsigned char threshold = (unsigned char)(mean * 0.85); // adapte le seuil à l'image

    printf("[Info] Luminosité moyenne = %.1f → seuil adaptatif = %d\n", mean, threshold);

    for (int y = 0; y < img->height; y++)
    {
        for (int x = 0; x < img->width; x++)
        {
            unsigned char *pixel = img->data + (y * img->width + x) * img->channels;

            unsigned char gray = (unsigned char)(0.3 * pixel[0] +
                                                 0.59 * pixel[1] +
                                                 0.11 * pixel[2]);

            unsigned char value = (gray > threshold) ? 255 : 0;
            pixel[0] = pixel[1] = pixel[2] = value;
        }
    }

    printf("[Info] Conversion adaptative terminée (seuil = %d)\n", threshold);
}
// -------------------------------------------------------------
// Flou léger (moyenne 3x3) pour aider la détection d'angle
// -------------------------------------------------------------
void blur_image(Image *img)
{
    if (!img || !img->data || img->channels < 3)
        return;

    unsigned char *copy = malloc(img->width * img->height * img->channels);
    if (!copy) return;

    memcpy(copy, img->data, img->width * img->height * img->channels);

    for (int y = 1; y < img->height - 1; y++)
    {
        for (int x = 1; x < img->width - 1; x++)
        {
            int sum = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                {
                    unsigned char *p = copy + ((y + dy) * img->width + (x + dx)) * img->channels;
                    sum += (unsigned char)(0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2]);
                }

            unsigned char avg = sum / 9;
            unsigned char *dst = img->data + (y * img->width + x) * img->channels;
            dst[0] = dst[1] = dst[2] = avg;
        }
    }

    free(copy);
    printf("[Info] Flou léger appliqué avant redressement.\n");
}


// -------------------------------------------------------------
// deskew_image : estimation d'angle par moments (PCA) + rotation
// plus robuste que la méthode par projection pour certaines images
// -------------------------------------------------------------


void deskew_image(Image *img)
{
    if (!img || !img->data) {
        printf("[Erreur] Image invalide pour redressement.\n");
        return;
    }

    const int W = img->width;
    const int H = img->height;

    if ((long long)W * H > 40000000LL) {
        printf("[Avertissement] Image très grande (%dx%d), redressement ignoré.\n", W, H);
        return;
    }

    unsigned char *bin = malloc((size_t)W * H);
    if (!bin) {
        printf("[Erreur] Allocation mémoire (bin)\n");
        return;
    }

    // --- Calcul de la luminosité moyenne ---
    double mean = 0.0;
    int samples = 0;
    for (int y = 0; y < H; y += 2) {
        for (int x = 0; x < W; x += 2) {
            unsigned char *p = img->data + (y * W + x) * img->channels;
            unsigned char gray = (unsigned char)(0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2]);
            mean += gray;
            samples++;
        }
    }
    mean /= (samples ? samples : 1);

    double thr = mean * 0.9;
    if (thr < 100) thr = mean * 0.85;

    long long black_count = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            unsigned char *p = img->data + (y * W + x) * img->channels;
            unsigned char gray = (unsigned char)(0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2]);
            unsigned char v = (gray < thr) ? 1 : 0;
            bin[y * W + x] = v;
            if (v) black_count++;
        }

    printf("[Debug] deskew: mean=%.1f thr=%.1f black_pixels=%lld\n", mean, thr, black_count);

    if (black_count < (W * H) / 10000 + 50) {
        printf("[Info] Pas assez de pixels noirs pour estimer angle.\n");
        free(bin);
        return;
    }

    // --- Calcul du centre et des moments ---
    double sum_x = 0, sum_y = 0, cnt = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (bin[y * W + x]) {
                sum_x += x;
                sum_y += y;
                cnt++;
            }

    double cx = sum_x / cnt;
    double cy = sum_y / cnt;

    double s_xx = 0, s_yy = 0, s_xy = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (bin[y * W + x]) {
                double dx = x - cx;
                double dy = y - cy;
                s_xx += dx * dx;
                s_yy += dy * dy;
                s_xy += dx * dy;
            }

    s_xx /= cnt;
    s_yy /= cnt;
    s_xy /= cnt;

    // --- Calcul de l’angle principal ---
    double theta = 0.5 * atan2(2.0 * s_xy, s_xx - s_yy);
    double angle_deg = theta * 180.0 / M_PI;

    // --- Amplification empirique ---
    double amplified_angle = angle_deg * 1.85; // 🔥 augmente légèrement la correction
    double rot = -amplified_angle;

    printf("[Info] Angle brut = %.2f°, amplifié = %.2f°, rotation = %.2f°\n",
           angle_deg, amplified_angle, rot);

    if (fabs(rot) < 0.15) {
        printf("[Info] Angle négligeable (%.3f°), pas de rotation.\n", rot);
        free(bin);
        return;
    }

    // --- Rotation de l'image ---
    unsigned char *newdata = malloc((size_t)W * H * img->channels);
    if (!newdata) {
        printf("[Erreur] Allocation (newdata)\n");
        free(bin);
        return;
    }

    double rad = rot * M_PI / 180.0;
    double cos_t = cos(rad), sin_t = sin(rad);
    double mcx = (W - 1) / 2.0, mcy = (H - 1) / 2.0;

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            double dx = x - mcx;
            double dy = y - mcy;
            double sx =  cos_t * dx + sin_t * dy + mcx;
            double sy = -sin_t * dx + cos_t * dy + mcy;
            int isx = (int)round(sx);
            int isy = (int)round(sy);
            for (int c = 0; c < img->channels; ++c) {
                unsigned char val = 255;
                if (isx >= 0 && isy >= 0 && isx < W && isy < H)
                    val = img->data[(isy * W + isx) * img->channels + c];
                newdata[(y * W + x) * img->channels + c] = val;
            }
        }
    }

    free(img->data);
    img->data = newdata;
    free(bin);

    printf("[OK] Image redressée (rotation %.2f°)\n", rot);
}

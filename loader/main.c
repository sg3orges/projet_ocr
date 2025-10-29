#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "loader.h"

int main(void)
{
    char image_name[128] = {0};

    // Read the filename to process
    FILE *f = fopen("last_image.txt", "r");
    if (!f)
    {
        printf("[Error] Cannot open last_image.txt\n");
        return 1;
    }

    if (!fgets(image_name, sizeof(image_name), f))
    {
        printf("[Error] No image name found.\n");
        fclose(f);
        return 1;
    }
    fclose(f);

    // Remove newline if present
    image_name[strcspn(image_name, "\n")] = '\0';

    // Construct full path
    char image_path[256];
    snprintf(image_path, sizeof(image_path), "images/%s", image_name);

    printf("[Info] Loading image: %s\n", image_path);

    // Load the image
    Image img = load_image(image_path);
    if (img.data == NULL)
    {
        printf("[Error] Failed to load image.\n");
        return 1;
    }

    if (img.width > 1600 || img.height > 1600)
        resize_image(&img, 1200, 900);

    // 🔹 Step 1: light contrast before deskew
    enhance_contrast_light(&img);

    // 🔹 Step 2: apply blur
    blur_image(&img);

    // 🔹 Step 3: deskew
    deskew_image(&img);

    // 🔹 Step 4: strong contrast after deskew
    enhance_contrast(&img);

    // 🔹 Step 5: final black & white conversion
    to_black_and_white(&img);

    // Save the result
    char output_path[256];
    snprintf(output_path, sizeof(output_path), "images/%s_bw.png", image_name);
    save_image(output_path, &img);

    // Delete original image
    if (remove(image_path) == 0)
        printf("[Info] Original image deleted: %s\n", image_path);
    else
        printf("[Warning] Cannot delete original image.\n");

    // Delete last_image.txt to avoid reprocessing
    if (remove("last_image.txt") == 0)
        printf("[Info] last_image.txt deleted.\n");

    // Free memory
    free_image(&img);

    printf("[OK] Processing completed successfully: %s\n", output_path);
    return 0;
}

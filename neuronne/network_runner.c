#include "networks.h"
#include <stdio.h>
#include <stdlib.h>
#include <err.h> // Indispensable pour la fonction errx()

void network_test(int argc, char *argv[]) {
    
    // Initialisation SDL pour charger les images (si utilisé dans predict/preprocess)
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        errx(1, "Erreur SDL: %s", SDL_GetError());

    NeuralNetwork net;
    init_network(&net);

    const char *save_file = "neuronne/brain.bin";
    
    // Tentative de chargement du cerveau
    if (!load_network(&net, save_file)) {
        printf(">>> No save found (%s). Training required...\n", save_file);
        
        // Make sure this folder exists!
        train_network(&net, "neuronne/dataset"); 
        
        save_network(&net, save_file);
    } else {
        printf(">>> Brain loaded from '%s'.\n", save_file);
    }

    // If an argument is given, test the image
    // argv[0] here is the argument passed by main (i.e., the image path)
    if (argc >= 1) {
        const char *user_image = argv[0]; 
        double confidence = 0.0;
        
        printf("\n--- IMAGE ANALYSIS ---\n");
        // Make sure predict is well-defined in networks.c
        // If predict is not defined, comment out these lines
        char result = predict(&net, user_image, &confidence);
        
        printf("File: %s\n", user_image);
        printf("Result: %c\n", result);
        printf("Confidence: %.2f%%\n", confidence * 100.0);
    } 
    else {
        printf("\n--- DEMO MODE ---\\n");
        printf("Usage via main: ./ocr neuron path/to/letter.png\n");
    }

    // SDL cleanup (Optional if the program exits right after)
    SDL_Quit();
}
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
        printf(">>> Aucune sauvegarde trouvée (%s). Entraînement nécessaire...\n", save_file);
        
        // Assurez-vous que ce dossier existe !
        train_network(&net, "neuronne/dataset"); 
        
        save_network(&net, save_file);
    } else {
        printf(">>> Cerveau chargé depuis '%s'.\n", save_file);
    }

    // Si un argument est donné, on teste l'image
    // argv[0] ici est l'argument passé par le main (donc le chemin de l'image)
    if (argc >= 1) {
        const char *user_image = argv[0]; 
        double confidence = 0.0;
        
        printf("\n--- ANALYSE DE L'IMAGE ---\n");
        // Assurez-vous que predict est bien défini dans networks.c
        // Si predict n'est pas défini, commentez ces lignes
        char result = predict(&net, user_image, &confidence);
        
        printf("Fichier : %s\n", user_image);
        printf("Résultat : %c\n", result);
        printf("Confiance : %.2f%%\n", confidence * 100.0);
    } 
    else {
        printf("\n--- MODE DÉMO ---\\n");
        printf("Usage via le main : ./ocr neuron chemin/vers/lettre.png\n");
    }

    // Nettoyage SDL (Optionnel si le programme quitte juste après)
    SDL_Quit();
}
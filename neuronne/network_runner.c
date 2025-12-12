#include "networks.h"

void network_test(int argc, char *argv[]) {
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) errx(1, "Erreur SDL: %s", SDL_GetError());

    NeuralNetwork net;
    init_network(&net);

	const char *save_file = "neuronne/brain.bin";
	
    if (!load_network(&net, save_file)) {
        printf(">>> Aucune sauvegarde trouvée. Entraînement nécessaire...\n");
        train_network(&net, "neuronne/dataset"); 
        save_network(&net, save_file);
    }

    if (argc > 1) {
        const char *user_image = argv[1]; 
        double confidence = 0.0;
        
        printf("\n--- ANALYSE DE L'IMAGE ---\n");
        char result = predict(&net, user_image, &confidence);
        
        printf("Fichier : %s\n", user_image);
        printf("Résultat : %c\n", result);
        printf("Confiance : %.2f%%\n", confidence);
    } 
    else {
        printf("\n--- MODE DÉMO ---\n");
        printf("Astuce : Pour tester ta propre image, lance :\n");
        printf("./ocr_ai chemin/vers/ton_image.png\n\n");
        
        const char *tests[] = {"neuronne/dataset/A1.png", "neuronne/dataset/B1.png", "neuronne/dataset/C1.png"};
        double conf = 0.0;
        
        for (int i = 0; i < 3; i++) {
            char l = predict(&net, tests[i], &conf);
            printf("Image : %s -> Lettre : %c (%.2f%%)\n", tests[i], l, conf);
        }
    }

    cleanup(&net);
    SDL_Quit();

}

#include "networks.h"

// --- Maths & Utilitaires ---

double random_weight() {
    return ((double)rand() / (double)RAND_MAX) - 0.5;
}

double sigmoid(double x) {
    return 1.0 / (1.0 + exp(-x));
}

double sigmoid_derivative(double x) {
    return x * (1.0 - x);
}

// --- Initialisation (Méthode Xavier) ---

void init_network(NeuralNetwork *net) {
    srand(time(NULL));

    net->hidden1_output = (double *)malloc(NUM_HIDDEN1 * sizeof(double));
    net->hidden2_output = (double *)malloc(NUM_HIDDEN2 * sizeof(double));
    net->final_output = (double *)malloc(NUM_OUTPUTS * sizeof(double));

    net->biases_h1 = (double *)calloc(NUM_HIDDEN1, sizeof(double));
    net->biases_h2 = (double *)calloc(NUM_HIDDEN2, sizeof(double));
    net->biases_o = (double *)calloc(NUM_OUTPUTS, sizeof(double));

    double scale1 = 1.0 / sqrt(NUM_INPUTS);
    net->weights_ih1 = (double **)malloc(NUM_INPUTS * sizeof(double *));
    for (int i = 0; i < NUM_INPUTS; i++) {
        net->weights_ih1[i] = (double *)malloc(NUM_HIDDEN1 * sizeof(double));
        for (int j = 0; j < NUM_HIDDEN1; j++) {
            net->weights_ih1[i][j] = random_weight() * scale1;
        }
    }

    double scale2 = 1.0 / sqrt(NUM_HIDDEN1);
    net->weights_h1h2 = (double **)malloc(NUM_HIDDEN1 * sizeof(double *));
    for (int i = 0; i < NUM_HIDDEN1; i++) {
        net->weights_h1h2[i] = (double *)malloc(NUM_HIDDEN2 * sizeof(double));
        for (int j = 0; j < NUM_HIDDEN2; j++) {
            net->weights_h1h2[i][j] = random_weight() * scale2;
        }
    }

    double scale3 = 1.0 / sqrt(NUM_HIDDEN2);
    net->weights_h2o = (double **)malloc(NUM_HIDDEN2 * sizeof(double *));
    for (int i = 0; i < NUM_HIDDEN2; i++) {
        net->weights_h2o[i] = (double *)malloc(NUM_OUTPUTS * sizeof(double));
        for (int j = 0; j < NUM_OUTPUTS; j++) {
            net->weights_h2o[i][j] = random_weight() * scale3;
        }
    }
}

// --- Traitement d'Image (Avec Debug Visuel) ---

void preprocess_image(const char *filepath, double *input_data) {
    SDL_Surface *surface = IMG_Load(filepath);
    if (!surface) {
        warnx("ERREUR CHARGEMENT : %s", filepath);
        for(int i=0; i < NUM_INPUTS; i++) input_data[i] = 0.0;
        return;
    }

    Uint32 rmask = 0x00FF0000;
    Uint32 gmask = 0x0000FF00;
    Uint32 bmask = 0x000000FF;
    Uint32 amask = 0xFF000000;

    SDL_Surface *dest = SDL_CreateRGBSurface(0, IMAGE_WIDTH, IMAGE_HEIGHT, 32, 
                                             rmask, gmask, bmask, amask);
    
    SDL_FillRect(dest, NULL, SDL_MapRGB(dest->format, 255, 255, 255));
    
    if (SDL_BlitScaled(surface, NULL, dest, NULL) != 0) {
        warnx("Erreur redimensionnement : %s", SDL_GetError());
    }

    SDL_LockSurface(dest);
    Uint32 *pixels = (Uint32 *)dest->pixels;
    int i = 0;
    

    for (int y = 0; y < IMAGE_HEIGHT; y++) {
        for (int x = 0; x < IMAGE_WIDTH; x++) {
            Uint32 pixel = pixels[y * dest->w + x];
            Uint8 r, g, b;
            SDL_GetRGB(pixel, dest->format, &r, &g, &b);
            
            double avg = (r + g + b) / 3.0;

            int isActive = (avg < 200); 
            
            input_data[i++] = isActive ? 1.0 : 0.0;

        }
    }

    SDL_UnlockSurface(dest);
    SDL_FreeSurface(surface);
    SDL_FreeSurface(dest);
}

// --- Chargement Dataset ---

void load_dataset(double inputs[NUM_TRAINING_SETS][NUM_INPUTS],
                  double outputs[NUM_TRAINING_SETS][NUM_OUTPUTS],
                  const char *path) {
    char filepath[1024];
    int idx = 0;
    
    printf("Chargement des images (A1.png -> Z1.png)...\n");

    for (char c = 'A'; c <= 'Z'; c++) {
        sprintf(filepath, "%s/%c1.png", path, c);
        
        if (idx >= NUM_TRAINING_SETS) break;

        preprocess_image(filepath, inputs[idx]);
        
        for (int i = 0; i < NUM_OUTPUTS; i++) outputs[idx][i] = 0.0;
        outputs[idx][c - 'A'] = 1.0;
        
        idx++;
    }
    printf("Chargement terminé (%d images).\n", idx);
}

// --- Coeur du Réseau (Forward & Backward) ---

void forward_pass(NeuralNetwork *net, double *inputs) {
    for (int j = 0; j < NUM_HIDDEN1; j++) {
        double sum = net->biases_h1[j];
        for (int k = 0; k < NUM_INPUTS; k++) 
            sum += inputs[k] * net->weights_ih1[k][j];
        net->hidden1_output[j] = sigmoid(sum);
    }
    for (int j = 0; j < NUM_HIDDEN2; j++) {
        double sum = net->biases_h2[j];
        for (int k = 0; k < NUM_HIDDEN1; k++) 
            sum += net->hidden1_output[k] * net->weights_h1h2[k][j];
        net->hidden2_output[j] = sigmoid(sum);
    }
    for (int j = 0; j < NUM_OUTPUTS; j++) {
        double sum = net->biases_o[j];
        for (int k = 0; k < NUM_HIDDEN2; k++) 
            sum += net->hidden2_output[k] * net->weights_h2o[k][j];
        net->final_output[j] = sigmoid(sum);
    }
}

double backward_pass(NeuralNetwork *net, double *inputs, double *targets) {
    double out_deltas[NUM_OUTPUTS];
    double h2_deltas[NUM_HIDDEN2];
    double h1_deltas[NUM_HIDDEN1];
    double loss = 0.0;

    for (int j = 0; j < NUM_OUTPUTS; j++) {
        double err = targets[j] - net->final_output[j];
        loss += 0.5 * err * err;
        out_deltas[j] = err * sigmoid_derivative(net->final_output[j]);
    }

    for (int j = 0; j < NUM_HIDDEN2; j++) {
        double err = 0.0;
        for (int k = 0; k < NUM_OUTPUTS; k++) err += out_deltas[k] * net->weights_h2o[j][k];
        h2_deltas[j] = err * sigmoid_derivative(net->hidden2_output[j]);
    }

    for (int j = 0; j < NUM_HIDDEN1; j++) {
        double err = 0.0;
        for (int k = 0; k < NUM_HIDDEN2; k++) err += h2_deltas[k] * net->weights_h1h2[j][k];
        h1_deltas[j] = err * sigmoid_derivative(net->hidden1_output[j]);
    }

    for (int k = 0; k < NUM_OUTPUTS; k++) {
        net->biases_o[k] += LEARNING_RATE * out_deltas[k];
        for (int j = 0; j < NUM_HIDDEN2; j++) 
            net->weights_h2o[j][k] += LEARNING_RATE * out_deltas[k] * net->hidden2_output[j];
    }
    for (int k = 0; k < NUM_HIDDEN2; k++) {
        net->biases_h2[k] += LEARNING_RATE * h2_deltas[k];
        for (int j = 0; j < NUM_HIDDEN1; j++) 
            net->weights_h1h2[j][k] += LEARNING_RATE * h2_deltas[k] * net->hidden1_output[j];
    }
    for (int k = 0; k < NUM_HIDDEN1; k++) {
        net->biases_h1[k] += LEARNING_RATE * h1_deltas[k];
        for (int j = 0; j < NUM_INPUTS; j++) 
            net->weights_ih1[j][k] += LEARNING_RATE * h1_deltas[k] * inputs[j];
    }
    return loss;
}

// --- Prédiction ---

char predict(NeuralNetwork *net, const char *filepath, double *confidence) {
    double input[NUM_INPUTS];
    preprocess_image(filepath, input); 
    forward_pass(net, input);

    int max_idx = 0;
    double max_val = net->final_output[0];

    for (int i = 1; i < NUM_OUTPUTS; i++) {
        if (net->final_output[i] > max_val) {
            max_val = net->final_output[i];
            max_idx = i;
        }
    }
    
    *confidence = max_val * 100.0; 
    return (char)('A' + max_idx);
}

// --- Entraînement ---

void shuffle(int *array, size_t n) {
    if (n > 1) {
        for (size_t i = n - 1; i > 0; i--) {
            size_t j = rand() % (i + 1);
            int t = array[j]; array[j] = array[i]; array[i] = t;
        }
    }
}

void print_bar(int epoch, int current, int total, double loss) {
    int width = 30;
    float prog = (float)(current + 1) / total;
    int pos = width * prog;
    
    printf("\rEpoch %3d | [", epoch + 1);
    for (int i = 0; i < width; ++i) printf(i < pos ? "=" : (i == pos ? ">" : " "));
    printf("] Loss: %.5f", loss);
    if (current + 1 == total) printf("\n");
    fflush(stdout);
}

void train_network(NeuralNetwork *net, const char *path) {
    double (*inputs)[NUM_INPUTS] = malloc(NUM_TRAINING_SETS * sizeof(*inputs));
    double (*targets)[NUM_OUTPUTS] = malloc(NUM_TRAINING_SETS * sizeof(*targets));
    
    if (!inputs || !targets) errx(1, "Erreur Mémoire Dataset");

    load_dataset(inputs, targets, path);

    int indices[NUM_TRAINING_SETS];
    for (int i = 0; i < NUM_TRAINING_SETS; i++) indices[i] = i;

    for (int ep = 0; ep < NUM_EPOCHS; ep++) {
        shuffle(indices, NUM_TRAINING_SETS);
        double avg_loss = 0.0;
        
        for (int i = 0; i < NUM_TRAINING_SETS; i++) {
            int idx = indices[i];
            forward_pass(net, inputs[idx]);
            avg_loss += backward_pass(net, inputs[idx], targets[idx]);
            print_bar(ep, i, NUM_TRAINING_SETS, avg_loss / (i+1));
        }
    }

    free(inputs);
    free(targets);
}

// --- Sauvegarde / Chargement ---

void save_network(NeuralNetwork *net, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;
    
    fwrite(net->biases_h1, sizeof(double), NUM_HIDDEN1, f);
    fwrite(net->biases_h2, sizeof(double), NUM_HIDDEN2, f);
    fwrite(net->biases_o, sizeof(double), NUM_OUTPUTS, f);
    
    for(int i=0; i<NUM_INPUTS; i++) fwrite(net->weights_ih1[i], sizeof(double), NUM_HIDDEN1, f);
    for(int i=0; i<NUM_HIDDEN1; i++) fwrite(net->weights_h1h2[i], sizeof(double), NUM_HIDDEN2, f);
    for(int i=0; i<NUM_HIDDEN2; i++) fwrite(net->weights_h2o[i], sizeof(double), NUM_OUTPUTS, f);
    
    fclose(f);
    printf("Réseau sauvegardé dans '%s'.\n", filename);
}

int load_network(NeuralNetwork *net, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return 0; 

    fread(net->biases_h1, sizeof(double), NUM_HIDDEN1, f);
    fread(net->biases_h2, sizeof(double), NUM_HIDDEN2, f);
    fread(net->biases_o, sizeof(double), NUM_OUTPUTS, f);
    
    for(int i=0; i<NUM_INPUTS; i++) fread(net->weights_ih1[i], sizeof(double), NUM_HIDDEN1, f);
    for(int i=0; i<NUM_HIDDEN1; i++) fread(net->weights_h1h2[i], sizeof(double), NUM_HIDDEN2, f);
    for(int i=0; i<NUM_HIDDEN2; i++) fread(net->weights_h2o[i], sizeof(double), NUM_OUTPUTS, f);
    
    fclose(f);
    printf("Réseau chargé depuis '%s'.\n", filename);
    return 1; 
}

void cleanup(NeuralNetwork *net) {
    free(net->hidden1_output); free(net->hidden2_output); free(net->final_output);
    free(net->biases_h1); free(net->biases_h2); free(net->biases_o);
    for (int i=0; i<NUM_INPUTS; i++) free(net->weights_ih1[i]); free(net->weights_ih1);
    for (int i=0; i<NUM_HIDDEN1; i++) free(net->weights_h1h2[i]); free(net->weights_h1h2);
    for (int i=0; i<NUM_HIDDEN2; i++) free(net->weights_h2o[i]); free(net->weights_h2o);
}

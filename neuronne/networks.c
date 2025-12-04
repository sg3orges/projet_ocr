#include "networks.h"

// Maths

double random_weight() {
    return ((double)rand() / (double)RAND_MAX) - 0.5;
}

double sigmoid(double x) {
    return 1.0 / (1.0 + exp(-x));
}

double sigmoid_derivative(double x) {
    return x * (1.0 - x);
}

void softmax(double *input, int n) {
    double max = input[0];
    for (int i = 1; i < n; i++) {
        if (input[i] > max) max = input[i];
    }

    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        input[i] = exp(input[i] - max); 
        sum += input[i];
    }

    for (int i = 0; i < n; i++) {
        input[i] /= sum;
    }
}

// Init

void init_network(NeuralNetwork *net) {
    srand(time(NULL));

    net->hidden_output = (double *)malloc(NUM_HIDDEN * sizeof(double));
    net->final_output = (double *)malloc(NUM_OUTPUTS * sizeof(double));

    net->biases_h = (double *)calloc(NUM_HIDDEN, sizeof(double));
    net->biases_o = (double *)calloc(NUM_OUTPUTS, sizeof(double));

    // Input -> Hidden
    double scale1 = 1.0 / sqrt(NUM_INPUTS);
    net->weights_ih = (double **)malloc(NUM_INPUTS * sizeof(double *));
    for (int i = 0; i < NUM_INPUTS; i++) {
        net->weights_ih[i] = (double *)malloc(NUM_HIDDEN * sizeof(double));
        for (int j = 0; j < NUM_HIDDEN; j++) {
            net->weights_ih[i][j] = random_weight() * scale1;
        }
    }

    // Hidden -> Output
    double scale2 = 1.0 / sqrt(NUM_HIDDEN);
    net->weights_ho = (double **)malloc(NUM_HIDDEN * sizeof(double *));
    for (int i = 0; i < NUM_HIDDEN; i++) {
        net->weights_ho[i] = (double *)malloc(NUM_OUTPUTS * sizeof(double));
        for (int j = 0; j < NUM_OUTPUTS; j++) {
            net->weights_ho[i][j] = random_weight() * scale2;
        }
    }
}

// traitment of picture
void preprocess_image(const char *filepath, double *input_data) {
    SDL_Surface *surface = IMG_Load(filepath);
    if (!surface) {
        warnx("ERREUR CHARGEMENT : %s", filepath);
        for(int i=0; i < NUM_INPUTS; i++) input_data[i] = 0.0;
        return;
    }

    // Standardisation format
    SDL_Surface *dest = SDL_CreateRGBSurface(0, IMAGE_WIDTH, IMAGE_HEIGHT, 32, 
                                             0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    
    SDL_FillRect(dest, NULL, SDL_MapRGB(dest->format, 255, 255, 255));
    SDL_BlitScaled(surface, NULL, dest, NULL);

    SDL_LockSurface(dest);
    Uint32 *pixels = (Uint32 *)dest->pixels;
    int i = 0;

    for (int y = 0; y < IMAGE_HEIGHT; y++) {
        for (int x = 0; x < IMAGE_WIDTH; x++) {
            Uint32 pixel = pixels[y * dest->w + x];
            Uint8 r, g, b;
            SDL_GetRGB(pixel, dest->format, &r, &g, &b);
            double avg = (r + g + b) / 3.0;
            input_data[i++] = (avg < 200) ? 1.0 : 0.0; 
        }
    }

    SDL_UnlockSurface(dest);
    SDL_FreeSurface(surface);
    SDL_FreeSurface(dest);
}

// Loading dataset
void load_dataset(double inputs[NUM_TRAINING_SETS][NUM_INPUTS],
                  double outputs[NUM_TRAINING_SETS][NUM_OUTPUTS],
                  const char *path) {
    char filepath[1024];
    int idx = 0;
    
    printf("Chargement des images...\n");
    for (int version = 1; version <= 2; version++) {
        for (char c = 'A'; c <= 'Z'; c++) {
            if (idx >= NUM_TRAINING_SETS) break;
            sprintf(filepath, "%s/%c%d.png", path, c, version);
            
            preprocess_image(filepath, inputs[idx]);
            
            for (int i = 0; i < NUM_OUTPUTS; i++) outputs[idx][i] = 0.0;
            outputs[idx][c - 'A'] = 1.0;
            idx++;
        }
    }
    printf("Dataset chargé : %d images.\n", idx);
}

// neuron

void forward_pass(NeuralNetwork *net, double *inputs) {
    // 1. Input -> Hidden (Sigmoid)
    for (int j = 0; j < NUM_HIDDEN; j++) {
        double sum = net->biases_h[j];
        for (int k = 0; k < NUM_INPUTS; k++) 
            sum += inputs[k] * net->weights_ih[k][j];
        net->hidden_output[j] = sigmoid(sum);
    }

    // 2. Hidden -> Output (Raw Logits -> Softmax)
    for (int j = 0; j < NUM_OUTPUTS; j++) {
        double sum = net->biases_o[j];
        for (int k = 0; k < NUM_HIDDEN; k++) 
            sum += net->hidden_output[k] * net->weights_ho[k][j];
        net->final_output[j] = sum; 
    }

    softmax(net->final_output, NUM_OUTPUTS);
}

double backward_pass(NeuralNetwork *net, double *inputs, double *targets) {
    double out_deltas[NUM_OUTPUTS];
    double hidden_deltas[NUM_HIDDEN];
    double loss = 0.0;

    for (int j = 0; j < NUM_OUTPUTS; j++) {
        double output = net->final_output[j];
        if (targets[j] == 1.0) {
            loss -= log(output + 1e-15); 
        }

        out_deltas[j] = (targets[j] - output); 
    }

    for (int j = 0; j < NUM_HIDDEN; j++) {
        double err = 0.0;
        for (int k = 0; k < NUM_OUTPUTS; k++) {
            err += out_deltas[k] * net->weights_ho[j][k];
        }
        hidden_deltas[j] = err * sigmoid_derivative(net->hidden_output[j]);
    }

    // update weight Hidden -> Output
    for (int k = 0; k < NUM_OUTPUTS; k++) {
        net->biases_o[k] += LEARNING_RATE * out_deltas[k];
        for (int j = 0; j < NUM_HIDDEN; j++) {
            net->weights_ho[j][k] += LEARNING_RATE * out_deltas[k] * net->hidden_output[j];
        }
    }

    // update weight Input -> Hidden
    for (int k = 0; k < NUM_HIDDEN; k++) {
        net->biases_h[k] += LEARNING_RATE * hidden_deltas[k];
        for (int j = 0; j < NUM_INPUTS; j++) {
            net->weights_ih[j][k] += LEARNING_RATE * hidden_deltas[k] * inputs[j];
        }
    }

    return loss;
}

// Prediction

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

// Save

void save_network(NeuralNetwork *net, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        warn("Impossible d'ouvrir le fichier de sauvegarde %s", filename);
        return;
    }
    
    fwrite(net->biases_h, sizeof(double), NUM_HIDDEN, f);
    fwrite(net->biases_o, sizeof(double), NUM_OUTPUTS, f);
    
    for(int i = 0; i < NUM_INPUTS; i++) {
        fwrite(net->weights_ih[i], sizeof(double), NUM_HIDDEN, f);
    }

    for(int i = 0; i < NUM_HIDDEN; i++) {
        fwrite(net->weights_ho[i], sizeof(double), NUM_OUTPUTS, f);
    }
    
    fclose(f);
    printf("Réseau sauvegardé dans '%s'.\n", filename);
}

int load_network(NeuralNetwork *net, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return 0; 

    size_t res = 0;

    res += fread(net->biases_h, sizeof(double), NUM_HIDDEN, f);
    res += fread(net->biases_o, sizeof(double), NUM_OUTPUTS, f);
    
    for(int i = 0; i < NUM_INPUTS; i++) {
        res += fread(net->weights_ih[i], sizeof(double), NUM_HIDDEN, f);
    }

    for(int i = 0; i < NUM_HIDDEN; i++) {
        res += fread(net->weights_ho[i], sizeof(double), NUM_OUTPUTS, f);
    }
    
    fclose(f);

    if (res == 0) {
        printf("Erreur: Le fichier de sauvegarde semble corrompu ou vide.\n");
        return 0;
    }

    printf("Réseau chargé depuis '%s'.\n", filename);
    return 1; 
}


void cleanup(NeuralNetwork *net) {
    free(net->hidden_output); 
    free(net->final_output);
    free(net->biases_h); 
    free(net->biases_o);
	
    for (int i = 0; i < NUM_INPUTS; i++) {
        free(net->weights_ih[i]);
    }
    free(net->weights_ih);

    for (int i = 0; i < NUM_HIDDEN; i++) {
        free(net->weights_ho[i]);
    }
    free(net->weights_ho);
}



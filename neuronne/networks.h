#ifndef NETWORKS_H
#define NETWORKS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <err.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <time.h>

// --- Paramètres ---
#define NUM_EPOCHS 750       
#define LEARNING_RATE 0.3   
#define IMAGE_WIDTH 48    
#define IMAGE_HEIGHT 48

// --- Architecture ---
#define NUM_INPUTS (IMAGE_WIDTH * IMAGE_HEIGHT)
#define NUM_HIDDEN1 64
#define NUM_HIDDEN2 32
#define NUM_OUTPUTS 26
#define NUM_TRAINING_SETS 26*2  // 1 image par lettre pour l'instant

// --- Structure ---
typedef struct {
    double **weights_ih1;
    double *biases_h1;
    double **weights_h1h2;
    double *biases_h2;
    double **weights_h2o;
    double *biases_o;
    double *hidden1_output;
    double *hidden2_output;
    double *final_output;
} NeuralNetwork;

// --- Fonctions ---
void init_network(NeuralNetwork *net);
void load_dataset(double training_inputs[NUM_TRAINING_SETS][NUM_INPUTS],
                  double training_outputs[NUM_TRAINING_SETS][NUM_OUTPUTS],
                  const char *dataset_path);
void preprocess_image(const char *filepath, double *input_data);

void train_network(NeuralNetwork *net, const char *dataset_path);

// stocker le pourcentage
char predict(NeuralNetwork *net, const char *filepath, double *confidence);

void save_network(NeuralNetwork *net, const char *filename);
int load_network(NeuralNetwork *net, const char *filename);

void cleanup(NeuralNetwork *net);
double sigmoid(double x);
double sigmoid_derivative(double x);
double random_weight();
void network_test();
#endif

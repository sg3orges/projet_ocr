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

// settings
#define NUM_EPOCHS 2000     
#define LEARNING_RATE 0.1
#define IMAGE_WIDTH 48      
#define IMAGE_HEIGHT 48

// archi
#define NUM_INPUTS (IMAGE_WIDTH * IMAGE_HEIGHT)
#define NUM_HIDDEN 64
#define NUM_OUTPUTS 26
#define NUM_TRAINING_SETS 26*2

// struct
typedef struct {
    double **weights_ih;
    double *biases_h;
    
    double **weights_ho;
    double *biases_o;

    double *hidden_output;
    double *final_output;
} NeuralNetwork;

// fonction
void init_network(NeuralNetwork *net);
void load_dataset(double training_inputs[NUM_TRAINING_SETS][NUM_INPUTS],
                  double training_outputs[NUM_TRAINING_SETS][NUM_OUTPUTS],
                  const char *dataset_path);
void preprocess_image(const char *filepath, double *input_data);

void train_network(NeuralNetwork *net, const char *dataset_path);
char predict(NeuralNetwork *net, const char *filepath, double *confidence);

void save_network(NeuralNetwork *net, const char *filename);
int load_network(NeuralNetwork *net, const char *filename);

void cleanup(NeuralNetwork *net);
void network_test(int argc, char *argv[]);

#endif

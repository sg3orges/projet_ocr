#ifndef SOLVER_H
#define SOLVER_H

#include <stdio.h>
#include <string.h>

#define SOLVER_MAX 100
#define TAILLE_MAX 100

int CreaMatrice(const char *Fichier, char matrice[SOLVER_MAX][SOLVER_MAX]);

int ChercheMot (const char *mot, char matrice[SOLVER_MAX][SOLVER_MAX],
                int nbLignes , int nbColonnes,
                int *ligneDebut , int *colDebut,
                int *ligneFin, int *colFin);

void ConvertirMajuscules(char *mot);

void solver_test(void);
void solver_run_words(const char *grid_file, const char *words_file);
#endif 

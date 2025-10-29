#ifndef SOLVER_H
#define SOLVER_H

#include <stdio.h>
#include <string.h>

#define MAX 100
#define TAILLE_MAX 100

int CreaMatrice(const char *Fichier, char matrice[MAX][MAX]);

int ChercheMot (const char *mot, char matrice[MAX][MAX],
                int nbLignes , int nbColonnes,
                int *ligneDebut , int *colDebut,
                int *ligneFin, int *colFin);

void ConvertirMajuscules(char *mot);

#endif 
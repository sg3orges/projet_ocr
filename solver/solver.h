#ifndef SOLVER_H
#define SOLVER_H

#include <stdio.h>
#include <string.h>

// Renommage de MAX en MAX_MAT pour éviter les conflits avec glib/gtk
#define MAX_MAT 100
#define TAILLE_MAX 100

int CreaMatrice(const char *Fichier, char matrice[MAX_MAT][MAX_MAT]);

int ChercheMot (const char *mot, char matrice[MAX_MAT][MAX_MAT],
                int nbLignes , int nbColonnes,
                int *ligneDebut , int *colDebut,
                int *ligneFin, int *colFin);

void ConvertirMajuscules(char *mot);

// Correction : ajout des arguments argc/argv
void solver_test(int argc, char *argv[]); 

#endif
#include <stdio.h>
#include <string.h>
#include "solver.h" // Always include the corresponding header

// MAX_MAT is defined in solver.h

int CreaMatrice(const char *Fichier , char matrice[MAX_MAT][MAX_MAT])
{
    FILE *f = fopen(Fichier, "r");
    if(f == NULL)
    {
        printf("Error: Cannot open file %s\n", Fichier);
        return 0;
    } 
    int ligne = 0;
    char ligneM[MAX_MAT];

    while (fgets(ligneM, sizeof(ligneM), f) && ligne < MAX_MAT)
    {
        ligneM[strcspn(ligneM,"\n")] = '\0'; 
        if(strlen(ligneM)==0) continue;
        strcpy(matrice[ligne], ligneM); 
        ligne++;
    }
    
    fclose(f);
    return ligne;
}

int ChercheMot(const char *mot, char matrice[MAX_MAT][MAX_MAT], int nbLignes, int nbColonnes,
               int *ligneDebut, int *colDebut, int *ligneFin, int *colFin)
{
    int len = strlen(mot);
    // 8 directions
    int directions[8][2] = {
        {0, 1}, {0, -1}, {1, 0}, {-1, 0}, 
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    for (int i = 0; i < nbLignes; i++) {
        for (int j = 0; j < nbColonnes; j++) {
            if (matrice[i][j] != mot[0]) continue;

            for (int d = 0; d < 8; d++) {
                int k, x = i, y = j;
                for (k = 1; k < len; k++) {
                    x += directions[d][0];
                    y += directions[d][1];

                    if (x < 0 || x >= nbLignes || y < 0 || y >= nbColonnes) break;
                    if (matrice[x][y] != mot[k]) break;
                }
                if (k == len) {
                    *ligneDebut = i; *colDebut = j;
                    *ligneFin = x; *colFin = y;
                    return 1;
                }
            }
        }
    }
    return 0;
}

void ConvertirMajuscules(char *mot)
{
    for (int i = 0; mot[i] != '\0'; i++) {
        if (mot[i] >= 'a' && mot[i] <= 'z') {
            mot[i] = mot[i] - 32;
        }
    }
}

void solver_test(int argc, char *argv[]) 
{
    // Le main passera (argc-1) et &argv[1].
    // Donc argv[0] sera "solver", argv[1] le fichier, argv[2] le mot.
    if (argc < 3)
    {
        printf("Usage: ./ocr solver <grid.txt> <word>\n");
        return;
    }   

    char matrice[MAX_MAT][MAX_MAT];
    int nbLignes = CreaMatrice(argv[1], matrice);
    if (nbLignes == 0) return;

    int nbColonnes = strlen(matrice[0]); 
    
    char mot_a_chercher[100];
    strncpy(mot_a_chercher, argv[2], 99);
    mot_a_chercher[99] = '\0';

    ConvertirMajuscules(mot_a_chercher);

    int dL, dC, fL, fC;
    if(ChercheMot(mot_a_chercher, matrice, nbLignes, nbColonnes, &dL, &dC, &fL, &fC))
    {
        printf("Word FOUND from (%d,%d) to (%d,%d)\n", dL, dC, fL, fC);
    }
    else {
        printf("Word NOT found.\n");
    }
}
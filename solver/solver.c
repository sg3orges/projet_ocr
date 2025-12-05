#include <stdio.h>
#include <string.h>
#include "solver.h"

int CreaMatrice(const char *Fichier , char matrice[SOLVER_MAX][SOLVER_MAX])
{
    FILE *f = fopen(Fichier, "r");
    if(f == NULL)
    {
        printf("impossible d ouvrire le fichier");
        return 0;
    } 
    int ligne = 0;
    char ligneM[SOLVER_MAX];

    while (fgets(ligneM, sizeof(ligneM), f))//select line  in document f 
    {
        ligneM[strcspn(ligneM,"\n")] = '\0'; //supp \n in the line 
        if(strlen(ligneM)==0)
        {
            continue;
        }
        strcpy(matrice[ligne], ligneM); //copy the line in mat 
        ligne++;
    }
    
    fclose(f);

    return ligne;

}

int ChercheMot(const char *mot, char matrice[SOLVER_MAX][SOLVER_MAX], int nbLignes, int nbColonnes,
               int *ligneDebut, int *colDebut, int *ligneFin, int *colFin)
{
    int len = strlen(mot);

    
    int directions[8][2] = {
        {0, 1},   
        {0, -1},  
        {1, 0},   
        {-1, 0},  
        {1, 1},   
        {1, -1},  
        {-1, 1},  
        {-1, -1}  
    }; // all direction in mat

    for (int i = 0; i < nbLignes; i++)
    {
        for (int j = 0; j < nbColonnes; j++)
        {
            if (matrice[i][j] == mot[0])
            {
                for (int d = 0; d < 8; d++)
                {
                    int dx = directions[d][0];
                    int dy = directions[d][1];
                    int x = i, y = j;
                    int k;

                    for (k = 1; k < len; k++)
                    {
                        x += dx;
                        y += dy;

                        if (x < 0 || x >= nbLignes || y < 0 || y >= nbColonnes)
                            break;

                        if (matrice[x][y] != mot[k])
                            break;
                    }

                    if (k == len)
                    {
                        *ligneDebut = i;
                        *colDebut = j;
                        *ligneFin = x;
                        *colFin = y;
                        return 1;
                    }
                }
            }
        }
    }

    return 0; // word not found
}

void ConvertirMajuscules(char *mot)//toUP
{
    for (int i = 0; mot[i] != '\0'; i++)
    {
        if (mot[i] >= 'a' && mot[i] <= 'z')
        {
            mot[i] = mot[i] - 32;
        }
    }
}


// Parcourt toutes les lignes de words_file et affiche les coordonnées pour chaque mot
void solver_run_words(const char *grid_file, const char *words_file)
{
    char matrice[SOLVER_MAX][SOLVER_MAX];
    int nbLignes = CreaMatrice(grid_file, matrice);
    if (nbLignes <= 0) {
        printf("[solver] Grille vide ou introuvable (%s)\n", grid_file);
        return;
    }
    int nbColonnes = strlen(matrice[0]);

    FILE *f = fopen(words_file, "r");
    if (!f) {
        printf("[solver] Impossible d'ouvrir %s\n", words_file);
        return;
    }

    char ligne[SOLVER_MAX];
    while (fgets(ligne, sizeof(ligne), f)) {
        ligne[strcspn(ligne, "\n")] = '\0';
        if (ligne[0] == '\0')
            continue;
        ConvertirMajuscules(ligne);

        int li1 = -1, li2 = -1, co1 = -1, co2 = -1;
        if (ChercheMot(ligne, matrice, nbLignes, nbColonnes, &li1, &co1, &li2, &co2)) {
            printf("%s: (%d,%d)(%d,%d)\n", ligne, co1, li1, co2, li2);
        } else {
            printf("%s: Not found\n", ligne);
        }
    }
    fclose(f);
}

// Compatibilité: version sans arguments, lance sur GRID / GRID_Word
void solver_test(void)
{
    solver_run_words("GRID", "GRID_Word");
}

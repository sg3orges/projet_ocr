#include <stdio.h>
#include <string.h>

#define MAX 100 

int CreaMatrice(const char *Fichier , char matrice[100][100])
{
    FILE *f = fopen(Fichier, "r");
    if(f == NULL)
    {
        printf("impossible d ouvrire le fichier");
        return 0;
    } 
    int ligne = 0;
    char ligneM[MAX];

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

int ChercheMot(const char *mot, char matrice[MAX][MAX], int nbLignes, int nbColonnes,
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


int main(int argc, char *argv[]) // this part call all fonction of the solver and print the final result
{
    if (argc != 3)
    {
        printf("il manque des argument");
        return 1;
    }   

    char matrice[MAX][MAX];
    int nbLignes = CreaMatrice(argv[1], matrice);
    int nbColonnes = strlen(matrice[0]); 
    ConvertirMajuscules(argv[2]);

    int li1 = -1;
    int li2 = -1;
    int co1 = -1;
    int co2 = -1;

    if (ChercheMot(argv[2], matrice, nbLignes, nbColonnes, &li1, &co1, &li2, &co2))
    {
       
        printf("(%d,%d)(%d,%d)\n", co1, li1,co2 ,li2);//print the position
    
    }
    else
    {
        printf("Not found\n");
    }

    return 0;
}
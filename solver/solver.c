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

    while (fgets(ligneM, sizeof(ligneM), f))
    {
        ligneM[strcspn(ligneM,"\n")] = '\0'; //supp \n
        if(strlen(ligneM)==0)
        {
            continue;
        }
        strcpy(matrice[ligne], ligneM);
        ligne++;
    }
    
    fclose(f);

    return ligne;

}

int main()
{
    char matrice[MAX][MAX] = {0};
    const char *fichier = "GRID.txt";
    int Mat = CreaMatrice(fichier, matrice);

    if (Mat == 0)
    {
        printf("erreur");
    }

    for(int i = 0; i<Mat; i++)
    {
        printf("%s\n",matrice[i]);
    }

    return 0;
    

}
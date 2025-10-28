#include <stdio.h>
#include <math.h>
#include <unistd.h>

double sigmoid(double x)
{
    return 1.0 / (1.0 + exp(-x));
}

double sigmoid_derivative(double x)
{
    return x * (1.0 - x);
}

void clear_screen() 
{
    printf("\033[H\033[J");
}

int main() 
{
    double input[4][2] = {{0,0}, {0,1}, {1,0}, {1,1}};
    double output[4] = {0, 1, 1, 0};

    // PARAMÈTRES 
    double learning_rate = 0.8;
    int reps = 5000;

    double w_hidden[2][2] = {{0.15, -0.25},{-0.20, 0.30}};
    double b_hidden[2] = {-0.35, 0.45};
    
    double w_output[2] = {0.20, -0.30};
    double b_output = 0.60;

    for (int rep = 0; rep < reps; rep++) 
    {
        double total_error = 0.0;
        
        for (int i = 0; i < 4; i++) 
        {
            double x1 = input[i][0];
            double x2 = input[i][1];
            double target = output[i];
            
            double hidden[2];
            for (int j = 0; j < 2; j++)
            {
                double z = x1 * w_hidden[j][0] + x2 * w_hidden[j][1] + b_hidden[j];
                hidden[j] = sigmoid(z);
            }
            double y_pred = sigmoid(hidden[0]*w_output[0] + hidden[1]*w_output[1] + b_output);
            
            double error = target - y_pred;
            total_error += error * error;
            
            double d_output = error * sigmoid_derivative(y_pred);
            double d_hidden[2];
            for (int j = 0; j < 2; j++)
            {
                d_hidden[j] = d_output * w_output[j] * sigmoid_derivative(hidden[j]);
            }
            
            for (int j = 0; j < 2; j++)
            {
                w_output[j] += learning_rate * d_output * hidden[j];
                w_hidden[j][0] += learning_rate * d_hidden[j] * x1;
                w_hidden[j][1] += learning_rate * d_hidden[j] * x2;
                b_hidden[j] += learning_rate * d_hidden[j];
            }
            b_output += learning_rate * d_output;
        }

        if (rep % 25 == 0)
        {
            clear_screen();
                   
            printf("Rep: %d/%d\n", rep, reps);
            printf("Erreur totale: %.6f\n\n", total_error);
            printf("PRÉDICTIONS ACTUELLES:\n");
            printf("┌───────┬─────────┬──────────┬────────┐\n");
            printf("│ Input │ Attendu │ Prédict° │ Statut │\n");
            printf("├───────┼─────────┼──────────┼────────┤\n");
            
            for (int i = 0; i < 4; i++)
            {
                double x1 = input[i][0];
                double x2 = input[i][1];
                double hidden[2];
                for (int j = 0; j < 2; j++)
                {
                    double z = x1 * w_hidden[j][0] + x2 * w_hidden[j][1] + b_hidden[j];
                    hidden[j] = sigmoid(z);
                }
                double y_pred = sigmoid(hidden[0]*w_output[0] + hidden[1]*w_output[1] + b_output);
                
                char* status = "❌";
                if ((y_pred < 0.5 && output[i] == 0) || (y_pred >= 0.5 && output[i] == 1))
                {
                    status = "✅";
                }
                
                printf("│ [%.0f,%.0f] │    %.0f    │   %.3f  │   %s   │\n", 
                       x1, x2, output[i], y_pred, status);
            }
            printf("└───────┴─────────┴──────────┴────────┘\n\n");
            
            int progress = (rep * 40) / reps;
            printf("Progression: [");
            for (int p = 0; p < 40; p++)
            {
                if (p < progress) printf("█");
                else printf("░");
            }
            printf("] %d%%\n", (rep * 100) / reps);
            usleep(100000);
        }
    }

    clear_screen();
    int correct = 0;
    printf("RÉSULTATS FINAUX:\n");
    printf("┌───────┬─────────┬──────────┬────────┐\n");
    printf("│ Input │ Attendu │ Prédict° │ Statut │\n");
    printf("├───────┼─────────┼──────────┼────────┤\n");
    
    for (int i = 0; i < 4; i++)
    {
        double x1 = input[i][0], x2 = input[i][1];
        double hidden[2];
        for (int j = 0; j < 2; j++)
        {
            double z = x1 * w_hidden[j][0] + x2 * w_hidden[j][1] + b_hidden[j];
            hidden[j] = sigmoid(z);
        }
        double y_pred = sigmoid(hidden[0]*w_output[0] + hidden[1]*w_output[1] + b_output);
        
        char* status = "❌";
        if ((y_pred < 0.5 && output[i] == 0) || (y_pred >= 0.5 && output[i] == 1))
        {
            status = "✅";
            correct++;
        }
        
        printf("│ [%.0f,%.0f] │    %.0f    │   %.3f  │   %s   │\n", 
               x1, x2, output[i], y_pred, status);
    }
    printf("└───────┴─────────┴──────────┴────────┘\n\n");   
    printf("%d/4\n", correct);
    return 0;
}

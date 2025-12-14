#include <string.h>
#include <stdio.h> // Ajout nécessaire
#include "rotations/gui.h"
#include "interface/gui.h"
#include "detectionV2/detection.h"
#include "solver/solver.h"
#include "neuronne/networks.h"

int main(int argc, char **argv)
{
    if (argc > 1) {
        if (strcmp(argv[1], "detect") == 0 && argc > 2) {
            return detection_run_app(2, &argv[1]);
        }
        if (strcmp(argv[1], "solver") == 0) {
            // CORRECTION : On passe argc - 1 et l'adresse de argv[1]
            // Ainsi dans solver_test, argv[0] devient "solver", argv[1] le fichier, etc.
            solver_test(argc - 1, &argv[1]);
            return 0;
        }
        if (strcmp(argv[1], "neuron") == 0) {
            network_test(argc - 1, &argv[1]);
            return 0;
        }
        if (strcmp(argv[1], "rotation") == 0) {
            run_gui(argc, argv);
            return 0;
        }
    }
    run_interface(argc, argv);
    return 0;
}
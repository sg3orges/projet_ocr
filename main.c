#include <string.h>
#include "rotations/gui.h"
#include "detectionV2/detection.h"
#include "solver/solver.h"
#include "neuronne/networks.h"

int main(int argc, char **argv)
{
    // --- Mode console (comme avant)
    if (argc > 1) {
        if (strcmp(argv[1], "detect") == 0 && argc > 2) {
            return detection_run_app(2, &argv[1]);
        }
        if (strcmp(argv[1], "solver") == 0) {
            solver_test();
            return 0;
        }
        if (strcmp(argv[1], "neuron") == 0) {
            // Passe les arguments après "neuron" à network_test (image utilisateur optionnelle)
            network_test(argc - 1, &argv[1]);
            return 0;
        }
    }

    // Aucun argument: lancement de l'interface de rotation
    run_gui(argc, argv);
    return 0;
}

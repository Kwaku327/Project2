/** NOTE functional simulator
 * This is a sample main function, you can modify main() to test,
 * but we may use a different main() to grade, so do not put any simulation
 * logics here.
 */

#include <iostream>

#include "MemoryStore.h"
#include "Utilities.h"
#include "funct.h"

using namespace std;

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << LOG_ERROR << "Usage: " << argv[0] << " <input_file>" << endl;
        return ERROR;
    }

    cout << "[Simulator] Loading memory from " << LOG_VAR(argv[1]) << endl;
    auto baseFilename = getBaseFilename(argv[1]) + "_funct";
    initSimulator(new MemoryStore(0, MEMORY_SIZE, argv[1]), baseFilename);

    cout << "[Simulator] Start simulation" << endl;
    auto status = runTillHalt();

    cout << "[Simulator] Finished simulation status: " << status << endl;
    finalizeSimulator();

    return status;
}

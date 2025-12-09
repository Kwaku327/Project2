/** NOTE cycle-accurate simulator
 * This is a sample main function, you can modify main() to test,
 * but we may use a different main() to grade, so do not put any simulation
 * logic here.
 */
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>

#include "cache.h"
#include "MemoryStore.h"
#include "Utilities.h"
#include "cycle.h"

using namespace std;

inline std::tuple<std::string, CacheConfig, CacheConfig> parseArgs(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << LOG_ERROR << "Usage: " << argv[0] << " <file.bin> <cache_config.txt>"
                  << std::endl
                  << "Note:" << std::endl
                  << "The sim_cycle binary should take two command-line arguments indicating the "
                     "name of the binary file to be read and the cache configuration file to be "
                     "used. [See detail in project description document]."
                  << std::endl;
        exit(ERROR);
    }

    try {
        std::string inputFile = argv[1];
        std::string cacheFile = argv[2];

        std::ifstream file(cacheFile);
        if (!file.is_open()) {
            std::cerr << LOG_ERROR << "Failed to open cache config file: " << cacheFile
                      << std::endl;
            exit(ERROR);
        }

        int line = 0;
        auto parseNextLine = [&](const char* name) -> uint32_t {
            line++;
            uint32_t value;
            if (!(file >> value)) {
                std::stringstream errorMessage;
                errorMessage << "Failed to parse property at line " << line << " for property "
                             << name;
                throw std::invalid_argument(errorMessage.str());
            }
            std::string discard;
            std::getline(file, discard);  // discard rest of the line
            return value;
        };

        CacheConfig icConfig{parseNextLine("ICache cache size"), parseNextLine("ICache block size"),
                             parseNextLine("ICache ways"), parseNextLine("ICache miss latency")};

        CacheConfig dcConfig{parseNextLine("DCache cache size"), parseNextLine("DCache block size"),
                             parseNextLine("DCache ways"), parseNextLine("DCache miss latency")};

        std::cout << LOG_INFO << LOG_VAR(icConfig) << std::endl;
        std::cout << LOG_INFO << LOG_VAR(dcConfig) << std::endl;

        return std::make_tuple(inputFile, icConfig, dcConfig);

    } catch (const std::invalid_argument& e) {
        std::cerr << LOG_ERROR << e.what() << std::endl;
        exit(ERROR);
    } catch (const std::out_of_range& e) {
        std::cerr << LOG_ERROR << "One of the integer arguments is out of range." << std::endl;
        exit(ERROR);
    }
}

int main(int argc, char** argv) {
    auto simArgs = parseArgs(argc, argv);
    auto inputFile = std::get<0>(simArgs);
    auto iCacheConfig = std::get<1>(simArgs);
    auto dCacheConfig = std::get<2>(simArgs);

    cout << "[Simulator] Loading memory from " << LOG_VAR(inputFile) << endl;
    auto baseFilename = getBaseFilename(argv[1]) + "_cycle";
    initSimulator(iCacheConfig, dCacheConfig, new MemoryStore(0, MEMORY_SIZE, argv[1]),
                  baseFilename);

    cout << "[Simulator] Start simulator" << endl;
    auto status = runTillHalt();
    //auto status = runCycles(10);

    cout << "[Simulator] Finished simulation status: " << status << endl;
    finalizeSimulator();

    return status;
}

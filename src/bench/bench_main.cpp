#include "load_generator.h"
#include <iostream>
#include <string>
#include <cstdlib>

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "  --host <str>       Server host (default: 127.0.0.1)\n"
              << "  --port <int>       Server port (default: 7001)\n"
              << "  --threads <int>    Number of threads (default: 4)\n"
              << "  --ops <int>        Total operations (default: 100000)\n"
              << "  --key-range <int>  Key range (default: 10000)\n"
              << "  --read-ratio <f>   Read ratio 0.0-1.0 (default: 0.8)\n"
              << "  --value-size <int> Value size in bytes (default: 64)\n"
              << "  --help             Print this help and exit\n";
}

int main(int argc, char* argv[]) {
    BenchConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--threads" && i + 1 < argc) {
            config.num_threads = static_cast<size_t>(std::atoi(argv[++i]));
        } else if (arg == "--ops" && i + 1 < argc) {
            config.total_operations = static_cast<size_t>(std::atoi(argv[++i]));
        } else if (arg == "--key-range" && i + 1 < argc) {
            config.key_range = static_cast<size_t>(std::atoi(argv[++i]));
        } else if (arg == "--read-ratio" && i + 1 < argc) {
            config.read_ratio = std::atof(argv[++i]);
        } else if (arg == "--value-size" && i + 1 < argc) {
            config.value_size = static_cast<size_t>(std::atoi(argv[++i]));
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    std::cout << "Benchmark Configuration:\n"
              << "  host:        " << config.host << "\n"
              << "  port:        " << config.port << "\n"
              << "  threads:     " << config.num_threads << "\n"
              << "  ops:         " << config.total_operations << "\n"
              << "  key-range:   " << config.key_range << "\n"
              << "  read-ratio:  " << config.read_ratio << "\n"
              << "  value-size:  " << config.value_size << "\n\n";

    LoadGenerator gen(config);
    gen.run();
    return 0;
}

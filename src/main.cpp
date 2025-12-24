#include "server/config.h"
#include "server/node.h"
#include "common/logger.h"
#include <csignal>
#include <iostream>
#include <string>
#include <memory>

static std::unique_ptr<Node> g_node;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        LOG_INFO("Received shutdown signal");
        if (g_node) {
            g_node->stop();
        }
    }
}

int main(int argc, char* argv[]) {
    std::string config_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: kvstore-server --config <path>\n";
            return 0;
        }
    }

    if (config_path.empty()) {
        std::cerr << "Error: --config <path> required\n";
        return 1;
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    Config config = parse_config(config_path);
    Logger::instance().set_level(config.log_level);

    g_node = std::make_unique<Node>(config);
    g_node->start();

    return 0;
}

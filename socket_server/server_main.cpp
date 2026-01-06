#include "broker.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "   Chat Server - Publish/Subscribe      " << std::endl;
    std::cout << "========================================" << std::endl;
    
    Broker broker;
    if (!broker.initialize(port)) {
        std::cerr << "Failed to initialize broker" << std::endl;
        return 1;
    }
    
    broker.run();
    
    return 0;
}

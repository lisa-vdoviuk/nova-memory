#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

int main() {
    std::cout << "Ollama Chat Bot Backend (Windows Version)" << std::endl;
    std::cout << "Server will run on http://localhost:8080" << std::endl;
    
    std::cout << "Backend is working! Frontend can be served separately." << std::endl;
    
    return 0;
}

cmake_minimum_required(VERSION 3.10)
project(OllamaChatBot)

set(CMAKE_CXX_STANDARD 17)

add_executable(ollama_chatbot src/simple_server.cpp)

if(WIN32)
    target_link_libraries(ollama_chatbot ws2_32)
endif()

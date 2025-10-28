#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

class OllamaClient {
private:
    string host;
    int port;
    
public:
    OllamaClient(const string& h = "localhost", int p = 11434) : host(h), port(p) {}

    string generateResponse(const string& prompt) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return "Error: WSAStartup failed";
        }

        SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket == INVALID_SOCKET) {
            WSACleanup();
            return "Error: Socket creation failed";
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

        if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(clientSocket);
            WSACleanup();
            return "Error: Cannot connect to Ollama on port " + to_string(port);
        }

        string request = R"({"model": "llama3.2:3b", "prompt": ")" + prompt + R"(", "stream": false})";
        string http_request = "POST /api/generate HTTP/1.1\r\n"
                             "Host: localhost:" + to_string(port) + "\r\n"
                             "Content-Type: application/json\r\n"
                             "Content-Length: " + to_string(request.length()) + "\r\n"
                             "\r\n" + request;

        if (send(clientSocket, http_request.c_str(), http_request.length(), 0) == SOCKET_ERROR) {
            closesocket(clientSocket);
            WSACleanup();
            return "Error: Send failed";
        }

        char buffer[8192];
        string response;
        int bytesReceived;
        
        while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytesReceived] = '\0';
            response += buffer;
        }

        closesocket(clientSocket);
        WSACleanup();

        size_t pos = response.find("\"response\":\"");
        if (pos != string::npos) {
            pos += 12;
            size_t end_pos = response.find("\"", pos);
            if (end_pos != string::npos) {
                return response.substr(pos, end_pos - pos);
            }
        }

        return "Error: Could not parse Ollama response";
    }
};

class SimpleWebServer {
private:
    SOCKET serverSocket;
    bool running;
    OllamaClient ollama;
    
public:
    SimpleWebServer() : serverSocket(INVALID_SOCKET), running(false), ollama("localhost", 11434) {}
    
    bool initialize() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cerr << "WSAStartup failed" << endl;
            return false;
        }
        
        serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket == INVALID_SOCKET) {
            cerr << "Socket creation failed" << endl;
            WSACleanup();
            return false;
        }
        
        int enable = 1;
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(enable)) == SOCKET_ERROR) {
            cerr << "Setsockopt failed" << endl;
        }
        
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(8080);
        
        if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            cerr << "Bind failed on port 8080" << endl;
            closesocket(serverSocket);
            WSACleanup();
            return false;
        }
        
        if (listen(serverSocket, 10) == SOCKET_ERROR) {
            cerr << "Listen failed" << endl;
            closesocket(serverSocket);
            WSACleanup();
            return false;
        }
        
        return true;
    }
    
    void start() {
        running = true;
        cout << "🚀 Web Server started on http://localhost:8080" << endl;
        cout << "🤖 Ollama connected: llama3.2:3b (port 11434)" << endl;
        cout << "⏹️  Press Ctrl+C to stop the server" << endl;
        cout << "----------------------------------------" << endl;
        
        while (running) {
            sockaddr_in clientAddr;
            int clientAddrSize = sizeof(clientAddr);
            SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);
            
            if (clientSocket != INVALID_SOCKET) {
                handleClient(clientSocket);
            }
            
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }
    
    void handleClient(SOCKET clientSocket) {
        char buffer[4096];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            
            string request(buffer);
            string response;
            
            if (request.find("GET / ") != string::npos || request.find("GET /index.html") != string::npos) {
                response = serveHTML();
            }
            else if (request.find("GET /style.css") != string::npos) {
                response = serveCSS();
            }
            else if (request.find("GET /script.js") != string::npos) {
                response = serveJS();
            }
            else if (request.find("POST /api/chat") != string::npos) {
                response = handleChatAPI(request);
            }
            else {
                response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>Nova Memory Server is Running!</h1>";
            }
            
            int bytesSent = send(clientSocket, response.c_str(), (int)response.length(), 0);
            if (bytesSent == SOCKET_ERROR) {
                cerr << "Send failed" << endl;
            }
        }
        
        closesocket(clientSocket);
    }
    
    string handleChatAPI(const string& request) {
        size_t json_start = request.find("{");
        if (json_start != string::npos) {
            string json_str = request.substr(json_start);
            size_t message_pos = json_str.find("\"message\":\"");
            if (message_pos != string::npos) {
                message_pos += 11;
                size_t message_end = json_str.find("\"", message_pos);
                if (message_end != string::npos) {
                    string user_message = json_str.substr(message_pos, message_end - message_pos);
                    string ai_response = ollama.generateResponse(user_message);
                    
                    string response_json = R"({"response": ")" + ai_response + R"("})";
                    
                    return "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Content-Length: " + to_string(response_json.length()) + "\r\n"
                           "\r\n" + response_json;
                }
            }
        }
        
        string error_response = R"({"response": "Error: Invalid request format"})";
        return "HTTP/1.1 400 Bad Request\r\n"
               "Content-Type: application/json\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Content-Length: " + to_string(error_response.length()) + "\r\n"
               "\r\n" + error_response;
    }
    
    string serveHTML() {
        string html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Nova Memory Chat Bot</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh; display: flex; justify-content: center; 
            align-items: center; padding: 20px; 
        }
        .chat-container {
            width: 100%; max-width: 800px; height: 600px; 
            background: white; border-radius: 15px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            display: flex; flex-direction: column; overflow: hidden;
        }
        .chat-header {
            background: #2d3748; color: white; padding: 20px;
            display: flex; justify-content: space-between; align-items: center;
        }
        .chat-messages {
            flex: 1; padding: 20px; overflow-y: auto; background: #f7fafc;
        }
        .message { margin-bottom: 16px; display: flex; }
        .user-message { justify-content: flex-end; }
        .bot-message { justify-content: flex-start; }
        .message-content {
            max-width: 70%; padding: 12px 16px; border-radius: 18px;
            line-height: 1.4; white-space: pre-wrap;
        }
        .user-message .message-content {
            background: #4299e1; color: white; border-bottom-right-radius: 4px;
        }
        .bot-message .message-content {
            background: white; color: #2d3748; 
            border: 1px solid #e2e8f0; border-bottom-left-radius: 4px;
        }
        .chat-input { 
            padding: 20px; border-top: 1px solid #e2e8f0; 
            background: white; display: flex; gap: 12px; 
        }
        #message-input {
            flex: 1; padding: 12px; border: 1px solid #cbd5e0;
            border-radius: 8px; resize: none; font-family: inherit;
            font-size: 14px; outline: none;
        }
        #send-button {
            padding: 12px 24px; background: #4299e1; color: white;
            border: none; border-radius: 8px; cursor: pointer;
            font-weight: 600;
        }
        .typing-indicator {
            display: none;
            padding: 12px 16px; background: white; 
            border: 1px solid #e2e8f0; border-radius: 18px;
            border-bottom-left-radius: 4px; max-width: 70%;
        }
        .typing-dots {
            display: inline-block;
        }
        .typing-dots::after {
            content: '...';
            animation: typing 1.5s infinite;
        }
        @keyframes typing {
            0%, 20% { content: '.'; }
            40% { content: '..'; }
            60%, 100% { content: '...'; }
        }
    </style>
</head>
<body>
    <div class="chat-container">
        <div class="chat-header">
            <h1>🤖 Nova Memory Chat</h1>
            <div class="status">
                <span style="color: #48bb78;">●</span>
                <span>Ollama: llama3.2:3b</span>
            </div>
        </div>
        
        <div id="chat-messages" class="chat-messages">
            <div class="message bot-message">
                <div class="message-content">
                    Hello! I'm your AI assistant powered by Ollama (llama3.2:3b). How can I help you today?
                </div>
            </div>
        </div>
        
        <div class="chat-input">
            <textarea id="message-input" placeholder="Type your message here..." rows="3"></textarea>
            <button id="send-button">Send</button>
        </div>
    </div>

    <script>
        class ChatApp {
            constructor() {
                this.chatMessages = document.getElementById('chat-messages');
                this.messageInput = document.getElementById('message-input');
                this.sendButton = document.getElementById('send-button');
                this.initEventListeners();
            }
            
            initEventListeners() {
                this.sendButton.addEventListener('click', () => this.sendMessage());
                this.messageInput.addEventListener('keypress', (e) => {
                    if (e.key === 'Enter' && !e.shiftKey) {
                        e.preventDefault();
                        this.sendMessage();
                    }
                });
            }
            
            async sendMessage() {
                const message = this.messageInput.value.trim();
                if (!message) return;
                
                this.addMessage('user', message);
                this.messageInput.value = '';
                this.setLoading(true);
                
                this.showTypingIndicator();
                
                try {
                    const response = await fetch('/api/chat', {
                        method: 'POST',
                        headers: {
                            'Content-Type': 'application/json',
                        },
                        body: JSON.stringify({ message })
                    });
                    
                    const data = await response.json();
                    this.hideTypingIndicator();
                    this.addMessage('bot', data.response);
                } catch (error) {
                    this.hideTypingIndicator();
                    this.addMessage('bot', 'Error: Could not connect to AI service');
                    console.error('Error:', error);
                } finally {
                    this.setLoading(false);
                }
            }
            
            addMessage(sender, text) {
                const messageDiv = document.createElement('div');
                messageDiv.className = `message ${sender}-message`;
                
                const contentDiv = document.createElement('div');
                contentDiv.className = 'message-content';
                contentDiv.textContent = text;
                
                messageDiv.appendChild(contentDiv);
                this.chatMessages.appendChild(messageDiv);
                this.scrollToBottom();
            }
            
            showTypingIndicator() {
                const typingDiv = document.createElement('div');
                typingDiv.className = 'message bot-message';
                typingDiv.id = 'typing-indicator';
                
                const contentDiv = document.createElement('div');
                contentDiv.className = 'typing-indicator';
                contentDiv.innerHTML = 'AI is thinking<span class="typing-dots"></span>';
                contentDiv.style.display = 'block';
                
                typingDiv.appendChild(contentDiv);
                this.chatMessages.appendChild(typingDiv);
                this.scrollToBottom();
            }
            
            hideTypingIndicator() {
                const typingIndicator = document.getElementById('typing-indicator');
                if (typingIndicator) {
                    typingIndicator.remove();
                }
            }
            
            setLoading(loading) {
                this.sendButton.disabled = loading;
                this.sendButton.textContent = loading ? 'Sending...' : 'Send';
            }
            
            scrollToBottom() {
                this.chatMessages.scrollTop = this.chatMessages.scrollHeight;
            }
        }

        document.addEventListener('DOMContentLoaded', () => {
            new ChatApp();
        });
    </script>
</body>
</html>
        )";
        
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/html\r\n"
               "Content-Length: " + to_string(html.length()) + "\r\n"
               "\r\n" + html;
    }
    
    string serveCSS() {
        string css = R"(
.chat-messages::-webkit-scrollbar {
    width: 6px;
}
.chat-messages::-webkit-scrollbar-track {
    background: #f1f1f1;
}
.chat-messages::-webkit-scrollbar-thumb {
    background: #cbd5e0;
    border-radius: 3px;
}
        )";
        
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/css\r\n"
               "Content-Length: " + to_string(css.length()) + "\r\n"
               "\r\n" + css;
    }
    
    string serveJS() {
        string js = R"(
console.log('Nova Memory Chat with Ollama loaded successfully');
        )";
        
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/javascript\r\n"
               "Content-Length: " + to_string(js.length()) + "\r\n"
               "\r\n" + js;
    }
    
    void stop() {
        running = false;
        if (serverSocket != INVALID_SOCKET) {
            closesocket(serverSocket);
            WSACleanup();
        }
    }
    
    ~SimpleWebServer() {
        stop();
    }
};

int main() {
    SimpleWebServer server;
    
    cout << "====================================" << endl;
    cout << "🤖 Nova Memory AI Chat Bot Server" << endl;
    cout << "====================================" << endl;
    
    if (!server.initialize()) {
        cerr << "Failed to initialize server" << endl;
        return 1;
    }
    
    server.start();
    
    return 0;
}

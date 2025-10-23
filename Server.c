#include <stdio.h>
#include <winsock2.h>
#include <windows.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define BACKLOG 8
#define BUFFER_SIZE 4096

void send_response(SOCKET client, const char* status, const char* content_type, const char* body, int body_len) {
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, content_type, body_len);
    
    send(client, header, header_len, 0);
    if (body && body_len > 0) {
        send(client, body, body_len, 0);
    }
}

void send_file(SOCKET client, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        const char* msg = "404 Not Found";
        send_response(client, "404 Not Found", "text/plain", msg, strlen(msg));
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = (char*)malloc(size);
    if (!content) {
        fclose(f);
        const char* msg = "500 Internal Server Error";
        send_response(client, "500 Internal Server Error", "text/plain", msg, strlen(msg));
        return;
    }
    
    fread(content, 1, size, f);
    fclose(f);
    
    
    const char* content_type = "text/html";
    if (strstr(filename, ".css")) content_type = "text/css";
    else if (strstr(filename, ".js")) content_type = "application/javascript";
    else if (strstr(filename, ".png")) content_type = "image/png";
    else if (strstr(filename, ".jpg") || strstr(filename, ".jpeg")) content_type = "image/jpeg";
    
    send_response(client, "200 OK", content_type, content, size);
    free(content);
}

int main() {
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    
    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Bind failed\n");
        closesocket(server);
        WSACleanup();
        return 1;
    }
    
    if (listen(server, BACKLOG) == SOCKET_ERROR) {
        printf("Listen failed\n");
        closesocket(server);
        WSACleanup();
        return 1;
    }
    
    printf("Server listening on http://localhost:%d\n", PORT);
    
    while (1) {
        SOCKET client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET) {
            printf("Accept failed\n");
            continue;
        }
        
        char request[BUFFER_SIZE] = {0};
        int recv_len = recv(client, request, BUFFER_SIZE - 1, 0);
        
        if (recv_len > 0) {
            request[recv_len] = '\0';
            printf("Request: %s\n", request);
            
            if (strncmp(request, "GET / ", 6) == 0 || strncmp(request, "GET /index.html ", 16) == 0) {
                send_file(client, "index.html");
            }
            else if (strncmp(request, "GET /", 5) == 0) {
                char filename[256];
                sscanf(request + 5, "%255s", filename);
                send_file(client, filename);
            }
            else {
                const char* msg = "400 Bad Request";
                send_response(client, "400 Bad Request", "text/plain", msg, strlen(msg));
            }
        }
        
        closesocket(client);
    }
    
    closesocket(server);
    WSACleanup();
    return 0;
}
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

using namespace std;

int main() {
    sockaddr_in server_address;
    server_address.sin_family=AF_INET;
    server_address.sin_addr.s_addr=INADDR_ANY;
    server_address.sin_port=htons(8080);

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address));
    int status=1;
    while(true) {
        string msg;
        getline(cin, msg);
        char buffer[1024]={0};
        if(msg=="exit" || cin.eof()) break;
        if(msg.empty()) continue;
        const char* message=msg.c_str();
        send(client_socket, message, strlen(message), 0);
        int bytes_recieved=recv(client_socket, buffer, sizeof(buffer), 0);
        cout<<"Response from Server: "<<buffer<<endl;
    }
    close(client_socket);
    return 0;
}
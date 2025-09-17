#include <cstring>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>

using namespace std;

int main() {
    int server_socket=socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_address;
    server_address.sin_family=AF_INET;
    server_address.sin_addr.s_addr=INADDR_ANY;
    server_address.sin_port=htons(8080);

    bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address));
    while(true) {
        cout<<"Waiting for client connection..."<<endl;
        listen(server_socket, 5);

        int client_socket = accept(server_socket, nullptr, nullptr);
        cout<<"Accepted connection from a client"<<endl;
        while(true) {
            char buffer[1024]={0};
            int bytes_received=recv(client_socket, buffer, sizeof(buffer), 0);
            if(bytes_received==0) break;
            cout<<"client: "<<buffer<<endl;
            const char* ack="Received message";
            send(client_socket, ack, strlen(ack), 0);
        }
    }
    close(server_socket);
    return 0;
}
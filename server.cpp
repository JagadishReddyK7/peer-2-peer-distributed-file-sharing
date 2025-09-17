#include <cstring>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>

using namespace std;

void* handle_client(void* arg) {
    int client_socket=*(int*)arg;
    delete (int*)arg;
    char buffer[1024]={0};

    while(true) {
        int bytes_received=recv(client_socket, buffer, sizeof(buffer), 0);
        if(bytes_received<=0) {
            cerr<<"Client #"<<client_socket<<" closed"<<endl;
            break;
        }
        buffer[bytes_received]='\0';
        cout<<"client #"<<client_socket<<":"<<buffer<<endl;
        const char* ack="Message received!";
        send(client_socket, ack, strlen(ack), 0);
    }
    close(client_socket);
    return nullptr;
}

int main() {
    int portno=8080;
    int server_socket=socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_address;
    server_address.sin_family=AF_INET;
    server_address.sin_addr.s_addr=INADDR_ANY;
    server_address.sin_port=htons(8080);

    bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address));
    listen(server_socket, 5);
    while(true) {
        cout<<"Running on " <<portno<<" and waiting for client connection..."<<endl;
        
        int client_socket = accept(server_socket, nullptr, nullptr);
        if(client_socket<0) {
            cerr<<"Error accepting connection"<<endl;
            continue;
        }
        cout<<"Accepted connection from client #"<<client_socket<<endl;
        int* new_socket = new int;
        *new_socket = client_socket;
        pthread_t thread_id;
        if(pthread_create(&thread_id, NULL, handle_client, (void*)new_socket)!=0) {
            perror("Failed to create thread");
            delete new_socket;
        }
        else {
            pthread_detach(thread_id);
        }
    }
    close(server_socket);
    return 0;
}
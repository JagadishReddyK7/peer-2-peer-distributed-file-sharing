#include <cstring>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <vector>
#include <errno.h>

using namespace std;

sockaddr_in local_address;
socklen_t address_length = sizeof(local_address);
int client_portno=0;

int request_connection(int portno, int& client_socket, sockaddr_in& server_address) {
    server_address.sin_port=htons(portno);
    if(connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address))<0) {
        if(errno == ECONNREFUSED) {
            perror("Server is not running/refused the connection");
        }
        else {
            perror("connection failed");
        }
        return 1;
    }
    else {
        if (getsockname(client_socket, (struct sockaddr*)&local_address, &address_length) == -1) {
            perror("getsockname failed");
        } else {
            client_portno  = ntohs(local_address.sin_port);
            // cout<<client_portno<<endl;
        }
        return 0;
    }
}

int main(int argc, char* argv[]) {
    const char* tracker_file = argv[1];
    int tracker_no;
    int tracker_fd;
    string ip;
    char buffer[1024];
    vector<string> tracker_ips;
    tracker_fd = open(tracker_file, O_RDONLY, 0644);
    ssize_t bytes_read = read(tracker_fd, buffer, 1024);
    if(bytes_read<0) {
        perror("Failed to read file");
        return 1;
    }
    for(size_t i=0;i<bytes_read;i++) {
        if(buffer[i]=='\n') {
            tracker_ips.push_back(ip);
            ip.clear();
        }
        else {
            ip+=buffer[i];
        }
    }
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_address;
    server_address.sin_family=AF_INET;
    server_address.sin_addr.s_addr=INADDR_ANY;
    for(size_t i=0;i<tracker_ips.size();i++) {
        string portno_str = tracker_ips[i].substr(10);
        int portno = atoi(portno_str.c_str());
        if(request_connection(portno, client_socket, server_address)==0) {
            cout<<"connecter to tracker "<<i<<" successfully"<<endl;
            tracker_no = i;
            break;
        }
    }
    
    while(true) {
        string msg;
        getline(cin, msg);
        string port=to_string(client_portno);
        msg=port+msg;
        char buffer[1024]={0};
        if(msg=="exit" || cin.eof()) break;
        if(msg.empty()) continue;
        const char* message=msg.c_str();
        // cout<<message<<endl;
        int bytes_sent = send(client_socket, message, strlen(message), MSG_NOSIGNAL);
        int bytes_recieved=recv(client_socket, buffer, sizeof(buffer), 0);
        if(bytes_recieved<=0) {
            if(tracker_no==0) {
                close(client_socket);
                client_socket = socket(AF_INET, SOCK_STREAM, 0);
                int portno=8081;
                if(request_connection(portno, client_socket, server_address)==1) break;
                else {
                    cout<<"connected to tracker 1 successfully"<<endl;
                    tracker_no = 1;
                }
            }
            else {
                cout<<"Both trackers are down"<<endl;
                break;
            }
        }
        else cout<<"Response from Server: "<<buffer<<endl;
    }
    close(client_socket);
    return 0;
}
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <vector>
#include <bits/stdc++.h>

using namespace std;

typedef struct {
    int client_socket;
    int tracker_socket;
} sockets;

typedef struct {
    string username;
    string password;
    int client_fd;
} User;

typedef struct {
    string group_id;
    string owner_id;
    map<string, User> users_present;
    map<string, string> request_queue;
} group;

unordered_map<string, User> user_list;
unordered_map<string, User> loggedin_users;
unordered_map<int, string> client_info;
unordered_map<string, group> group_info;

void list_users() {
    for(auto user=user_list.begin();user!=user_list.end();user++) {
        cout<<user->first<<endl;
    }
}

int create_user(vector<string> &command, string &ack, int client_fd) {
    string user_id = command[1];
    if(user_list.count(user_id)!=0) {
        ack = "User "+user_id+" already exists";
        cout<<ack<<endl;
    }
    User user;
    user.username = command[1];
    user.password = command[2];
    user.client_fd = client_fd;
    user_list.insert({user_id, user});
    ack = "User "+user_id+" created successfully";
    cout<<ack<<endl;
    return 0;
}

void login_user(vector<string> &command, string& ack, int client_fd) {
    if(client_info.count(client_fd)!=0) {
        ack="A user is already logged in this client";
        cout<<ack<<endl;
        return;
    }
    string user_id = command[1];
    if(user_list.count(user_id)!=0) {
        if(loggedin_users.count(user_id)!=0) {
            ack="User "+user_id+" already logged in";
            cout<<ack<<endl;
        }
        else {
            User known_user = user_list[user_id];
            string password = command[2];
            if(known_user.password!=password) {
                ack="Inavlid username/password";
                cout<<ack<<endl;
                return;
            }
            known_user.client_fd = client_fd;
            loggedin_users.insert({user_id, known_user});
            client_info.insert({client_fd, user_id});
            ack="User "+user_id+" logged in successfully";
            cout<<ack<<endl;
        }
    }
    else {
        ack="User "+user_id+" does not exist";
        cout<<ack<<endl;
    }
}

void logout_user(string& ack, int client_fd) {
    if(client_info.count(client_fd)==0) {
        ack="No user logged in currently";
        return;
    }
    string user_id = client_info[client_fd];
    client_info.erase(client_fd);
    loggedin_users.erase(user_id);
    ack = "user "+user_id+" logged out successfully";
    cout<<ack<<endl;
}

vector<string> tokenize_message(string &part) {
    vector<string> tokens;
    const char* delimiters = " \t\n\r";
    char* token=strtok(&part[0], delimiters);
    while(token!=nullptr) {
        tokens.push_back(string(token));
        token=strtok(nullptr, delimiters);
    }
    return tokens;
}

int execute_command(string &message, string& ack, int client_fd) {
    vector<string> command = tokenize_message(message);
    if(command[0]=="create_user") {
        create_user(command, ack, client_fd);
    }
    else if(command[0]=="list_users") {
        list_users();
    }
    else if(command[0]=="login") {
        login_user(command, ack, client_fd);
    }
    else if(command[0]=="logout") {
        logout_user(ack, client_fd);
    }
    else if(command[0]=="create_group") {
        string user_id = client_info[client_fd];
        string group_id = command[1];
        group grp;
        grp.group_id = group_id;
        grp.owner_id = user_id;
        grp.users_present[user_id] = user_list[user_id];
        group_info[group_id] = grp;
        ack = "Group "+group_id+" created successfully";
        cout<<ack<<endl;
    }
    else if(command[0]=="join_group") {
        string group_id = command[1];
        string user_id = client_info[client_fd];
        group_info[group_id].request_queue[user_id]=user_id;
        ack="Request sent to owner to join the group";
        cout<<ack<<endl;
    }
    else if(command[0]=="list_groups") {
        ack="";
        for(auto it=group_info.begin();it!=group_info.end();it++) {
            cout<<it->first<<endl;
            ack+=it->first+" : ";
        }
    }
    else if(command[0]=="list_requests") {
        string group_id = command[1];
        ack="";
        group grp = group_info[group_id];
        for(auto it=grp.request_queue.begin(); it!=grp.request_queue.end();it++) {
            cout<<it->first<<endl;
            ack+=it->first+" : ";
        }
    }
    else if(command[0]=="accept_request") {
        string group_id = command[1];
        string user_id = command[2];
        if(group_info[group_id].request_queue.count(user_id)==0) {
            ack="User "+user_id+" did not request for the group";
            cout<<ack<<endl;
        }
        else {
            group_info[group_id].users_present[user_id] = user_list[user_id];
            group_info[group_id].request_queue.erase(user_id);
            ack="User "+user_id+" joined group successfully";
            cout<<ack<<endl;
        }
    }
    else if(command[0]=="leave_group") {
        string user_id = client_info[client_fd];
        string group_id = command[1];
        if(group_info[group_id].users_present.count(user_id)!=0) {
            group_info[group_id].users_present.erase(user_id);
            ack = "User "+user_id+" left group "+group_id+" successfully";
            cout<<ack<<endl;
        }
    }
    else {
        cout<<"Invalid command"<<endl;
    }
    return 0;
}

void* handle_client(void* arg) {
    sockets* socket =(sockets*)arg;
    int client_socket = socket->client_socket;
    int tracker_socket = socket->tracker_socket;
    delete (sockets*)arg;
    char buffer[1024]={0};

    while(true) {
        int bytes_received=recv(client_socket, buffer, sizeof(buffer), 0);
        if(bytes_received<=0) {
            cerr<<"Client #"<<client_socket<<" closed"<<endl;
            break;
        }
        buffer[bytes_received]='\0';
        string message = string(buffer);
        string check = message.substr(0,3);
        if(tracker_socket!=0 && check!="syn") {
            string sync_message = "syn"+message;
            char sync_buffer[sync_message.length()+1];
            strcpy(sync_buffer, sync_message.c_str());
            send(tracker_socket, sync_buffer, sizeof(sync_buffer), 0);
        }
        else {
            message = message.substr(3);
        }
        int client_socket_2 = atoi(message.substr(0,5).c_str());
        string ack="default";
        execute_command(message, ack, client_socket_2);
        cout<<"client #"<<client_socket_2<<":"<<message<<endl;
        send(client_socket, ack.c_str(), strlen(ack.c_str()), 0);
    }
    close(client_socket);
    return nullptr;
}

int main(int argc, char* argv[]) {
    int portno=atoi(argv[1]);
    int tracker_no = atoi(argv[2]);
    int tracker_portno=portno+1;

    int server_socket=socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_address;
    server_address.sin_family=AF_INET;
    server_address.sin_addr.s_addr=INADDR_ANY;
    server_address.sin_port=htons(portno);
    int tracker_socket=0;
    if(tracker_no==0) {
        tracker_socket=socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in tracker_address;
        tracker_address.sin_family=AF_INET;
        tracker_address.sin_addr.s_addr=INADDR_ANY;
        tracker_address.sin_port=htons(tracker_portno);

        connect(tracker_socket, (struct sockaddr*)&tracker_address, sizeof(tracker_address));
    }
    

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
        sockets* new_socket = new sockets;
        new_socket->client_socket = client_socket;
        new_socket->tracker_socket = tracker_socket;
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
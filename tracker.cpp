#include <cstring>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <set>
#include <map>
#include <arpa/inet.h>

using namespace std;

typedef struct {
    int client_socket;
    int tracker_socket;
    string client_ip;
} sockets;

typedef struct {
    string username;
    string password;
    int client_fd;
    int client_port;
} User;

typedef struct {
    string group_id;
    string owner_id;
    unordered_map<string, User> users_present;
    unordered_map<string, string> request_queue;
} group;

typedef struct {
    string filename;
    long long file_size;
    string piece_hashes_str;
    set<string> seeders;
    string full_file_hash;
} FileMetadata;

unordered_map<string, User> user_list;
unordered_map<string, User> loggedin_users;
unordered_map<int, string> client_info;
unordered_map<string, group> group_info;
unordered_map<string, map<string, FileMetadata>> group_files;

mutex user_list_mutex;
mutex loggedin_users_mutex;
mutex client_info_mutex;
mutex group_info_mutex;
mutex group_files_mutex;

void list_users() {
    for(auto user=user_list.begin();user!=user_list.end();user++) {
        cout<<user->first<<endl;
    }
}

int create_user(vector<string> &command, string &ack, int client_fd) {
    lock_guard<mutex> lock(user_list_mutex);
    cout<<"*******************"<<endl;
    string user_id = command[1];
    cout<<user_id<<endl;
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
    scoped_lock lock(user_list_mutex, loggedin_users_mutex, client_info_mutex);
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
    scoped_lock lock(loggedin_users_mutex, client_info_mutex);
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

int execute_command(string &message, string& ack, int client_fd, const string& client_ip) {
    vector<string> command = tokenize_message(message);
    if(command[0]=="create_user") {
        cout<<"###########"<<endl;
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
        scoped_lock lock(user_list_mutex, client_info_mutex, group_info_mutex);
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
        scoped_lock lock(user_list_mutex, loggedin_users_mutex, client_info_mutex);
        string group_id = command[1];
        string user_id = client_info[client_fd];
        group_info[group_id].request_queue[user_id]=user_id;
        ack="Request sent to owner to join the group";
        cout<<ack<<endl;
    }
    else if(command[0]=="list_groups") {
        scoped_lock lock(user_list_mutex, loggedin_users_mutex, client_info_mutex);
        ack="";
        for(auto it=group_info.begin();it!=group_info.end();it++) {
            cout<<it->first<<endl;
            ack+=it->first+" : ";
        }
    }
    else if(command[0]=="list_requests") {
        scoped_lock lock(user_list_mutex, loggedin_users_mutex, client_info_mutex);
        string group_id = command[1];
        ack="";
        group grp = group_info[group_id];
        for(auto it=grp.request_queue.begin(); it!=grp.request_queue.end();it++) {
            cout<<it->first<<endl;
            ack+=it->first+" : ";
        }
    }
    else if(command[0]=="accept_request") {
        scoped_lock lock(user_list_mutex, loggedin_users_mutex, client_info_mutex);
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
        scoped_lock lock(user_list_mutex, loggedin_users_mutex, client_info_mutex);
        string user_id = client_info[client_fd];
        string group_id = command[1];
        if(group_info[group_id].users_present.count(user_id)!=0) {
            group_info[group_id].users_present.erase(user_id);
            ack = "User "+user_id+" left group "+group_id+" successfully";
            cout<<ack<<endl;
        }
    }
    else if(command[0] == "upload_file") {
        scoped_lock lock(user_list_mutex, loggedin_users_mutex, client_info_mutex, group_files_mutex);

        if(command.size() != 6) {
            ack = "Error: Invalid upload_file command format.";
            cout << ack << endl;
            return 1;
        }

        string group_id = command[1];
        string filename = command[2];
        long long file_size = stoll(command[3]);
        string full_hash = command[4];
        string piece_hashes = command[5];

        // Find the P2P port of the client. The client sends this as the first
        // part of every message. In your handle_client, you called it client_socket_2.
        // It's better to rename it to client_p2p_port for clarity.
        int client_p2p_port = client_fd; // This is how you are currently passing it
        string seeder_address = client_ip + ":" + to_string(client_p2p_port);

        // Create metadata object
        FileMetadata meta;
        meta.filename = filename;
        meta.file_size = file_size;
        meta.full_file_hash = full_hash;
        meta.piece_hashes_str = piece_hashes;
        
        // Insert/update file metadata
        group_files[group_id][filename] = meta;
        // Add the current user as a seeder
        group_files[group_id][filename].seeders.insert(seeder_address);

        ack = "File '" + filename + "' registration successful.";
        cout << ack << endl;
    }
    else if(command[0] == "list_files") {
        // Lock the necessary mutexes for reading shared data
        scoped_lock lock(user_list_mutex, loggedin_users_mutex, client_info_mutex, group_files_mutex);

        if(command.size() != 2) {
            ack = "Error: Usage: list_files <group_id>";
            cout << ack << endl;
            return 1;
        }

        string group_id = command[1];
        string user_id = client_info[client_fd]; // The p2p port is passed as client_fd

        // Authorization: Check if the group exists and if the user is a member
        if (group_info.find(group_id) == group_info.end()) {
            ack = "Error: Group '" + group_id + "' does not exist.";
            cout << ack << endl;
            return 1;
        }
        if (group_info[group_id].users_present.find(user_id) == group_info[group_id].users_present.end()) {
            ack = "Error: You are not a member of group '" + group_id + "'.";
            cout << ack << endl;
            return 1;
        }

        // Check if there are any files uploaded for this group
        if (group_files.find(group_id) == group_files.end() || group_files[group_id].empty()) {
            ack = "No files found in group '" + group_id + "'.";
            cout << ack << endl;
            return 0;
        }

        // Build the response string with all the filenames
        string file_list_str;
        for (auto const& [filename, metadata] : group_files[group_id]) {
            file_list_str += filename + "\n";
        }

        ack = file_list_str;
        cout << "Sent file list for group '" << group_id << "' to client." << endl;
    }
    else if(command[0] == "download_file") {
        lock_guard<mutex> lock(group_files_mutex);
        // You might also lock group_info_mutex and client_info_mutex for the authorization check

        if (command.size() != 3) {
            ack = "Error: Usage: download_file <group_id> <file_name>";
            cout << ack << endl;
            return 1;
        }

        string group_id = command[1];
        string filename = command[2];
        
        // Authorization check can go here...

        // Check if the file exists in the group
        if (group_files.count(group_id) == 0 || group_files[group_id].count(filename) == 0) {
            ack = "Error: File not found in this group.";
            cout << ack << endl;
            return 1;
        }

        // If the file exists, get its metadata
        FileMetadata meta = group_files[group_id][filename];
        if (meta.seeders.empty()) {
            ack = "Error: No seeders currently available for this file.";
            cout << ack << endl;
            return 1;
        }

        // Build the response payload:
        // FORMAT: <file_size> <all_piece_hashes> <seeder1_ip:port> <seeder2_ip:port> ...
        string response_payload = to_string(meta.file_size) + " " + meta.full_file_hash + " " + meta.piece_hashes_str;
        for (const string& seeder : meta.seeders) {
            response_payload += " " + seeder;
        }

        ack = response_payload;
        cout << "Sent metadata for '" << filename << "' to a client." << endl;
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
    string client_ip = socket->client_ip;
    
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
        message = message.substr(5);
        string ack="Invalid command";
        execute_command(message, ack, client_socket_2, client_ip);
        cout<<"client on portno "<<client_socket_2<<": "<<message<<endl;
        send(client_socket, ack.c_str(), strlen(ack.c_str()), 0);
    }
    close(client_socket);
    delete (sockets*)arg;
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
        sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_len);
        if(client_socket<0) {
            cerr<<"Error accepting connection"<<endl;
            continue;
        }
        char client_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip_str, INET_ADDRSTRLEN);
        cout<<"Accepted connection from client #"<<client_socket<<" with IP: "<<client_ip_str<<endl;
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
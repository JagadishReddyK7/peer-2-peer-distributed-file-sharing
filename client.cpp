#include <cstring>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <vector>
#include <errno.h>
#include <sstream>
#include "hashing.h"
#include <pthread.h>
#include <mutex>
#include <map>
#include <arpa/inet.h>
#include "protocol.h"

using namespace std;

sockaddr_in local_address;
socklen_t address_length = sizeof(local_address);
int client_portno=0;

map<string, string> files_being_seeded;
mutex seeding_map_mutex;

// string calculate_sha1_of_file(string& file_path) {
//     return "";
// }

// In client.cpp
void* handle_peer_thread(void* arg) {
    int peer_socket = *(int*)arg;
    delete (int*)arg;

    PieceRequest request;
    int bytes_received = recv(peer_socket, &request, sizeof(request), 0);

    if (bytes_received <= 0) {
        cerr << "Peer disconnected before sending request." << endl;
        close(peer_socket);
        return nullptr;
    }

    string file_hash_str(request.file_hash);
    string file_path;

    // Find the requested file path using the hash
    {
        lock_guard<mutex> lock(seeding_map_mutex);
        if (files_being_seeded.count(file_hash_str) == 0) {
            cerr << "Peer requested a file I am not seeding." << endl;
            close(peer_socket);
            return nullptr;
        }
        file_path = files_being_seeded[file_hash_str];
    }
    
    // Read the requested piece from the file
    int fd = open(file_path.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("Could not open file to read piece");
        close(peer_socket);
        return nullptr;
    }
    
    char piece_buffer[PIECE_SIZE];
    lseek(fd, request.piece_index * PIECE_SIZE, SEEK_SET);
    ssize_t bytes_read = read(fd, piece_buffer, PIECE_SIZE);
    close(fd);

    if (bytes_read > 0) {
        // Send the piece data back to the peer
        cout << "Sending piece #" << request.piece_index << " of file " << file_hash_str << endl;
        send(peer_socket, piece_buffer, bytes_read, 0);
    }

    close(peer_socket);
    return nullptr;
}


void* seeder_thread_function(void* arg) {
    // 1. Retrieve the port number from the argument and free the memory
    int p2p_port = *(int*)arg;
    delete (int*)arg;

    int listening_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    // Set socket options to allow reusing the address, which is helpful for quick restarts
    int opt = 1;
    if (setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
    }
    
    sockaddr_in seeder_address;
    seeder_address.sin_family = AF_INET;
    seeder_address.sin_addr.s_addr = INADDR_ANY;
    seeder_address.sin_port = htons(p2p_port);

    if (bind(listening_socket, (struct sockaddr*)&seeder_address, sizeof(seeder_address)) < 0) {
        perror("P2P Bind Failed. This port may be in use.");
        close(listening_socket);
        return nullptr;
    }

    listen(listening_socket, 5);
    cout << "Client is now listening for other peers on port " << p2p_port << endl;

    while (true) {
        int peer_socket = accept(listening_socket, nullptr, nullptr);
        if (peer_socket < 0) {
            cerr << "Error accepting peer connection" << endl;
            continue;
        }
        cout << "Accepted a connection from another peer." << endl;
        int* peer_sock_arg = new int;
        *peer_sock_arg = peer_socket;

        pthread_t peer_thread_id;
        if (pthread_create(&peer_thread_id, NULL, handle_peer_thread, (void*)peer_sock_arg) != 0) {
            perror("Failed to create peer handler thread");
            delete peer_sock_arg;
        } else {
            pthread_detach(peer_thread_id);
        }
    }
    close(listening_socket);
    return nullptr;
}


vector<string> tokenize_message_client(string part) {
    vector<string> tokens;
    char* token = strtok(&part[0], " \t\n\r");
    while(token != nullptr) {
        tokens.push_back(string(token));
        token = strtok(nullptr, " \t\n\r");
    }
    return tokens;
}

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
    int p2p_listening_port = atoi(argv[2]);
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
    cout << "My P2P port is: " << p2p_listening_port << endl;

    // Launch the seeder thread to listen for other peers
    int* port_arg = new int;
    *port_arg = p2p_listening_port;

    // 2. Create the pthread
    pthread_t seeder_thread_id;
    if (pthread_create(&seeder_thread_id, NULL, seeder_thread_function, (void*)port_arg) != 0) {
        perror("Failed to create seeder thread");
        delete port_arg; // Clean up memory if thread creation fails
    } else {
        // 3. Detach the thread so it runs independently in the background
        pthread_detach(seeder_thread_id);
    }

    while(true) {
        string msg;
        getline(cin, msg);
        char buffer[1024]={0};
        // if(msg=="exit" || cin.eof()) break;
        if(msg.empty()) continue;
        vector<string> tokens = tokenize_message_client(msg);
        string final_message;
        string command = tokens[0];
        cout << command << endl;
        if (command == "upload_file") {
            if (tokens.size() != 3) {
                cout << "Usage: upload_file <group_id> <file_path>" << endl;
                continue;
            }
            string group_id = tokens[1];
            string file_path = tokens[2];

            int fd = open(file_path.c_str(), O_RDONLY);
            if (fd < 0) {
                perror("Error opening file");
                continue;
            }
            long long file_size = lseek(fd, 0, SEEK_END);
            close(fd);

            string piece_hashes = get_piece_hashes(file_path);
            string full_file_hash = calculate_sha1_of_file(file_path); 
            if (piece_hashes.empty() || full_file_hash.empty()) {
                cout << "Error hashing file." << endl;
                continue;
            }

            // Store this file for our seeder thread
            {
                lock_guard<mutex> lock(seeding_map_mutex);
                files_being_seeded[full_file_hash] = file_path;
                cout << "[Client A DEBUG] I am now seeding a file." << endl;
                cout << "  Hash: " << full_file_hash << endl;
                cout << "  Path: " << files_being_seeded[full_file_hash] << endl;
            }
            string filename = file_path.substr(file_path.find_last_of("/\\") + 1);
            final_message = "upload_file " + group_id + " " + filename + " " + to_string(file_size) + " " + full_file_hash + " " + piece_hashes;
        }
        else if (command == "list_files") { // <<<--- ADD THIS BLOCK
            if (tokens.size() != 2) {
                cout << "Usage: list_files <group_id>" << endl;
                continue;
            }
            // For this command, the message is simply the raw user input
            final_message = msg;

        }
        else if (command == "download_file") {
            if (tokens.size() != 4) {
                cout << "Usage: download_file <group_id> <file_name> <destination_path>" << endl;
                continue;
            }
            string group_id = tokens[1];
            string filename = tokens[2];
            string destination_path = tokens[3]; // We save this for later

            // The message to the tracker only needs the group and filename
            final_message = "download_file " + group_id + " " + filename;
            
            // --- This part is CRITICAL for the next phase ---
            // Send the request and get the metadata response
            string port = to_string(p2p_listening_port);
            string message_to_send = port + final_message;
            send(client_socket, message_to_send.c_str(), message_to_send.length(), MSG_NOSIGNAL);

            char response_buffer[4096] = {0}; // Use a larger buffer for metadata
            int bytes_recieved = recv(client_socket, response_buffer, sizeof(response_buffer) - 1, 0);

            if (bytes_recieved <= 0) {
                cout << "Tracker disconnected." << endl;
                // ... failover logic ...
                continue; // Skip the rest of the loop
            }
            
            string response(response_buffer);
            cout << "Response from Tracker: " << response << endl;

            // --- PARSE THE METADATA ---
            stringstream ss(response);
            long long file_size;
            string piece_hashes;
            string full_file_hash_from_tracker;
            vector<string> seeder_list;

            ss >> file_size>>full_file_hash_from_tracker>>piece_hashes;
            string seeder;
            while (ss >> seeder) {
                seeder_list.push_back(seeder);
            }
            
            cout << "----------------------------------" << endl;
            cout << "  Full File Hash: " << full_file_hash_from_tracker << endl;
            cout << "Download Info Received:" << endl;
            cout << "  File Size: " << file_size << " bytes" << endl;
            cout << "  Total Seeders: " << seeder_list.size() << endl;
            cout << "  Ready to start download to: " << destination_path << endl;
            cout << "----------------------------------" << endl;

            cout << "Attempting to download piece #0 from the first seeder..." << endl;

            if (seeder_list.empty()) {
                cout << "No seeders available to download from." << endl;
                continue;
            }

            string full_destination_path = destination_path + "/" + filename;

            int fd = open(full_destination_path.c_str(), O_WRONLY | O_CREAT, 0644);
            if (fd < 0) {
                perror("Failed to open destination file for writing");
                continue;
            }

            // 1. Get the address of the first seeder
            string first_seeder = seeder_list[0];
            size_t colon_pos = first_seeder.find(':');
            string seeder_ip = first_seeder.substr(0, colon_pos);
            int seeder_port = stoi(first_seeder.substr(colon_pos + 1));

            // 2. Connect to the seeder
            int peer_sock = socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in peer_addr;
            peer_addr.sin_family = AF_INET;
            peer_addr.sin_port = htons(seeder_port);
            inet_pton(AF_INET, seeder_ip.c_str(), &peer_addr.sin_addr);

            if (connect(peer_sock, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
                perror("Failed to connect to peer");
                close(peer_sock);
                continue;
            }

            // 3. Request Piece #0
            PieceRequest request;
            // You need the full file hash here! You must get this from the tracker.
            // For now, let's assume you've parsed it into a variable `full_file_hash_from_tracker`.
            strncpy(request.file_hash, full_file_hash_from_tracker.c_str(), 40);
            request.piece_index = 0;
            send(peer_sock, &request, sizeof(request), 0);

            // 4. Receive the piece data
            char piece_buffer[PIECE_SIZE];
            ssize_t bytes_received = recv(peer_sock, piece_buffer, PIECE_SIZE, 0); // Add a loop here for robust recv
            close(peer_sock);

            if (bytes_received > 0) {
                // 5. Verify the piece
                string received_hash = calculate_sha1(piece_buffer, bytes_received);
                // You need to get the expected hash for piece #0 from the tracker's `piece_hashes` string
                string expected_hash = piece_hashes.substr(0, 40); 

                if (received_hash == expected_hash) {
                    cout << "SUCCESS: Piece #0 downloaded and verified!" << endl;
                    off_t offset = 0;
                    ssize_t bytes_written = pwrite(fd, piece_buffer, bytes_received, offset);
                    if (bytes_written < 0) {
                        perror("Error writing piece to file");
                    } else {
                        cout << "Piece #0 has been written to " << full_destination_path << endl;
                    }
                } else {
                    cout << "ERROR: Hash mismatch for Piece #0." << endl;
                    cout << "  Expected: " << expected_hash << endl;
                    cout << "  Received: " << received_hash << endl;
                }
            } else {
                cout << "Failed to receive piece data from peer." << endl;
            }

            continue; // Go back to command prompt

        }
        else if (command == "exit" || cin.eof()) {
            break;
        }
        else {
            final_message = msg;
        }
        string port=to_string(p2p_listening_port);
        final_message=port+final_message;
        const char* message=final_message.c_str();
        cout<<message<<endl;
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
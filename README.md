# **Peer-2-Peer Distributed File Sharing System**

In this interim submission, I have implemented the following requirements: 

### 1. Handled multiple clients using Multi-threading
- Accepted the connections from multiple clients and spawned a new thread for each client connection.

### 2. Implemented two trackers

### 3. Synchronized the trackers using message passing
- Achieved synchronization between the trackers by establishing a client-server connection between them.
- The secondary tracker acts as the server and the primary tracker acts as the client and sends the command from client to the secondary tracker.
- The message/command received by the secondary tracker is executed similar to that of the primary tracker.
- This achieves the tracker synchronization using message passing.
- Used message passing mechanism because it works fine when the clients are requesting from different machines whereas shared memory mechanism fails in this case.

### 4. Managed Users and Groups
- Implemented user management like creating, logging in and logging out the user
- Stored the data related to users in unordered map of struct (user_list)
- Implemented group management like creating, listing, joining and leaving the groups.
- Also implemented accepting the request for joining the group by group owner
- stored the data related to groups in unordered map (group_info)

### commands for compilation and execution:

    $ g++ tracker.cpp -o tracker
    $ g++ client.cpp -o client
    
    $ ./tracker 8080
    $ ./tracker 8081
    
    $ ./client 8080 8081

### Note:
- I am not giving the tracker_info.txt file that contains the addresses of the two trackers as input as of now. 
- The users connect to the tracker running on the first port and if it is down then connected to the second tracker.

#include "lib/macros.h"
#include "lib/ack.h"

using namespace std;

// Main variables
int clientFD, keep_aliveFD;
struct sockaddr_in client_addr, keep_alive_addr;
int seq_number = 0;
int msg_id = 0;
int addr_len= sizeof(struct sockaddr_in);
struct timeval tv;

// Storages
bool is_alive[4]{0}; // all storages start in death state
struct sockaddr_in storage_addr[4], storage_conn;
int storageFD[4];
int storage_port[4] = {5001, 5002, 5003, 5004};

// Each storage and client has a ack_controller
map<string, ACK_controller> ack_controllers;

void keep_alive();
void processing_client(struct sockaddr_in client, Packet packet);
void answer_query(struct sockaddr_in client, Packet packet);
void send_packet(int destinyFD, struct sockaddr_in destiny_addr, Packet packet);
void send_notification(int destinyFD, struct sockaddr_in destiny_addr, string data);

int main(){
    Packet recv_packet;
    string THIS_IP = "127.0.0.1";
    tv.tv_sec = SEC_TIMEOUT;
    tv.tv_usec = 0;
    
    memset(&client_addr, 0, sizeof(client_addr));
    memset(&keep_alive_addr, 0, sizeof(keep_alive_addr));
    
    if ((clientFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1)     ERROR("Socket")
    if ((keep_aliveFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1) ERROR("Socket")
    for (int i= 0; i < 4; i++){
        if ((storageFD[i] = socket(AF_INET, SOCK_DGRAM, 0)) == -1) ERROR("Socket")
    }
    
    // configure client's socket
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(5000);
    if (inet_pton(AF_INET, THIS_IP.c_str(), &client_addr.sin_addr) == -1)                ERROR("inet_pton")
    if (bind(clientFD,(struct sockaddr *)&client_addr, sizeof(struct sockaddr)) == -1)   ERROR("Bind")
    if (setsockopt(clientFD, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) ERROR("setsockopt")

    // configure keep-alive socket
    keep_alive_addr.sin_family = AF_INET;
    keep_alive_addr.sin_port = htons(5005);
    if (inet_pton(AF_INET, THIS_IP.c_str(), &keep_alive_addr.sin_addr) == -1)                  ERROR("inet_pton")
    if (bind(keep_aliveFD,(struct sockaddr *)&keep_alive_addr, sizeof(struct sockaddr)) == -1) ERROR("Bind")
    if (setsockopt(keep_aliveFD, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0)   ERROR("setsockopt")   

    // configure storage's sockets
    for (int i = 0; i < 4; i++){
        memset(&storage_addr[i], 0, sizeof(storage_addr[i]));
        storage_addr[i].sin_family = AF_INET;
        storage_addr[i].sin_port = htons(storage_port[i]);
        if (inet_pton(AF_INET, THIS_IP.c_str(), &storage_addr[i].sin_addr) == -1)                   ERROR("inet_pton")
        if (bind(storageFD[i],(struct sockaddr *)&storage_addr[i], sizeof(struct sockaddr)) == -1)  ERROR("Bind")
        if (setsockopt(storageFD[i], SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0)    ERROR("setsockopt")
    }

    thread(keep_alive).detach();
    while(true){
        recv_packet.clear();
        int bytes_readed = recvfrom(clientFD, &recv_packet, sizeof(Packet), MSG_WAITALL, (struct sockaddr *)&client_addr, (socklen_t *)&addr_len);
        if (bytes_readed == -1){
            // Timeout ?????????????????
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            else
                perror("recvfrom");
        }
        else{
            // Received on time
            thread(processing_client, client_addr, recv_packet).detach();
        }
    }
}

void processing_client(struct sockaddr_in client, Packet packet){
    
    string seq_num = packet.type();
    string hash = packet.hash();
    string nickname = packet.nickname();
    
    // If it´s the first connection from the client
    if (ack_controllers.find(nickname) == ack_controllers.end()){
        ack_controllers[nickname] = ACK_controller(nickname, clientFD, client);
    }

    // If packet is ack
    if (packet.type() == "A"){
        ack_controllers[nickname].process_ack(seq_num);
        return;
    }

// If packet is not corrupted send one ACK, else send one more ACK
    // Calc hash only to data, without header
    bool is_good= (hash == calc_hash(packet.data()))? true : false;
    ack_controllers[nickname].replay_ack(seq_num);
    // If packet is corrupted, send one more ACK
    if (!is_good)
        ack_controllers[nickname].replay_ack(seq_num);
    // If packet is not corrupted and it´s a CRUD request
    else
        answer_query(client, packet);
}

void answer_query(struct sockaddr_in client, Packet packet){
    
    // Key = first character of the first node
    int key = (packet.data()[2] % 4); // every client data begin with: 00node...
    
    // If storage server is alive, do the query
    if (is_alive[key]){
        send_packet(storageFD[key], storage_addr[key], packet);
    }
    else{
        // If storage server is not alive, do the query to the next storage server
        // Send notification to the client
        Packet notify;
        int next_key = (key + 1) % 4;
        if (is_alive[next_key]){
            send_packet(storageFD[next_key], storage_addr[next_key], packet);
            send_notification(clientFD, client, "The main storage server is not available, querying next storage server");
        }
        // If no one is alive, send notification to the client
        else
            send_notification(clientFD, client, "The storages servers are not available");
    }
}

void send_packet(int destinyFD, struct sockaddr_in destiny_addr, Packet packet){
    // Save packet into Cache if it's necesary to resend
    ack_controllers[packet.nickname()].insert_packet(stoi(packet.seq_num()), packet);

    sendto(destinyFD, &packet, sizeof(Packet), MSG_CONFIRM, (struct sockaddr *)&destiny_addr, sizeof(struct sockaddr));
    
    seq_number = (seq_number + 1) % 100; // Increment sequence number
    msg_id = (msg_id + 1) % 1000; // Increment message id: FROM STORAGE DON'T CHANGE THE MSG_ID OF THE PACKET;
}

void send_notification(int destinyFD, struct sockaddr_in destiny_addr, string data){
    Packet packet;
    packet.set_data(data);
    string hash = calc_hash(packet.data());
    string header = format_int(seq_number, 2) + hash + "N" + format_int(msg_id, 3) + "0" + "4" + "MAIN";
    packet.set_header(header);
    send_packet(destinyFD, destiny_addr, packet);
}

void keep_alive(){
    struct timeval start, end, elapsed;
    string msg(1, '0');
    int num;
    while (true){
        for (int i = 0; i < 4; i++){
            if (is_alive[i])
                sendto(keep_aliveFD, msg.data(), msg.size(), MSG_CONFIRM, (struct sockaddr *)&storage_addr[i], sizeof(struct sockaddr));
            
            gettimeofday(&start, NULL);
            // Storage answers sending its index
            num = recvfrom(keep_aliveFD, msg.data(), msg.size(), MSG_WAITALL, (struct sockaddr *)&storage_addr[i], (socklen_t *)&addr_len);
            if (num == -1){
                // Timeout
                if (errno == EAGAIN || errno == EWOULDBLOCK){
                    is_alive[i] = false;
                    cout << "Storage" << i << " is not alive" << endl;
                }
                else 
                    perror("recvfrom");
            }
            else{
                // Received on time
                gettimeofday(&end, NULL);
                // Compute elapsed time
                timersub(&end, &start, &elapsed);
                int uremaining = (tv.tv_sec - elapsed.tv_sec) * 1000000 + (tv.tv_usec - elapsed.tv_usec);
                is_alive[i] = true;
                // Sleep for remaining time
                if (uremaining > 0)
                    usleep(uremaining);
            }
        }
    }    
}
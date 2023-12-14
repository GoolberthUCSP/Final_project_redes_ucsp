#include "lib/macros.h"
#include "lib/ack.h"

using namespace std;

// Main variables
int clientFD, keep_aliveFD;
struct sockaddr_in client_addr, keep_alive_addr;
int seq_number = 0;
int msg_id = 0;
struct timeval tv;

// Storages
bool is_alive[4]{0}; // all storages start in death state
struct sockaddr_in storage_addr[4], storage_conn;
int storageFD[4];
int storage_port[4] = {5001, 5002, 5003, 5004};

// Each storage and client has a ack_controller
map<string, ACK_controller> ack_controllers;

void keep_alive();
void processing_client(struct sockaddr_in client, vector<unsigned char> buffer);
void answer_query(struct sockaddr_in client, vector<unsigned char> buffer);
void send_packet(int destinyFD, struct sockaddr_in destiny_addr, string type, string flag, string data);

int main(){
    vector<unsigned char> recv_buffer(SIZE);
    string THIS_IP = "127.0.0.1";
    tv.tv_sec = SEC_TIMEOUT;
    tv.tv_usec = 0;
    
    memset(&client_addr, 0, sizeof(client_addr));
    memset(&keep_alive_addr, 0, sizeof(keep_alive_addr));
    
    if ((clientFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1)     ERROR("Socket")
    if ((keep_aliveFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1) ERROR("Socket")
    
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
        memset(recv_buffer.data(), 0, SIZE);
        int bytes_readed = recvfrom(clientFD, recv_buffer.data(), SIZE, MSG_WAITALL, (struct sockaddr *)&client_addr, (socklen_t *)sizeof(struct sockaddr_in));
        if (bytes_readed == -1){
            // Timeout ?????????????????
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            else
                perror("recvfrom");
        }
        else{
            // Received on time
            thread(processing_client, client_addr, recv_buffer).detach();
        }
    }
}

void processing_client(struct sockaddr_in client, vector<unsigned char> buffer){
    // ss : packet: seq_num=2|hash=6|type=1|msg_id=3|flag=1|nick_size=2|nickname=<nick_size>|<data>
    string seq_num(2, 0), hash(6, 0), type(1, 0), msg_id(3, 0), flag(1, 0), nick_size(2, 0);
    stringstream ss;
    ss.write((char *)buffer.data(), buffer.size());
    ss.read(seq_num.data(), seq_num.size());
    ss.read(hash.data(), hash.size());
    ss.read(type.data(), type.size());
    ss.read(msg_id.data(), msg_id.size());
    ss.read(flag.data(), flag.size());
    ss.read(nick_size.data(), nick_size.size());
    string nickname(stoi(nick_size), 0);
    ss.read(nickname.data(), nickname.size());
    // Reading data
    vector<unsigned char> data(buffer.size() - ss.tellg());
    ss.read((char *)data.data(), data.size());
    
    // If it´s the first connection from the client
    if (ack_controllers.find(nickname) == ack_controllers.end()){
        ack_controllers[nickname] = ACK_controller(nickname, clientFD, client);
    }

    // If packet is ack
    if (type == "A"){
        ack_controllers[nickname].process_ack(seq_num);
        return;
    }

// If packet is not corrupted send one ACK, else send one more ACK
    // Calc hash only to data, without header
    bool is_good= (hash == calc_hash(data))? true : false;
    ack_controllers[nickname].replay_ack(seq_num);
    // If packet is corrupted, send one more ACK
    if (!is_good)
        ack_controllers[nickname].replay_ack(seq_num);
    // If packet is not corrupted and it´s a CRUD request
    else
        answer_query(client, data);
}

void answer_query(struct sockaddr_in client, vector<unsigned char> buffer){

    string nick_size(2, 0), type(1, 0), flag(1, 0);
    copy(nick_size.begin(), nick_size.end(), buffer.begin() + 13);
    copy(type.begin(), type.end(), buffer.begin() + 8);
    copy(flag.begin(), flag.end(), buffer.begin() + 12);
    int header_size = 15 + stoi(nick_size);
    string data(buffer.begin() + header_size, buffer.end());

    // Key = first character of the first node
    int key = (data[2] % 4); // every client data begin with: 00node...
    
    // If storage server is alive, do the query
    if (is_alive[key]){
        send_packet(storageFD[key], storage_addr[key], type, flag, data);
    }
    else{
        // If storage server is not alive, do the query to the next storage server
        // Send notification to the client
        int next_key = (key + 1) % 4;
        if (is_alive[next_key]){
            send_packet(storageFD[next_key], storage_addr[next_key], type, flag, data);
            send_packet(clientFD, client, "N", "0", "The main storage server is not available, ");
        }
        // If no one is alive, send notification to the client
        else
            send_packet(clientFD, client, "N", "0", "The storages servers are not available");
    }
}

void send_packet(int destinyFD, struct sockaddr_in destiny_addr, string type, string flag, string data){
    // seq_num(2, 0), hash(6, 0), type(1, 0), msg_id(3, 0), flag(1, 0), nick_size(2, 0);
    string hash = calc_hash(data);
    string seq_num = format_int(seq_number, 2);
    string msg_id_str = format_int(msg_id, 3);
    string header = seq_num + hash + type + msg_id_str + flag + "01" + "M";
    vector<unsigned char> packet(SIZE);
    copy(header.begin(), header.end(), packet.begin());
    copy(data.begin(), data.end(), packet.begin() + header.size());
    sendto(destinyFD, packet.data(), packet.size(), MSG_CONFIRM, (struct sockaddr *)&destiny_addr, sizeof(struct sockaddr_in));
    seq_number = (seq_number + 1) % 100; // Increment sequence number
    msg_id = (msg_id + 1) % 1000; // Increment message id: FROM STORAGE DON'T CHANGE THE MSG_ID OF THE PACKET;
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
            num = recvfrom(keep_aliveFD, msg.data(), msg.size(), MSG_WAITALL, (struct sockaddr *)&storage_addr[i], (socklen_t *)sizeof(struct sockaddr));
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
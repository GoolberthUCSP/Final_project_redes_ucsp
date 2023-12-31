#include "lib/macros.h"
#include "lib/ack.h"
#include "lib/send.h"
#include "lib/graphdb.hpp"

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
struct sockaddr_in storage_keep_alive_addr[4], storage_keep_alive_conn;
struct sockaddr_in storage_addr[4], storage_conn;
int storageFD[4];
int storage_port[4] = {20121, 20122, 20123, 20124};
mutex storage_mtx[4];
//Only for receiving read responses from storages
vector<map<string, vector<unsigned char>>> incomplete_message(4); // msg_id, data

// Each storage and client has a ack_controller
map<string, ACK_controller> ack_controllers;

void keep_alive();
void processing_client(struct sockaddr_in client, Packet packet);
void answer_query(struct sockaddr_in client, Packet packet);
void send_message_to_one(int destinyFD, struct sockaddr_in destiny_addr, string data, string type, string destiny_nick);


string process_read_query(Packet packet);
string process_cud_query(int storage_idx, Packet packet);

int main(){
    Packet recv_packet;
    tv.tv_sec = SEC_TIMEOUT;
    tv.tv_usec = USEC_TIMEOUT;
    
    memset(&client_addr, 0, sizeof(client_addr));
    if ((clientFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1)     ERROR("Socket")
    
    memset(&keep_alive_addr, 0, sizeof(keep_alive_addr));
    if ((keep_aliveFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1) ERROR("Socket")
    
    for (int i= 0; i < 4; i++){
        memset(&storage_addr[i], 0, sizeof(storage_addr[i]));
        if ((storageFD[i] = socket(AF_INET, SOCK_DGRAM, 0)) == -1) ERROR("Socket")
    }
    
    // configure client's socket
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(20120);
    if (inet_pton(AF_INET, MAIN_IP, &client_addr.sin_addr) == -1)                ERROR("inet_pton")
    if (bind(clientFD,(struct sockaddr *)&client_addr, sizeof(struct sockaddr)) == -1)   ERROR("Bind")
    //if (setsockopt(clientFD, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) ERROR("setsockopt")

    // configure keep-alive socket
    keep_alive_addr.sin_family = AF_INET;
    keep_alive_addr.sin_port = htons(20125);
    if (inet_pton(AF_INET, MAIN_IP, &keep_alive_addr.sin_addr) == -1)                  ERROR("inet_pton")
    if (bind(keep_aliveFD,(struct sockaddr *)&keep_alive_addr, sizeof(struct sockaddr)) == -1) ERROR("Bind")
    if (setsockopt(keep_aliveFD, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0)   ERROR("setsockopt")   

    // configure storage's sockets
    for (int i = 0; i < 4; i++){
        memset(&storage_addr[i], 0, sizeof(storage_addr[i]));
        storage_addr[i].sin_family = AF_INET;
        storage_addr[i].sin_port = htons(storage_port[i]);
        if (inet_pton(AF_INET, MAIN_IP, &storage_addr[i].sin_addr) == -1)                   ERROR("inet_pton")
        if (bind(storageFD[i],(struct sockaddr *)&storage_addr[i], sizeof(struct sockaddr)) == -1)  ERROR("Bind")
        //if (setsockopt(storageFD[i], SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0)    ERROR("setsockopt")
    }

    // Receive first message from storage servers to identify their address
    for (int i = 0; i < 4; i++){
        thread([i](){
            string ans(1, '0');
            recvfrom(storageFD[i], ans.data(), ans.size(), MSG_WAITALL, (struct sockaddr *)&storage_conn, (socklen_t *)&addr_len);
            storage_addr[i] = storage_conn;
            cout << "Storage" << i << " is connected" << endl;
        }).detach();
    }

    thread(keep_alive).detach();
    
    while(true){
        recv_packet.clear();
        int bytes_readed = recvfrom(clientFD, &recv_packet, sizeof(Packet), MSG_WAITALL, (struct sockaddr *)&client_addr, (socklen_t *)&addr_len);
        if (bytes_readed == -1){
            perror("recvfrom");
        }
        else{
            cout << MSG_RECV(client_addr, recv_packet) << endl;
            // Received on time (With timeout)
            thread(processing_client, client_addr, recv_packet).detach();
        }
    }
}

void processing_client(struct sockaddr_in client, Packet packet){
    string seq_num = packet.seq_num();
    string hash = packet.hash();
    string nickname = packet.nickname();
    
    // If it´s the first connection from the client
    if (ack_controllers.find(nickname) == ack_controllers.end()){
        ack_controllers[nickname] = ACK_controller("MAIN", clientFD, client);
    }

    // If packet is ack
    if (packet.packet_type() == "A"){
        ack_controllers[nickname].process_ack(seq_num);
        return;
    }

// If packet is not corrupted send one ACK, else send one more ACK
    // Calc hash only to data, without header
    bool is_good= (hash == calc_hash(packet.data<vector<unsigned char>>()))? true : false;
    ack_controllers[nickname].replay_ack(seq_num);
    // If packet is corrupted, send one more ACK
    if (!is_good)
        ack_controllers[nickname].replay_ack(seq_num);
    // If packet is not corrupted -> it´s a CRUD request
    else
        answer_query(client, packet);
}

void answer_query(struct sockaddr_in client, Packet packet){
    // Key and nickname of the main storage server
    int key = packet.data<string>()[2] % 4; //= first character of the first node. Every client data request begin with: 00node...
    string storage_nick = to_string(key);
    // Key and nickname of the next storage server = redundant
    int next_key = (key + 1) % 4;
    string next_storage_nick = to_string(next_key);

    if (packet.data_type() == "R"){
        // If no one is alive, send a notification of failure
        if (!is_alive[key] && !is_alive[next_key]) {
            string msg = notify("Operation failed: Storage servers " + storage_nick + " and " + next_storage_nick + " are not available");
            // Send notification to the client
            send_message_to_one(clientFD, client, msg, "N", packet.nickname());
            return;
        }

        string result;
        if (is_alive[key]) {
            // Do the query to the main storage server
            result = process_read_query(packet);
        } else { 
            // Do the same operation with the next storage server
            result = process_read_query(packet);
        }

        if (!result.empty())
        {
            vector<Edge> result_edges = stringToEdgeList(result);
            vector<string> result_list = edgeListToString(result_edges, DATA_SIZE-5);
            for (int i = 0; i < result_list.size(); i++)
            {
                send_message_to_one(clientFD, client, (format_int(result_list[i].size(), 3) + result_list[i]), "R", packet.nickname());
                usleep(10000);
            }
        } else {
            result = notify("Operation failed: Node not found in the server");
            send_message_to_one(clientFD, client, result, "N", packet.nickname());
        }
    }
    // If packet is not a read query, storage server sends only a notification
    // To do the query, it needs that the two storage servers are alive
    else {
        // If no one is alive, send a notification of failure
        if (!is_alive[key] && !is_alive[next_key]){
            string msg = notify("Operation failed: Storage servers " + storage_nick + " and " + next_storage_nick + " are not available");
            // Send notification to the client
            send_message_to_one(clientFD, client, msg, "N", packet.nickname());
            return;
        }
        string notification;
        
        if (is_alive[key]){
            // Do the query to the main storage server
            notification = process_cud_query(key, packet);
        }
        if (is_alive[next_key]){ 
            // Do the same operation with the next storage server
            notification = process_cud_query(next_key, packet);
        }
        // Send only a notification to the client
        send_message_to_one(clientFD, client, notification, "N", packet.nickname());
    }
}

void keep_alive(){
    struct timeval start, end, elapsed;
    string msg(1, '0');
    int num;

    while (true){
        for (int i = 0; i < 4; i++){
            if (is_alive[i])
                sendto(keep_aliveFD, msg.data(), msg.size(), MSG_CONFIRM, (struct sockaddr *)&storage_keep_alive_addr[i], sizeof(struct sockaddr));
        
            gettimeofday(&start, NULL);
            // Storage answers sending its index
            num = recvfrom(keep_aliveFD, msg.data(), msg.size(), MSG_WAITALL, (struct sockaddr *)&storage_keep_alive_conn, (socklen_t *)&addr_len);
            if (num == -1){
                // Timeout
                if (errno == EAGAIN || errno == EWOULDBLOCK){
                    is_alive[i] = false;
                }
                else 
                    perror("recvfrom");
            }
            else{
                // Received on time
                gettimeofday(&end, NULL);
                // Identify the storage
                int storage_idx = stoi(msg);
                // Compute elapsed time
                timersub(&end, &start, &elapsed);
                int uremaining = (tv.tv_sec - elapsed.tv_sec) * 1000000 + (tv.tv_usec - elapsed.tv_usec);
                is_alive[storage_idx] = true;
                storage_keep_alive_addr[storage_idx] = storage_keep_alive_conn;
                // Sleep for remaining time
                if (uremaining > 0)
                    usleep(uremaining);
            }
        }
    }    
}

/*
    Process the CUD query
    @param storage_idx: index of the storage server
    @param packet: packet to send to the server
    @return: notification from the storage server
*/
string process_cud_query(int storage_idx, Packet packet){

    storage_mtx[storage_idx].lock();
    string data_str = packet.data<string>();
    string storage_nick = to_string(storage_idx);
    Packet result;
    send_message_to_one(storageFD[storage_idx], storage_addr[storage_idx], data_str, packet.data_type(), packet.nickname());
    // Do loop while the server answers with its 
    do{
        result.clear();
        int bytes_readed = recvfrom(storageFD[storage_idx], &result, sizeof(Packet), MSG_WAITALL, (struct sockaddr *)&storage_addr[storage_idx], (socklen_t *)&addr_len);
        cout << MSG_RECV(storage_addr[storage_idx], result) << endl;
        // If it's the response of the server's first query.
        if (ack_controllers.find(storage_nick) == ack_controllers.end()){
            ack_controllers[storage_nick] = ACK_controller("MAIN", storageFD[storage_idx], storage_addr[storage_idx]);
        }

        if (result.packet_type() == "A"){
            ack_controllers[storage_nick].process_ack(result.seq_num());
            continue;
        }

    // If packet is not corrupted send one ACK, else send one more ACK
        // Calc hash only to data, without header
        string seq_num = result.seq_num();
        bool is_good= (result.hash() == calc_hash(result.data<vector<unsigned char>>()))? true : false;
        ack_controllers[storage_nick].replay_ack(seq_num);
        // If packet is corrupted, send one more ACK
        if (!is_good){
            ack_controllers[storage_nick].replay_ack(seq_num);
            // If packet isn't good, wait one more time; change the type of the packet so as not to get out of the loop
            result.set_data_type("A");
        }
    } while(result.data_type() == "A");

    storage_mtx[storage_idx].unlock();
    return result.data<string>();
}

/*
    Get the read query
    @param storage_idx: index of the storage server
    @param node: node to check connections in the server
    @param nickname: nickname of client
    @return: edges with the node as the first element
*/
string get_read_query(const string& node, const string& nickname) {
    int storage_idx = node[0] % 4;
    if (!is_alive[storage_idx]) 
        storage_idx = (storage_idx + 1) % 4;
    
    if (!is_alive[storage_idx])
        return string();
    
    string data_str = format_int(node.size(), 2) + node + "1";
    string storage_nick = to_string(storage_idx);
    send_message_to_one(storageFD[storage_idx], storage_addr[storage_idx], data_str, "R", nickname);
    
    Packet result;
    // Do loop while the server answers with its 
    do{
        result.clear();
        int bytes_readed = recvfrom(storageFD[storage_idx], &result, sizeof(Packet), MSG_WAITALL, (struct sockaddr *)&storage_addr[storage_idx], (socklen_t *)&addr_len);
        cout << MSG_RECV(storage_addr[storage_idx], result) << endl;
        // If it's the response of the server's first query.
        if (ack_controllers.find(storage_nick) == ack_controllers.end()){
            ack_controllers[storage_nick] = ACK_controller("MAIN", storageFD[storage_idx], storage_addr[storage_idx]);
        }

        if (result.packet_type() == "A"){
            ack_controllers[storage_nick].process_ack(result.seq_num());
            continue;
        }

        // If packet is not corrupted send one ACK, else send one more ACK
        // Calc hash only to data, without header
        string seq_num = result.seq_num();
        bool is_good= (result.hash() == calc_hash(result.data<vector<unsigned char>>()))? true : false;
        ack_controllers[storage_nick].replay_ack(seq_num);
        // If packet is corrupted, send one more ACK
        if (!is_good){
            ack_controllers[storage_nick].replay_ack(seq_num);
            // If packet isn't good, wait one more time; change the type of the packet so as not to get out of the loop
            result.set_data_type("A");
        }
    } while(result.data_type() == "A");
    
    if (result.data_type() == "N")
        return string();

    vector<unsigned char> data = result.data<vector<unsigned char>>(); 
    int data_size = stoi(string(data.begin(), data.begin() + 3));
    return string(data.begin() + 3, data.begin() + 3 + data_size);
}

/*
    Process general read query: recursive and simple read
    @param storage_idx: index of the storage server
    @param packet: packet to send to the storage server
    @return: result of the recursive or simple read   
*/
string process_read_query(Packet packet) {
    for(int i = 0; i < 4; ++i)
        storage_mtx[i].lock();

    vector<unsigned char> data_vec = packet.data<vector<unsigned char>>();
    string data_str = packet.data<string>();
    
    int start_node_len = stoi(data_str.substr(0, 2));
    string start_node = data_str.substr(2, start_node_len);
    int start_recur = stoi(data_str.substr(2 + start_node_len, 1));

    cerr << "DEBUG: " << start_node << " " << start_recur << "\n";

    queue<pair<string, int> > nodes;
    set<string> used_nodes;
    nodes.emplace(start_node, start_recur);
    used_nodes.insert(start_node);
    string result;
    while (!nodes.empty()) {
        string node;
        int recur;
        tie(node, recur) = nodes.front();
        nodes.pop();
        
        //send and receive string
        string storage_result = get_read_query(node, packet.nickname());
        if (!storage_result.empty() && storage_result.back() != ';')
            storage_result.push_back(';');
        
        result += storage_result;
        for (int l = 0, r = 0, n = storage_result.size(); r < n; ++r) {
            switch (storage_result[r]) {
            case ',':
            case ';':
                if (recur > 1 && !used_nodes.count(storage_result.substr(l, r - l))) {
                    nodes.emplace(storage_result.substr(l, r - l), recur - 1);
                    used_nodes.insert(storage_result.substr(l, r - l));
                }
            case ':':
                l = r + 1;
                break;
            default:
                break;
            }
        }
    }

    // cerr << "DEBUG: " << result << "\n";

    for(int i = 0; i < 4; ++i)
        storage_mtx[i].unlock();

    if (result.empty())
        return result;

    //return format_int(result.size(), 3) + result;
    return result;
}

/*
    Send a message that can be too long to be sent in one packet, ONLY USED IN READ RESPONSES
    @param destinyFD: file descriptor of the destiny (socket)
    @param destiny_addr: address of the destiny
    @param data: data to send
    @param destiny_nick: nickname of the destiny
    @return: void
*/
void send_message_to_one(int destinyFD, struct sockaddr_in destiny_addr, string data, string type, string destiny_nick){
    Packet packet;
    packet.set_data_type(type);

    packet.set_seq_num(format_int(seq_number, 2));
    packet.set_msg_id(format_int(msg_id, 3));
    packet.set_nickname("MAIN");

    int packets_sent = send_message(destinyFD, destiny_addr, ack_controllers[destiny_nick], data, packet);
    
    seq_number = (seq_number + packets_sent) % 100;
    msg_id = (msg_id + 1) % 1000;
}
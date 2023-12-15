#include "lib/macros.h"
#include "lib/ack.h"

using namespace std;

// Server variables
int serverFD;
struct sockaddr_in server_addr;
string SERVER_IP = "127.0.0.1";

ACK_controller ack_controller;
map<string, vector<unsigned char>> incomplete_message; // msg_id, data
int seq_number = 0;
int msg_id = 0;
string nickname;
string nick_size;
int addr_len= sizeof(struct sockaddr_in);

void encoding(string buffer);
void decoding(Packet packet);
void thread_receiver();

// CRUD request functions
void create_request(stringstream &ss);
void read_request(stringstream &ss);
void rread_request(stringstream &ss); // Recursive read
void update_request(stringstream &ss);
void delete_request(stringstream &ss);

// Receive functions
void read_response(vector<unsigned char> data);
void recv_notification(vector<unsigned char> data);

// Other functions
void send_packet(Packet packet);

typedef void (*req_ptr)(stringstream&);
typedef void (*recv_ptr)(vector<unsigned char>);

map<string, req_ptr> request_functions({
    {"create", &create_request},
    {"read", &read_request},
    {"rread", &rread_request},
    {"update", &update_request},
    {"delete", &delete_request}
});

map<char, recv_ptr> response_functions({
    {'R', &read_response},
    {'N', &recv_notification}
});

int main(){
    if ((serverFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1)  ERROR("Socket")

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    if (inet_pton(AF_INET, SERVER_IP.c_str(), &server_addr.sin_addr) == -1)   ERROR("inet_pton")

    cout << "Input your nickname: ";
    getline(cin, nickname);
    cin.clear();
    if (nickname.size() > 9) 
        nickname.resize(9);
    nick_size = to_string(nickname.size());

    ack_controller = ACK_controller(nickname, serverFD, server_addr);

    thread(thread_receiver).detach();

    while(true){ // Wait for user input; then send to server
        string usr_input;
        getline(cin, usr_input);
        cin.clear();
        // If user input is exit or quit, exit program
        if (usr_input == "exit" || usr_input == "quit"){
            system("clear || cls");
            exit(EXIT_SUCCESS);
        } 
        // If user input is clear or cls, clear screen
        else if (usr_input == "clear" || usr_input == "cls"){
            system("clear || cls");
        } 
        // If user input is CRUD request
        else{
            thread(encoding, usr_input).detach();
        }
    }
}

void thread_receiver(){
    Packet recv_packet;
    int bytes_readed;
    while(true){
        recv_packet.clear();
        bytes_readed = recvfrom(serverFD, &recv_packet, sizeof(Packet), MSG_WAITALL, (struct sockaddr *)&server_addr, (socklen_t *)&addr_len);
        thread(decoding, recv_packet).detach();
    }   
}

void decoding(Packet packet){
    
    string seq_num = packet.seq_num();
    string hash = packet.hash();
    vector<unsigned char> data = packet.data();
    string message_id = packet.msg_id();
    
    // If packet is ack
    if (packet.type() == "A"){
        ack_controller.process_ack(seq_num);
        return;
    }
    
// If packet is not corrupted send one ACK, else send one more ACK
    // Calc hash only to data, without header
    bool is_good= (hash == calc_hash(data))? true : false;
    ack_controller.replay_ack(seq_num);
    // If packet is corrupted, send one more ACK
    if (!is_good)
        ack_controller.replay_ack(seq_num);
    // If packet is not corrupted and it´s a CRUD response
    else{
        copy(data.begin(), data.end(), back_inserter(incomplete_message[message_id]));
        
        // Verify if flag = 1 (incomplete) else (complete)
        if (packet.flag() == "0"){
            vector<unsigned char> message = incomplete_message[message_id];
            incomplete_message.erase(message_id);
            thread(response_functions[packet.type()[0]], message).detach();
        }
    }
}

void encoding(string buffer){
    stringstream ss(buffer);
    string action;
    vector<unsigned char> encoded;
    getline(ss, action, ' ');
    transform(action.begin(), action.end(), action.begin(), ::tolower);

    if (request_functions.find(action) == request_functions.end()){
        cout << "Invalid action" << endl;
        return;
    }
    request_functions[action](ss);
}


// CRUD request functions
void create_request(stringstream &ss){
    // ss : node1 node2
    Packet packet;
    packet.set_type("C");

    string node1, node2;
    getline(ss, node1, ' ');
    getline(ss, node2, '\0');
    string size1 = format_int(node1.size(), 2);
    string size2 = format_int(node2.size(), 2);
    string data = size1 + node1 + size2 + node2;
    
    packet.set_data(data);
    send_packet(packet);
}

void read_request(stringstream &ss){
    // ss : node
    Packet packet;
    packet.set_type("R");

    string node;
    getline(ss, node, '\0');
    string size = format_int(node.size(), 2);
    string data = size + node + "1"; // Depth = 1
    
    packet.set_data(data);
    send_packet(packet);
}

void rread_request(stringstream &ss){
    // ss : depth node
    Packet packet;
    packet.set_type("R");
    
    string depth, node;
    getline(ss, depth, ' ');
    getline(ss, node, '\0');
    string size = format_int(node.size(), 2);
    string data = size + node + depth;
    
    packet.set_data(data);
    send_packet(packet);
}

void update_request(stringstream &ss){
    // ss : node1 node2 new2
    Packet packet;
    packet.set_type("U");

    string node1, node2, new2;
    getline(ss, node1, ' ');
    getline(ss, node2, ' ');
    getline(ss, new2, '\0');
    string size1 = format_int(node1.size(), 2);
    string size2 = format_int(node2.size(), 2);
    string size3 = format_int(new2.size(), 2);
    string data = size1 + node1 + size2 + node2 + size3 + new2;
    
    packet.set_data(data);
    send_packet(packet);
}

void delete_request(stringstream &ss){
    // ss : node1 node2
    Packet packet;
    packet.set_type("D");

    string node1, node2;
    getline(ss, node1, ' ');
    getline(ss, node2, '\0');
    string size1 = format_int(node1.size(), 2);
    string size2 = format_int(node2.size(), 2);
    string data = size1 + node1 + size2 + node2;
    
    packet.set_data(data);
    send_packet(packet);
}

// Receive functions

void read_response(stringstream &ss){
    // ss : 00node000node1,node2,etc
    string node_size(2, 0), nodes_size(3, 0);

    ss.read(node_size.data(), node_size.size());
    string node(stoi(node_size), 0);
    ss.read(node.data(), node.size());

    ss.read(nodes_size.data(), nodes_size.size());
    string nodes(stoi(nodes_size), 0);
    ss.read(nodes.data(), nodes.size());
    cout << "Read response: " << node << "->" << nodes << endl;
}

void recv_notification(stringstream &ss){
    // ss : 00notification
    string size(2, 0);

    ss.read(size.data(), size.size());
    string notification(stoi(size), 0);
    ss.read(notification.data(), notification.size());
    cout << "Notification received: " << notification << endl;
}

void send_packet(Packet packet){
    string seq_num = format_int(seq_number, 2);

    packet.set_seq_num(seq_num);
    packet.set_hash(calc_hash(packet.data()));
    packet.set_msg_id(format_int(msg_id, 3));
    packet.set_nickname(nickname);
    packet.set_flag("0");

    // Save packet into Cache if it's necesary to resend
    ack_controller.insert_packet(seq_number, packet);
    ack_controller.acks_to_recv.insert(seq_num);
    sendto(serverFD, &packet, sizeof(Packet), MSG_CONFIRM, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
    
    seq_number = (seq_number + 1) % 100; // Increment sequence number
    msg_id = (msg_id + 1) % 1000; // Increment message id
}

void read_response(vector<unsigned char> data){
    // data : 00node000node1,node2,etc
    stringstream ss;
    ss.write((char *)data.data(), data.size());
    string node_size(2, 0), nodes_size(3, 0);

    ss.read(node_size.data(), node_size.size());
    string node(stoi(node_size), 0);
    ss.read(node.data(), node.size());

    ss.read(nodes_size.data(), nodes_size.size());
    string nodes(stoi(nodes_size), 0);
    cout << "Read response: " << node << "->" << nodes << endl;
}

void recv_notification(vector<unsigned char> data){
    // data : 00notification
    stringstream ss;
    ss.write((char *)data.data(), data.size());
    string size(2, 0);
    ss.read(size.data(), size.size());
    string notification(stoi(size), 0);
    ss.read(notification.data(), notification.size());
    cout << "Notification received: " << notification << endl;    
}
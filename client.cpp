#include "lib/macros.h"
#include "lib/ack.h"
#include "lib/send.h"

using namespace std;

// Server variables
int serverFD;
struct sockaddr_in server_addr;
string SERVER_IP = "127.0.0.1";

ACK_controller ack_controller;
// In case of read response too long to be received
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
void send_packet_to_server(Packet packet);

typedef void (*req_ptr)(stringstream&);
typedef void (*recv_ptr)(vector<unsigned char>);

// Map of user input allowed
map<string, req_ptr> request_functions({
    {"create", &create_request},
    {"read", &read_request},
    {"rread", &rread_request},
    {"update", &update_request},
    {"delete", &delete_request}
});

// Map of packet type allowed
map<string, recv_ptr> response_functions({
    {"R", &read_response},
    {"N", &recv_notification}
});

int main(){
    if ((serverFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1)  ERROR("Socket")

    // Define the server's address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    if (inet_pton(AF_INET, SERVER_IP.c_str(), &server_addr.sin_addr) == -1)   ERROR("inet_pton")

    // Process user nickname, if it is too long, cut it into 9 characters
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
        usleep(10000);
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

/*
    Decoding packet from server, if packet is not corrupted send one ACK, else send one more ACK
    @param packet: Packet
    @return void
*/
void decoding(Packet packet){
    string seq_num = packet.seq_num();
    string hash = packet.hash();
    vector<unsigned char> data = packet.data<vector<unsigned char>>();
    string message_id = packet.msg_id();
    
    // If packet is ack
    if (packet.packet_type() == "A"){
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
            thread(response_functions[packet.data_type()], message).detach();
        }
    }
}

/*
    Encoding user input to send to server
    @param buffer : user input
    @return void
*/
void encoding(string buffer){
    stringstream ss(buffer);
    string action;
    vector<unsigned char> encoded;
    getline(ss, action, ' ');
    transform(action.begin(), action.end(), action.begin(), ::tolower);

    // If action is not in request_functions
    if (request_functions.find(action) == request_functions.end()){
        cout << "Invalid action" << endl;
        return;
    }
    request_functions[action](ss);
}


// CRUD request functions

/*
    Create request to the server, the relation node1->node2 is created and duplicated
    @param ss : node1 node2
    @return void
*/
void create_request(stringstream &ss){
    // ss : node1 node2
    Packet packet;

    string node1, node2;
    getline(ss, node1, ' ');
    getline(ss, node2, '\0');
    string size1 = format_int(node1.size(), 2);
    string size2 = format_int(node2.size(), 2);
    // Format data : 00 node1 00 node2
    string data = size1 + node1 + size2 + node2;
    
    packet.set_data(data);
    packet.set_data_type("C");

    send_packet_to_server(packet);
}

/*
    Read request to the server, the relation node->node is read. depth = 1
    @param ss : node
    @return void
*/
void read_request(stringstream &ss){
    // ss : node
    Packet packet;

    string node;
    getline(ss, node, '\0');
    string size = format_int(node.size(), 2);
    // Format data : 00 node 1
    string data = size + node + "1"; // Depth = 1
    
    packet.set_data(data);
    packet.set_data_type("R");

    send_packet_to_server(packet);
}

/*
    Recursive read request to the server, the relation node->nodes is read. depth = [1..3]
    @param ss : depth node
    @return void
*/
void rread_request(stringstream &ss){
    // ss : depth node
    Packet packet;
    
    string depth, node;
    getline(ss, depth, ' ');
    getline(ss, node, '\0');
    string size = format_int(node.size(), 2);
    // Format data : 00 node 0
    string data = size + node + depth;
    
    packet.set_data(data);
    packet.set_data_type("R");

    send_packet_to_server(packet);
}

/*
    Update request to the server, the relation node1->node2 is updated to node1->new2
    @param ss : node1 node2 new2
    @return void
*/
void update_request(stringstream &ss){
    // ss : node1 node2 new2
    Packet packet;

    string node1, node2, new2;
    getline(ss, node1, ' ');
    getline(ss, node2, ' ');
    getline(ss, new2, '\0');
    string size1 = format_int(node1.size(), 2);
    string size2 = format_int(node2.size(), 2);
    string size3 = format_int(new2.size(), 2);
    // Format data : 00 node1 00 node2 00 new2
    string data = size1 + node1 + size2 + node2 + size3 + new2;
    
    packet.set_data(data);
    packet.set_data_type("U");

    send_packet_to_server(packet);
}

/*
    Delete request to the server, the relation node1->node2 is deleted. If node2 = '*' all the relations with node1 are deleted
    @param ss : node1 node2
    @return void
*/
void delete_request(stringstream &ss){
    // ss : node1 node2
    Packet packet;

    string node1, node2;
    getline(ss, node1, ' ');
    getline(ss, node2, '\0');
    string size1 = format_int(node1.size(), 2);
    string size2 = format_int(node2.size(), 2);
    // Format data : 00 node1 00 node2
    string data = size1 + node1 + size2 + node2;
    
    packet.set_data(data);
    packet.set_data_type("D");

    send_packet_to_server(packet);
}

// Receive functions

/*
    Process read response from the server, can be simple or recursive
    @param data: formatted data (00node000node1,node2,etc. 00 = size of node, 000 = size of nodes)
    @return void
*/
void read_response(vector<unsigned char> data){
    // ss : 00node000node1,node2,etc
    stringstream ss;
    ss.write((char *)data.data(), data.size());

    string node_size(2, 0), nodes_size(3, 0);

    ss.read(node_size.data(), node_size.size());
    string node(stoi(node_size), 0);
    ss.read(node.data(), node.size());

    ss.read(nodes_size.data(), nodes_size.size());
    string nodes(stoi(nodes_size), 0);
    ss.read(nodes.data(), nodes.size());
    cout << "Read response: " << node << "->" << nodes << endl;

    // TODO: Process the recursive read response
}

/*
    Process notification from the server and show on screen
    @param data: formatted data (00notification, 00 = size of data)
    @return void
*/
void recv_notification(vector<unsigned char> data){
    // ss : 00notification
    stringstream ss;
    ss.write((char *)(data.data()+1), data.size());
    
    string size(2, 0);

    ss.read(size.data(), size.size());
    string notification(stoi(size), 0);
    ss.read(notification.data(), notification.size());
    cout << "Notification received: " << notification << endl;
}

/*
    Send packet to server, all CRUD requests are sent without to be fragmented
    @param packet: packet to send with sections TYPE and DATA filled
    @return void
*/
void send_packet_to_server(Packet packet){
    string seq_num = format_int(seq_number, 2);

    packet.set_packet_type("D");
    packet.set_seq_num(seq_num);
    packet.set_hash(calc_hash(packet.data<vector<unsigned char>>()));
    packet.set_msg_id(format_int(msg_id, 3));
    packet.set_nickname(nickname);
    // Send message in one packet without fragmentation
    packet.set_flag("0"); 

    send_packet(serverFD, server_addr, ack_controller, packet);
    
    seq_number = (seq_number + 1) % 100; // Increment sequence number
    msg_id = (msg_id + 1) % 1000; // Increment message id
}
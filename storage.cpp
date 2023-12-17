#include "lib/macros.h"
#include "lib/ack.h"
#include "lib/graphdb.hpp"
#include "lib/send.h"

using namespace std;

int mainFD, keep_aliveFD;
struct sockaddr_in main_addr, keep_alive_addr;
string MAIN_IP = "127.0.0.1";
int seq_number = 0;
int msg_id = 0;
int storage_idx;
string storage_nick;
int addr_len= sizeof(struct sockaddr_in);

ACK_controller ack_controller;
GraphDB database; // database["node"]: {nodes that are connected to "node"}
vector<int> storage_ports= {5001, 5002, 5003, 5004};

// CRUD request functions
void create_request(vector<unsigned char> data);
void read_request(vector<unsigned char> data);
void update_request(vector<unsigned char> data);
void delete_request(vector<unsigned char> data);

void processing(Packet packet);
void send_message_to_server(string type, string data);

void keep_alive();

typedef void (*func_ptr)(vector<unsigned char>);
map<string, func_ptr> crud_requests({
    {"C", &create_request},
    {"R", &read_request},
    {"U", &update_request},
    {"D", &delete_request}
});

int main(int argc, char *argv[]){
    if (argc != 2){
        cout << "Bad number of arguments" << endl;
        return 1;
    }
    storage_idx = atoi(argv[1])%4;
    storage_nick = to_string(storage_idx);
    int port = storage_ports[storage_idx];
    ack_controller = ACK_controller(storage_nick, mainFD, main_addr);

    int bytes_readed;
    Packet recv_packet;

    if ((mainFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1)        ERROR("Socket")
    if ((keep_aliveFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1)  ERROR("Socket")

    // Define the main server's address
    memset(&main_addr, 0, sizeof(main_addr));
    main_addr.sin_family = AF_INET;
    main_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, MAIN_IP.c_str(), &main_addr.sin_addr) == -1) ERROR("inet_pton")

    // Define the keep-alive server's address
    memset(&keep_alive_addr, 0, sizeof(keep_alive_addr));
    keep_alive_addr.sin_family = AF_INET;
    keep_alive_addr.sin_port = htons(5005);
    if (inet_pton(AF_INET, MAIN_IP.c_str(), &keep_alive_addr.sin_addr) == -1) ERROR("inet_pton")

    thread(keep_alive).detach();

    // Send first message to main server
    sendto(mainFD, "0", 1, MSG_CONFIRM, (struct sockaddr *)&main_addr, sizeof(main_addr));

	cout << "Storage server connected to main server on port " << port << "..." << endl;
    
    while(true){
        recv_packet.clear();
        bytes_readed = recvfrom(mainFD, &recv_packet, sizeof(Packet), MSG_WAITALL, (struct sockaddr *)&main_addr, (socklen_t *)&addr_len);
        cout << MSG_RECV(main_addr, recv_packet) << endl;
        thread(processing, recv_packet).detach();
    }
}


void processing(Packet packet){
    string seq_num = packet.seq_num();
    string hash = packet.hash();
    vector<unsigned char> data = packet.data<vector<unsigned char>>();

    // If packet is ACK
    if (packet.packet_type() == "A"){
        ack_controller.process_ack(seq_num);
        return;
    }
    
// If packet is not corrupted send one ACK, else send one more ACK 
    // Calc hash only to data, without header
    bool is_good= (hash == calc_hash(data))? true : false; 
    ack_controller.replay_ack(seq_num);
    // If packet is corrupted, send second ACK
    if (!is_good) 
        ack_controller.replay_ack(seq_num);
    // If packet is good, process it
    else
        thread(crud_requests[packet.data_type()], data).detach();
}

void send_message_to_server(string type, string data){
    Packet packet;
    // Set packet header
    packet.set_seq_num(format_int(seq_number, 2));
    packet.set_msg_id(format_int(msg_id, 3));
    packet.set_data_type(type);

    packet.set_nickname(storage_nick);

    int packets_sent = send_message(mainFD, main_addr, ack_controller, data, packet);

    seq_number = (seq_number + packets_sent) % 100; // Increment sequence number
    msg_id = (msg_id + 1) % 1000; // Increment message id
}

// CRUD functions
void create_request(vector<unsigned char> data){
    // data : 00node100node2
    stringstream ss;
    ss.write((char *)data.data(), data.size());
    string size1(2, 0), size2(2, 0);
    // Reading node1
    ss.read(size1.data(), size1.size());
    string node1(stoi(size1), 0);
    ss.read(node1.data(), node1.size());
    // Reading node2
    ss.read(size2.data(), size2.size());
    string node2(stoi(size2), 0);
    ss.read(node2.data(), node2.size());
    
    if (database.hasRelation(node1, node2)){
        // Send notification of failure: relation(node1->node2) already exists
        send_message_to_server("N", notify("The relation " + node1 + " -> " + node2 + " already exists"));
        return;
    }

    database.addEdge(node1, node2);
    //Send notification of success
    send_message_to_server("N", notify("The relation " + node1 + " -> " + node2 + " was created"));
}

void read_request(vector<unsigned char> data){
    // data : 00node
    stringstream ss;
    ss.write((char *)data.data(), data.size());
    string size(2, 0);
    // Reading node
    ss.read(size.data(), size.size());
    string node(stoi(size), 0);
    ss.read(node.data(), node.size());
    
    if (!database.exists(node)){
        // Send notification of failure: node doesn't exist
        send_message_to_server("N", notify("The node doesn't exist"));
        return;
    }
  
    //Return response to primary server
    vector<string> result = database.getEdgesAsString(node, 1000);
    // If response can fit in 1 packet
    if (result.size() == 1)
    {
        string res = result[0];
        string size_res = format_int(res.size(), 3);
        res = size_res + res;
        // Send response
        send_message_to_server("R", res);
    }
    // Response has to be split in multiple packets
    else
    {
        // to implement .....
    }
}

void update_request(vector<unsigned char> data){
    // data : 00node100node200new2
    stringstream ss;
    ss.write((char *)data.data(), data.size());
    string size1(2, 0), size2(2, 0), size3(2, 0);
    // Reading node1
    ss.read(size1.data(), size1.size());
    string node1(stoi(size1), 0);
    ss.read(node1.data(), node1.size());
    // Reading node2
    ss.read(size2.data(), size2.size());
    string node2(stoi(size2), 0);
    ss.read(node2.data(), node2.size());
    // Reading new2
    ss.read(size3.data(), size3.size());
    string new2(stoi(size3), 0);
    ss.read(new2.data(), new2.size());

    if (!database.exists(node1)){
        // Send notification of failure: node1 doesn't exist
        send_message_to_server("N", notify("The node " + node1 + " doesn't exist"));
        return;
    }

    database.deleteEdge(node1, node2);
    database.addEdge(node1, new2);
    //Send notification of success
    send_message_to_server("N", notify("The relation " + node1 + " -> " + node2 + " was updated to " + node1 + " -> " + new2));
}

void delete_request(vector<unsigned char> data){
    // data : 00node100node2
    stringstream ss;
    ss.write((char *)data.data(), data.size());
    string size1(2, 0), size2(2, 0);
    // Reading node1
    ss.read(size1.data(), size1.size());
    string node1(stoi(size1), 0);
    ss.read(node1.data(), node1.size());
    // Reading node2
    ss.read(size2.data(), size2.size());
    string node2(stoi(size2), 0);
    ss.read(node2.data(), node2.size());
    
    if (!database.exists(node1)){
        // Send notification of failure: node1 doesn't exist
        send_message_to_server("N", notify("The node " + node1 + " doesn't exist"));
        return;
    }

    if (node2 == "*"){ // Delete all relations
        database.deleteNode(node1);
    }
    else { // Delete one relation only
        database.deleteEdge(node1, node2);
    }
    //Send notification of success
    send_message_to_server("N", notify("The relation " + node1 + " -> " + node2 + " was deleted"));
}

void keep_alive(){
    int num;
    string data(1, 0);
    // Send first message to identify this storage addr in main server
    sendto(keep_aliveFD, storage_nick.data(), storage_nick.size(), MSG_CONFIRM, (struct sockaddr *)&keep_alive_addr, sizeof(struct sockaddr));
    while(true){
        num = recvfrom(keep_aliveFD, data.data(), data.size(), MSG_WAITALL, (struct sockaddr *)&keep_alive_addr, (socklen_t *)&addr_len);
        sendto(keep_aliveFD, storage_nick.data(), storage_nick.size(), MSG_CONFIRM, (struct sockaddr *)&keep_alive_addr, sizeof(struct sockaddr));
    }
}
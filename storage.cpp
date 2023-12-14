#include "lib/macros.h"
#include "lib/ack.h"

using namespace std;

int mainFD, keep_aliveFD;
struct sockaddr_in main_addr, keep_alive_addr;
string MAIN_IP = "127.0.0.1";
int seq_number = 0;
int msg_id = 0;
int storage_idx;
int addr_len= sizeof(struct sockaddr_in);

ACK_controller ack_controller;
map<string, set<string>> database; // database["node"]: {nodes that are connected to "node"}
vector<int> storage_ports= {5001, 5002, 5003, 5004};

// CRUD request functions
void create_request(vector<unsigned char> data);
void read_request(vector<unsigned char> data);
void update_request(vector<unsigned char> data);
void delete_request(vector<unsigned char> data);

void processing(Packet packet);
void send_message(string type, string data);
void send_packet(Packet packet); // flag (0=last packet, 1=not last packet)

void keep_alive();
string get_relations(string node);

typedef void (*func_ptr)(vector<unsigned char>);
map<char, func_ptr> crud_requests({
    {'C', &create_request},
    {'R', &read_request},
    {'U', &update_request},
    {'D', &delete_request}
});

int main(int argc, char *argv[]){
    if (argc != 2){
        cout << "Bad number of arguments" << endl;
        return 1;
    }
    storage_idx = atoi(argv[1])%4;
    int port = storage_ports[storage_idx];
    ack_controller = ACK_controller(to_string(storage_idx), mainFD, main_addr);

    int bytes_readed;
    Packet recv_packet;

    if ((mainFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        perror("Storage: socket");
        exit(1);
    }
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

	cout << "UDPServer connected to main server on port " << port << "..." << endl;
    
    while(true){
        recv_packet.clear();
        bytes_readed = recvfrom(mainFD, &recv_packet, sizeof(Packet), MSG_WAITALL, (struct sockaddr *)&main_addr, (socklen_t *)&addr_len);
        cout << "Received " << bytes_readed << " bytes" << endl;
        thread(processing, recv_packet).detach();
    }
}


void processing(Packet packet){
    string seq_num = packet.seq_num();
    string hash = packet.hash();
    vector<unsigned char> data = packet.data();

    // If packet is ACK
    if (packet.type() == "A"){
        ack_controller.process_ack(seq_num);
        return;
    }
    
// If packet is not corrupted send one ACK, else send one more ACK 
    // Calc hash only to data, without header
    bool is_good= (hash == calc_hash(packet.data()))? true : false; 
    ack_controller.replay_ack(seq_num);
    // If packet is corrupted, send second ACK
    if (!is_good) 
        ack_controller.replay_ack(seq_num);
    // If packet is good, process it
    else
        thread(crud_requests[packet.type()[0]], data).detach();
}

void send_message(string type, string data){
    Packet packet;
    packet.set_type(type);
    packet.set_msg_id(format_int(msg_id, 3));
    packet.set_nickname(to_string(storage_idx));

    int packet_data_size = packet.data_size();
    int remaining_size = data.size();

    stringstream ss;
    ss.write((char *)data.data(), data.size());
    string fragment(packet_data_size, 0);
    // Send message in fragmented packets if it's too big
    while (remaining_size > packet_data_size){
        // Send full packets
        ss.read(fragment.data(), packet_data_size);
        remaining_size -= packet_data_size;
        packet.set_data(fragment);
        packet.set_flag("1");
        send_packet(packet);
    }
    // Send last packet
    fragment.resize(remaining_size);
    ss.read(fragment.data(), remaining_size);
    packet.set_data(fragment);
    packet.set_flag("1");
    send_packet(packet);
    msg_id = (msg_id + 1) % 1000; // Increment message id
}

void send_packet(Packet packet){

    packet.set_seq_num(format_int(seq_number, 2));
    packet.set_hash(calc_hash(packet.data()));
    
    // Save packet into Cache if it's necesary to resend
    ack_controller.insert_packet(seq_number, packet);
    sendto(mainFD, &packet, sizeof(Packet), MSG_CONFIRM, (struct sockaddr *)&main_addr, sizeof(struct sockaddr));
    seq_number = (seq_number + 1) % 100; // Increment sequence number
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
    
    if (database[node1].find(node2) != database[node1].end()){
        // Send notification of failure: relation(node1->node2) already exists
        return;
    }

    database[node1].insert(node2);
    //Send notification of success
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
    
    if (database.find(node) == database.end()){
        // Send notification of failure: node doesn't exist
        return;
    }
    //Return response to primary server
    string result = get_relations(node);
    ostringstream size_os;
    size_os << setw(3) << setfill('0') << result.size();
    result = size_os.str() + result;
    send_message("R", result);
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

    if (database.find(node1) == database.end()){
        // Send notification of failure: node1 doesn't exist
        return;
    }

    database[node1].erase(node2);
    database[node1].insert(new2);
    //Send notification of success
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
    
    if (database.find(node1) == database.end()){
        // Send notification of failure: node1 doesn't exist
        return;
    }

    if (node2 == "*"){ // Delete all relations
        database.erase(node1);
    }
    else { // Delete one relation only
        database[node1].erase(node2);
    }
    //Send notification of success
}

void keep_alive(){
    int num;
    string data;
    data = to_string(storage_idx);
    // Send first message to identify this storage in main server
    sendto(keep_aliveFD, data.data(), data.size(), MSG_CONFIRM, (struct sockaddr *)&keep_alive_addr, sizeof(struct sockaddr));
    while(true){
        num = recvfrom(keep_aliveFD, data.data(), data.size(), MSG_WAITALL, (struct sockaddr *)&keep_alive_addr, (socklen_t *)sizeof(keep_alive_addr));
        data = to_string(storage_idx);
        sendto(keep_aliveFD, data.data(), data.size(), MSG_CONFIRM, (struct sockaddr *)&keep_alive_addr, sizeof(struct sockaddr));
    }
}

string get_relations(string node){
    // Return all relations in format: node1,node2,node3...
    string relations;
    set<string> &rel = database[node];
    for (auto it : rel){
        relations += it + ",";
    }
    relations = relations.substr(0, relations.size() - 1);
    return relations;
}
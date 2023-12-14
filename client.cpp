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

void encoding(string buffer);
void decoding(vector<unsigned char> buffer);
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
void send_packet(string type, string data);

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
    int addr_len= sizeof(struct sockaddr_in);
    if ((serverFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1)  ERROR("Socket")

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    if (inet_pton(AF_INET, SERVER_IP.c_str(), &server_addr.sin_addr) == -1)   ERROR("inet_pton")

    cout << "Input your nickname: ";
    getline(cin, nickname);
    cin.clear();
    ostringstream oss;
    oss << setw(2) << setfill('0') << nickname.size();
    nick_size = oss.str();

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
    vector<unsigned char> recv_buffer(SIZE);
    int bytes_readed, addr_len= sizeof(struct sockaddr_in);
    while(true){
        memset(recv_buffer.data(), 0, SIZE);
        bytes_readed = recvfrom(serverFD, recv_buffer.data(), SIZE, 0, (struct sockaddr *)&server_addr, (socklen_t *)&addr_len);
        thread(decoding, recv_buffer).detach();
    }   
}

void decoding(vector<unsigned char> buffer){
    stringstream ss;
    ss.write((char *)buffer.data(), buffer.size());
    
    // ss : seq_num=2|hash=6|type=1|msg_id=3|flag=1|nick_size=2|nickname=<nick_size>|<data>
    string seq_num(2, 0), hash(6, 0), type(1, 0), msg_id(3, 0), flag(1, 0), nick_size(2, 0);
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
    
    // If packet is ack
    if (type == "A"){
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
    // If packet is not corrupted and it´s a CRUD request
    else{
        copy(data.begin(), data.end(), back_inserter(incomplete_message[msg_id]));
        
        // Verify if flag = 1 (incomplete) else (complete)
        if (flag == "0"){
            vector<unsigned char> message = incomplete_message[msg_id];
            incomplete_message.erase(msg_id);
            thread(response_functions[type[0]], message).detach();
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
    string node1, node2;
    getline(ss, node1, ' ');
    getline(ss, node2, '\0');
    ostringstream size1, size2;
    size1 << setw(2) << setfill('0') << node1.size();
    size2 << setw(2) << setfill('0') << node2.size();
    string out_str = size1.str() + node1 + size2.str() + node2;
    send_packet("C", out_str);
}

void read_request(stringstream &ss){
    // ss : node
    string node;
    getline(ss, node, '\0');
    ostringstream size;
    size << setw(2) << setfill('0') << node.size();
    string out_str = size.str() + node + "1"; // Depth = 1
    send_packet("R", out_str);
}

void rread_request(stringstream &ss){
    // ss : depth node
    string depth, node;
    getline(ss, depth, ' ');
    getline(ss, node, '\0');
    ostringstream size;
    size << setw(2) << setfill('0') << node.size();
    string out_str = size.str() + node + depth;
    send_packet("R", out_str);
}

void update_request(stringstream &ss){
    // ss : node1 node2 new2
    string node1, node2, new2;
    getline(ss, node1, ' ');
    getline(ss, node2, ' ');
    getline(ss, new2, '\0');
    ostringstream size1, size2, size3;
    size1 << setw(2) << setfill('0') << node1.size();
    size2 << setw(2) << setfill('0') << node2.size();
    size3 << setw(2) << setfill('0') << new2.size();
    string out_str = size1.str() + node1 + size2.str() + node2 + size3.str() + new2;
    send_packet("U", out_str);
}

void delete_request(stringstream &ss){
    // ss : node1 node2
    string node1, node2;
    getline(ss, node1, ' ');
    getline(ss, node2, '\0');
    ostringstream size1, size2;
    size1 << setw(2) << setfill('0') << node1.size();
    size2 << setw(2) << setfill('0') << node2.size();
    string out_str = size1.str() + node1 + size2.str() + node2;
    send_packet("D", out_str);
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

void send_packet(string type, string data){
    // packet: seq_num=2|hash=6|type=1|msg_id=3|flag=1|nick_size=2|nickname=<nick_size>|<data>
    string seq_number_str = format_int(seq_number, 2);
    string msg_id_str = format_int(msg_id, 3);
    vector<unsigned char> packet(SIZE, '-');
    string hash = calc_hash(data);
    string header = seq_number_str + hash + type + msg_id_str + "0" + nick_size + nickname;
    copy(header.begin(), header.end(), packet.begin());
    copy(data.begin(), data.end(), packet.begin() + header.size());
    packets.insert(seq_number, packet);
    sendto(serverFD, packet.data(), packet.size(), MSG_CONFIRM, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
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
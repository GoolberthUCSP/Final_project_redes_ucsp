#include "macros.h"
#include "ack.h"

using namespace std;

void send_packet(int destinyFD, struct sockaddr_in destiny_addr, ACK_controller &ack_controller, Packet packet);
int send_message(int destinyFD, struct sockaddr_in destiny_addr, ACK_controller &ack_controller, string data, Packet packet);

/*
    Send a message that can be too long to be sent in one packet
    @param destinyFD: file descriptor of the destiny (socket)
    @param destiny_addr: address of the destiny
    @param ack_controller: ACK controller of the destiny
    @param data: data to send
    @param packet: packet to send, with sections SEQ_NUM, MSG_ID, TYPE and NICKNAME filled 
    @return Number of packets sent, update your local seq_number and msg_id
*/
int send_message(int destinyFD, struct sockaddr_in destiny_addr, ACK_controller &ack_controller, string data, Packet packet){

    int packet_data_size = packet.max_data_size();
    int remaining_size = data.size();
    int packets_sent = 0;
    int seq_num = stoi(packet.seq_num());
    stringstream ss;
    ss.write((char *)data.data(), data.size());
    string fragment(packet_data_size, 0);
    // Set packet type, all packets are D
    packet.set_packet_type("D");
    // Set flag to 1 to indicate that the packet is not the last one
    packet.set_flag("1");
    
    // Fragment message in fragmented packets if it's too big
    while (remaining_size > packet_data_size){
        // Send full packets
        ss.read(fragment.data(), packet_data_size);
        remaining_size -= packet_data_size;
        
        packet.set_seq_num(format_int(seq_num, 2));
        packet.set_data(fragment);
        
        send_packet(destinyFD, destiny_addr, ack_controller, packet);
        
        packets_sent++;
        seq_num = (seq_num + 1) % 100; // Increment sequence number
    }
    // Send last packet
    fragment.resize(remaining_size);
    ss.read(fragment.data(), remaining_size);
    // Set flag to 0 to indicate that the packet is the last one
    packet.set_flag("0");
    packet.set_seq_num(format_int(seq_num, 2));
    packet.set_data(fragment);
    
    send_packet(destinyFD, destiny_addr, ack_controller, packet);

    return ++packets_sent;
}

/*
    Send packet to the destiny
    @param destinyFD: file descriptor of the destiny (socket)
    @param destiny_addr: address of the destiny
    @param ack_controller: ACK controller of the destiny
    @param packet: packet to send, with sections SEQ_NUM, MSG_ID, TYPE, NICKNAME and DATA filled
    @return void
*/
void send_packet(int destinyFD, struct sockaddr_in destiny_addr, ACK_controller &ack_controller, Packet packet){
    int seq_num = stoi(packet.seq_num());
    // Calc hash only to data, without header
    packet.set_hash(calc_hash(packet.data<vector<unsigned char>>()));
    // Save packet into Cache if it's necesary to resend
    ack_controller.insert_packet(seq_num, packet);
    int bytes_sent = sendto(destinyFD, &packet, sizeof(Packet), MSG_CONFIRM, (struct sockaddr *)&destiny_addr, sizeof(struct sockaddr));
    
    cout << MSG_SEND(destiny_addr, packet) << endl;
}
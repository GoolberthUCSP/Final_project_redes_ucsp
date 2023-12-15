# ifndef ACK_H
# define ACK_H

#include "macros.h"

using namespace std;

struct ACK_controller{
    // nickname = nickname of the sender
    string nickname;
    int originFD;
    struct sockaddr_in origin_addr;
    Cache packets;
    Packet ack_packet;
    set<string> acks_to_recv;

    ACK_controller() = default;
    ACK_controller(string nickname, int fd, struct sockaddr_in addr) : nickname(nickname), originFD(fd), origin_addr(addr) {
        packets.size = CACHE_SIZE;

        ack_packet.set_hash("000000");
        ack_packet.set_type("A");
        ack_packet.set_msg_id("000");
        ack_packet.set_flag("0");
        ack_packet.set_nickname(nickname);
    }
    void process_ack(string seq_num);
    void replay_ack(string seq_num);
    void resend_packet(string seq_num);
    void insert_packet(int seq_num, Packet packet);
};

void ACK_controller::process_ack(string seq_num){
    // If the ack is not in the set = it is the second ACK, resend the packet
    if (acks_to_recv.find(seq_num) == acks_to_recv.end())
        resend_packet(seq_num);
    else
        acks_to_recv.erase(seq_num);
}

void ACK_controller::replay_ack(string seq_num){
    ack_packet.set_seq_num(seq_num);

    sendto(originFD, &packet, sizeof(Packet), MSG_CONFIRM, (struct sockaddr *)&origin_addr, sizeof(struct sockaddr));
}

void ACK_controller::resend_packet(string seq_num){
    Packet packet = packets.get(stoi(seq_num));
    sendto(originFD, &packet, sizeof(Packet), MSG_CONFIRM, (struct sockaddr *)&origin_addr, sizeof(struct sockaddr));
    // Add ack to the set
    acks_to_recv.insert(seq_num);
    // Add packet to the Cache
    packets.insert(stoi(seq_num), packet);
}

void ACK_controller::insert_packet(int seq_num, Packet packet){
    packets.insert(seq_num, packet);
}

#endif
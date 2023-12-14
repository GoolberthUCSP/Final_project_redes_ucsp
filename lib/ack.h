# ifndef ACK_H
# define ACK_H

#include "macros.h"

using namespace std;

struct ACK_controller{
    string nickname, nick_size;
    int originFD;
    struct sockaddr_in origin_addr;
    Cache packets;
    set<string> acks_to_recv;

    ACK_controller() = default;
    ACK_controller(string nickname, int fd, struct sockaddr_in addr) : nickname(nickname), originFD(fd), origin_addr(addr) {
        ostringstream ss;
        nick_size = to_string(nickname.size());
        packets.size = CACHE_SIZE;
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
    Packet packet;
    string header = seq_num + "000000" + "A" + "000" + "0" + nick_size + nickname;
    packet.set_header(header);
    sendto(originFD, &packet, sizeof(Packet), MSG_CONFIRM, (struct sockaddr *)&origin_addr, sizeof(struct sockaddr));
}

void ACK_controller::resend_packet(string seq_num){
    Packet packet = packets.get(stoi(seq_num));
    packets.insert(stoi(seq_num), packet);
    sendto(originFD, &packet, sizeof(Packet), MSG_CONFIRM, (struct sockaddr *)&origin_addr, sizeof(struct sockaddr));
}

void ACK_controller::insert_packet(int seq_num, Packet packet){
    packets.insert(seq_num, packet);
}

#endif
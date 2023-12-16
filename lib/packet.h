// packet: seq_num=2|hash=6|type=1|msg_id=3|flag=1|nick_size=1|nickname=9|<data>
#ifndef PACKET_H
#define PACKET_H

#define SIZE 1024

#include <iostream>
#include <cstring>
#include <vector>

using namespace std;

class Packet{
    char seq_number[2];
    char hash_value[6];
    char packet_type;
    char message_id[3];
    char packet_flag;
    char nickname_size;
    char nickname_value[9];
    char data_value[SIZE - 23];
public:
    Packet(){ memset(this, '-', SIZE); }
    Packet(const Packet &packet){ memcpy(this, &packet, SIZE); }
    // Getters
    string seq_num(){ return string(seq_number, 2); }
    string hash(){ return string(hash_value, 6); }
    string type(){ return string(&packet_type, 1); }
    string msg_id(){ return string(message_id, 3); }
    string flag(){ return string(&packet_flag, 1); }
    string nick_size(){ return string(&nickname_size, 1); }
    string nickname(){ return string(nickname_value, nickname_size-'0'); }
    vector<unsigned char> data(){ return vector<unsigned char>(data_value, data_value + SIZE - 23); }
    string data_str(){ return string(data_value, SIZE - 23); }
    string header(){ return seq_num() + hash() + type() + msg_id() + flag() + nick_size() + nickname(); }
    int data_size(){ return SIZE - 23; }
    string get_data(){ return string(data_value, 10); }

    // Setters
    void set_seq_num(string seq_num){ copy(seq_num.begin(), seq_num.end(), seq_number); }
    void set_hash(string hash){ copy(hash.begin(), hash.end(), hash_value); }
    void set_type(string type){ packet_type = type[0]; }
    void set_msg_id(string msg_id){ copy(msg_id.begin(), msg_id.end(), message_id); }
    void set_flag(string flag){ packet_flag = flag[0]; }
    void set_nickname(string nick){ 
        if (nick.size() > 9) runtime_error("Nickname too long: " + nick);
        copy(nick.begin(), nick.end(), nickname_value);
        nickname_size = to_string(nick.size())[0];
    }
    void set_data(vector<unsigned char> data){ 
        clear_data();
        copy(data.begin(), data.begin() + data_size(), data_value); 
    }
    void set_data(string data){ 
        clear_data();
        copy(data.begin(), data.begin() + data_size(), data_value); 
    }
    void set_header(string header){ copy(header.begin(), header.end(), seq_number); }

    void clear(){ memset(this, '-', SIZE); }
    void clear_data(){ memset(data_value, '-', data_size()); }
    void print(){
        cout << seq_num() << "|" << hash() << "|" << type() << "|" << msg_id() << "|" << flag() << "|" << nick_size() << "|" << nickname() << "|" << get_data() << endl;
        //cout << "data: " << data_str() << endl;
    }

    void operator = (const Packet& packet){ memcpy(this, &packet, SIZE); }
};

#endif
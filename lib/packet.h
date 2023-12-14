// packet: seq_num=2|hash=6|type=1|msg_id=3|flag=1|nick_size=1|nickname=9|<data>
#ifndef PACKET_H
#define PACKET_H

#define PACKET_SIZE 1024

#include <iostream>
#include <string>
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
    char data_value[PACKET_SIZE - 23];
public:
    Packet(){ memset(this, '-', PACKET_SIZE); }
    // Getters
    string seq_num(){ return string(seq_number, 2); }
    string hash(){ return string(hash_value, 6); }
    string type(){ return string(&packet_type, 1); }
    string msg_id(){ return string(message_id, 3); }
    string flag(){ return string(&packet_flag, 1); }
    string nick_size(){ return string(&nickname_size, 1); }
    string nick(){ return string(nickname_value, nickname_size-'0'); }
    vector<unsigned char> data(){ return vector<unsigned char>(data_value, data_value + PACKET_SIZE - 23); }
    // Setters
    void set_seq_num(string seq_num){ copy(seq_num.begin(), seq_num.end(), seq_number); }
    void set_hash(string hash){ copy(hash.begin(), hash.end(), hash_value); }
    void set_type(string type){ packet_type = type[0]; }
    void set_msg_id(string msg_id){ copy(msg_id.begin(), msg_id.end(), message_id); }
    void set_flag(string flag){ packet_flag = flag[0]; }
    void set_nick(string nick){ 
        if (nick.size() > 9) runtime_error("Nickname too long: " + nick);
        copy(nick.begin(), nick.end(), nickname_value);
        nickname_size = nick.size() + '0';
    }
    void set_data(vector<unsigned char> data){ copy(data.begin(), data.end(), data_value); }
    void print(){
        cout << "seq_num: " << seq_num() << endl;
        cout << "hash: " << hash() << endl;
        cout << "type: " << type() << endl;
        cout << "msg_id: " << msg_id() << endl;
        cout << "flag: " << flag() << endl;
        cout << "nick_size: " << nick_size() << endl;
        cout << "nick: " << nick() << endl;
        cout << "data: " << data().data() << endl;
    }
};

#endif
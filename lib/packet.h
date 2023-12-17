#ifndef PACKET_H
#define PACKET_H

#define SIZE 1024
#define HEADER_SIZE 23
#define DATA_SIZE SIZE - HEADER_SIZE

#include <iostream>
#include <cstring>
#include <vector>

/*
FORMAT OF PACKET
    NETWORK LAYER = header
    char packet_type;
    char seq_number[2];
    char hash_value[6];
    char message_id[3];
    char packet_flag;
    char nickname_size;
    char nickname_value[9];
    APP LAYER = data type + data
    char data_value[DATA_SIZE] = {
        char data_type;
        char data_size[3];
        char data[DATA_SIZE - 4]
    }
*/

using namespace std;

class Packet{
    char header[HEADER_SIZE];
    char data_value[DATA_SIZE];
    void clear_data(){
        memset(data_value + 4, '-', max_data_size());
        memset(data_value + 1, '0', 3);
    }
public:
    Packet(){ 
        memset(header, '0', HEADER_SIZE + 4);
        memset(data_value + 4, '-', DATA_SIZE - 4);
    }
    Packet(const Packet &packet){ 
        memcpy(this, &packet, SIZE); 
    }
    // Getters
    string packet_type(){ 
        return string(&header[0], 1); 
    }
    string seq_num(){
        return string(&header[1], 2);
    }
    string hash(){
        return string(&header[3], 6);
    }
    string msg_id(){
        return string(&header[9], 3);
    }
    string flag(){
        return string(&header[12], 1);
    }
    string nick_size(){
        return string(&header[13], 1);
    }
    string nickname(){
        return string(&header[14], stoi(nick_size()));
    }
    string data_type(){
        return string(&data_value[0], 1);
    }
    string data_size(){
        return string(&data_value[1], 3);
    }
    string head(){
        return packet_type() + "|" + data_type() + "|" + seq_num() + "|" + hash() + "|" + msg_id() + "|" + flag() + "|" + nick_size() + "|" + nickname();
    }
    template<typename T>
    T data(){
        return T(&data_value[4], &data_value[4 + stoi(data_size())]);
    }
    // Setters
    void set_packet_type(string type){
        header[0] = type[0];
    }
    void set_seq_num(string seq_num){
        copy(seq_num.begin(), seq_num.begin() + 2, header + 1);
    }
    void set_hash(string hash){
        copy(hash.begin(), hash.begin() + 6, header + 3);
    }
    void set_msg_id(string msg_id){
        copy(msg_id.begin(), msg_id.begin() + 3, header + 9);
    }
    void set_flag(string flag){
        header[12] = flag[0];
    }
    void set_nickname(string nick){
        if (nick.size() > 9) 
            throw range_error("Nickname too long: " + nick.size());
        copy(nick.begin(), nick.end(), header + 14);
        header[13] = to_string(nick.size())[0];
    }
    void set_data_type(string data_type){
        data_value[0] = data_type[0];
    }
    template <typename T>
    void set_data(T data){
        if (data.size() > max_data_size()) 
            throw range_error("Data too long: " + data.size());
        clear_data();
        copy(data.begin(), data.end(), data_value + 4);
        string data_size = to_string(data.size());
        data_size.insert(0, 3 - data_size.size(), '0');
        copy(data_size.begin(), data_size.begin() + 3, data_value + 1);
    }

    int max_data_size(){
        return DATA_SIZE - 4;
    }
    void clear(){
        memset(header, '0', HEADER_SIZE + 4);
        memset(data_value + 4, '-', DATA_SIZE - 4);
    }
    void operator = (const Packet& packet){ memcpy(this, &packet, SIZE); }
    void print(){
        cout << "Header: " << packet_type() << "|" << seq_num() << "|" << hash() << "|" << msg_id() << "|" << flag() << "|" << nick_size() << "|" << nickname() << endl; 
        cout << "Data  : " << data_type() << "|" << data_size() << "|" << data<string>() << endl;
    }
};

#endif
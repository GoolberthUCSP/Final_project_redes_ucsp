#ifndef LIB_H
#define LIB_H

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

using namespace std;

string calc_hash(vector<unsigned char> packet){
    int sum = 0;
    ostringstream output;
    for (auto it : packet){
        sum += (char)it;
    }
    output << setw(6) << setfill('0') << sum;
    return output.str();
}

string calc_hash(string packet){
    int sum = 0;
    ostringstream output;
    for (auto it : packet){
        sum += it;
    }
    output << setw(6) << setfill('0') << sum;
    return output.str();
}

/*
    Format integer to string
    @param num : number
    @param size : size of string
    @return string
*/
string format_int(int num, int size){
    ostringstream output;
    output << setw(size) << setfill('0') << num;
    return output.str();
}

#endif
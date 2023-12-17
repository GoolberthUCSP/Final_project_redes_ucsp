#ifndef MACROS_H
#define MACROS_H
    #include <sys/ioctl.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <cstdio>
    #include <cstdlib>
    #include <unistd.h>
    #include <errno.h>
    #include <string.h>
    #include <iostream>
    #include <thread>
    #include <map>
    #include <string>
    #include <sstream>
    #include <iomanip>
    #include <vector>
    #include <random>
    #include <utility>
    #include <algorithm>
    #include <thread>
    #include <set>
    #include <mutex>
    #include "cache.h"
    #include "util.h"
    #include "packet.h"
    // SIZE defined in packet.h
    // Timeout in seconds
    #define SEC_TIMEOUT 2
    // Timeout in microseconds
    #define USEC_TIMEOUT 0
    // CACHE_SIZE of packets
    #define ERROR(s) {perror(s); exit(1);}
    // Message recv and send macros
    #define MSG_RECV(packet) "Received packet from " + packet.nickname() + ", with header: " + packet.head()
    #define MSG_SEND(destiny_addr, packet) "Sending packet to " + inet_ntoa(destiny_addr.sin_addr) + ":" + to_string(ntohs(destiny_addr.sin_port)) + ", with header: " + packet.head()
    #define CACHE_SIZE 10
#endif
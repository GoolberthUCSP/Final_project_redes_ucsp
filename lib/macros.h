#ifndef MACRO_H
#define MACRO_H
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
    // SIZE of packet
    #define SIZE 1024
    // Timeout in seconds
    #define SEC_TIMEOUT 3
    // CACHE_SIZE of packets
    #define CACHE_SIZE 10
    #define ERROR(s) {perror(s); exit(1);}
#endif
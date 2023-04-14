#pragma once 
#include <iostream>
#include <mesi.h>
#include <cache/L1_cache.h>
#include <map>
#include <queue>

using namespace std;


struct Ott_entry{
    Msg _msg;
    Block _block;
    int pending_invals;
    bool home_msg_received;
};

class OTT{
public:
    map<unsigned long long, Ott_entry*> table;


    void add_entry();
    void remove_entry();
    void check_entry();
};

class pending_msgs{
public:
    map<unsigned long long, Msg> buffer;
};

class trace_buffer {
public:
    map<unsigned long long, queue<Trace *>> buffer;
};

struct eb_entry{
    Block _block;
    int pending_invals;
};

class evicted_blocks {
public:
    map<unsigned long long, struct eb_entry*> buffer;
};
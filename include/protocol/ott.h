#pragma once 
#include <iostream>
#include <vector>
#include <mesi.h>
#include <cache/L1_cache.h>
#include <map>
#include <queue>
#include <list>

using namespace std;


struct Ott_entry{
    Msg _msg;
    Block _block;
    int pending_invals;
    bool home_msg_received;
};

typedef struct Ott_entry Ott_entry;

Ott_entry* create_ott_entry(Msg* msg, Block* _block, int pi, bool hmr);
void update_ott_entry(L1Cache* l1_cache, unsigned long long addr, bool hmr, int inval_exp);

class OTT{
public:
    map<unsigned long long, Ott_entry*> table;
    list<vector<Ott_entry*>> nackTimer;
    set<unsigned long long> nacke_addrs;

    OTT();
    void add_entry(unsigned long long addr, Ott_entry* entry);
    void remove_entry(unsigned long long addr);
    bool check_entry(unsigned long long addr);
    void add_nacked(unsigned long long addr);
    void decrement_timer();
};

class pending_msgs{
public:
    map<unsigned long long, queue<Msg>> buffer;
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


//add functions for updating vales of ott entries
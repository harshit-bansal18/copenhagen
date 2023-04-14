#pragma once 
#include <iostream>
#include <mesi.h>
#include <cache/L1_cache.h>

using namespace std;


struct Ott_entry{
    Msg _msg;
    Block _block;
    int pending_invals;
};

class OTT{
public:
    map<unsigned long long, Ott_entry*> table;


    void add_entry();
    void remove_entry();
    void check_entry();
};

class bufferl1{
public:
    map<unsigned long long, Msg> buffer;
};

struct buffer_L2_entry{
    Block _block;
    int pending_invals;
};

class bufferl2{
public:
    map<unsigned long long, buffer_L2_entry*> buffer;
};
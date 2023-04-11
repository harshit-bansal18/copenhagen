#pragma once
#include<iostream>
#include<fstream>
#include<vector>
#include<cache/L1_cache.h>
#include<cache/L2_cache.h>
#include<specs.h>
#include<protocol/mesi.h>

using namespace std;

class Simulator {
public:
    Block      *l1_block;
    Block      *l2_block;
    Block      *_victim;

    vector<L1Cache*> l1_caches;
    L2Cache* l2_cache;
    
    Mesi* mesi;

    unsigned long long cycle_counter;
    unsigned long long global_counter_to_process;

    vector<fstream> f_traces;

    Trace* tmp_trace;
    vector<Msg*> tmp_msg1_queue;
    vector<Msg*> tmp_msg2_queue;

    Simulator(string f_name);
    void execute_part2();
    void start_simulator();
    bool end_condition(bool started);
};
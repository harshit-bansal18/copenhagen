#pragma once
#include <set.h>
#include <specs.h>
#include <utils.h>
#include <trace.h>
#include <protocol/mesi.h>

using namespace std;

/*
L1Cache: Private cache
    Sets
    Ways
    State for each cache blo    queue<Msg *> msgs;ck

***********DEFAULT CONFIGURATION*************
    WAYS: 8
    CAPACITY: 32 KB
    REPLACMENT:  LRU
    TYPE: PRIVATE
*******************************************
*/

class Cache {

};

class L1Cache:public Cache {
    // cannot allow direct access to cache blocks
    vector<Set *> sets;
public:
    #ifdef L1_ASSOC
        const unsigned int no_ways = L1_ASSOC;
    #else
        const unsigned int no_ways = 8;
    #endif

    #ifdef L1_CACHE_CAPACITY
        const unsigned long capacity = L1_CACHE_CAPACITY;
    #else
        const unsigned long capacity = 32 KB;
    #endif

    #ifdef BLOCK_SIZE
        const unsigned int block_size = BLOCK_SIZE;
    #else
        const unsigned int block_size = 64;
    #endif

    const unsigned int no_sets = get_sets(no_ways, capacity, block_size);
    
    const unsigned int no_block_bits = get_log2(block_size);
    const unsigned int no_index_bits = get_log2(no_sets);
    const unsigned int no_tag_bits = get_tag_bits(no_block_bits, no_index_bits);

    const int mask = no_sets - 1;
    
    int ID;

    policy repl_policy;

    unsigned long accesses;
    unsigned long misses;
    unsigned long upgrade_misses;

    queue<Trace *> trace_input;
    queue<Msg *> msgs;

    Block *victim;
    L1Cache();
    void lookup(Block *_block);
    void invalidate(Block *_block);
    int invoke_repl_policy(int index);
    void update_repl_params(int index, int way);
    void get_block(unsigned long long addr, Block *_block);
    int get_target_way(int index);
    unsigned long long get_addr(Block *_block);
    void copy(Block *_block);
    bool empty_trace_queue();
    bool empty_msg_queue();
    void set_block_state(int index, int way, state new_state);
    void queue_msg(Msg *_msg);
};
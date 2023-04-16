#pragma once

#include <set.h>
#include <specs.h>
#include <utils.h>
#include <trace.h>
#include <protocol/mesi.h>
#include <cache/L1_cache.h>

using namespace std;

// class Bank {
// public:
//     unsigned int no_ways;
//     unsigned long capacity;
//     unsigned long no_sets;
//     unsigned int block_size;
//     vector<Set *> sets;
//     queue<Msg *> msgs;
//     Block *victim;

//     Bank(unsigned int _ways, unsigned long _cap, unsigned int _b_size);
//     void lookup(Block *_block);
//     void invalidate(Block *_block);
//     int invoke_repl_policy(int index);
//     void update_repl_params(int index, int way);
//     int get_target_way(int index);
//     unsigned long long get_addr(Block *_block);
// };
/*
L2Cache: Shared cache
    Banks
    Sets
    Ways
    State for each cache block

***********DEFAULT CONFIGURATION*************
    WAYS: 16
    CAPACITY: 4 MB
    REPLACMENT:  LRU
    TYPE: SHARED
*******************************************
*/

class L2Cache: public Cache {
    vector<Set*> sets;
public:
    #ifdef L2_ASSOC
        const unsigned int no_ways = L2_ASSOC;
    #else
        const unsigned int no_ways = 8;
    #endif

    #ifdef L2_CACHE_CAPACITY
        const unsigned long capacity = L2_CACHE_CAPACITY;
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

    unsigned long misses;

    const int mask = no_sets -1;
    const int bank_mask = BANKS - 1;
    Block *victim;
    policy repl_policy;
    vector<queue<Msg *>> msg_queues;
    evicted_blocks *eb_buffer;
    L2Cache();
    void lookup(Block *_block);
    void invalidate(Block *_block);
    int invoke_repl_policy(int index);
    void update_repl_params(int index, int way);
    int get_target_way(int index);
    unsigned long long get_addr(Block *_block);
    void get_block(unsigned long long addr, Block *_block);
    void copy(Block *_block);
    bool empty_msg_queues();
    void queue_msg(Msg *_msg, int bank);
    void set_directory_state(state dir_state, int index, int way);
    void set_sharers(vector<int> &sharers, int index, int way);
    void set_owner(int owner, int index, int way);
    void add_sharer(int _sharer, int index, int way);
    void lookup_evicted_blocks(Block *_block);
    void drop_evicted_block(Block *_block);
    // returns pending inv after decrementing the pendinng invalidations
    int dec_pending_inv(Block *_block);
    void insert_evicted_block(Block *_block);
};
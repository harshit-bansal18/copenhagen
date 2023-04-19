#ifndef __CACHE_H__
#define __CACHE_H__

#include <bits/stdc++.h>

class Directory_entry;
class Mesi;


#ifndef __MESI_H__
    #include "protocol/mesi.h"
#endif


#include <utils.h>
#include <specs.h>
#include <replacement.h>

using namespace std;

class Block {
    public:
    unsigned long long addr;
    unsigned long tag;
    unsigned long index;
    unsigned int way;
    bool  valid;
    Directory_entry dir_entry; // for L2 cache
    state block_state; // for L1 cache
    // field for state bit
    Block();
    Block(unsigned long _index, unsigned int _way, unsigned long _tag, unsigned long long _addr);

};

class Set {
    public:
    vector<Block> blocks;
    set<int> invalid_ways;
    struct access_list repl_list;

    Set(int _ways);

};


struct Ott_entry{
    Msg _msg;
    Block _block;
    int pending_invals;
    bool home_msg_received;
    bool invalid;
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

class L1Cache {
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

    OTT *ott;
    pending_msgs *pending_msgs_buffer;
    trace_buffer *miss_trace_buffer;

    // Stats
    vector<unsigned long> num_msgs; 

    L1Cache(int id);
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
    void insert_to_pending(Msg *_msg);
    Ott_entry* find_ott_entry(unsigned long long addr);
};

class L2Cache {
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
    vector<vector<unsigned long >> num_msgs;
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
    void insert_evicted_block(Block *_block, int num_inval);
};

#endif
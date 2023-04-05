#include <set.h>
#include <specs.h>
#include <utils.h>
#include <trace.h>
#include <msg.h>

using namespace std;

class Bank {
    const unsigned int no_ways;
    const unsigned long capacity;
    const unsigned long no_sets;
    vector<Set *> sets;
    queue<Msg *> msgs;
    unordered_map<unsigned long long, int> tag_index;
};
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

class L2Cache {
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

    vector<Bank *> banks;
    policy repl_policy;



    queue<Trace *> trace_input;
    queue<Msg *> msgs;
    
    L2Cache();
};
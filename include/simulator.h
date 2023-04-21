
#include <bits/stdc++.h>


#include <utils.h>
#include <specs.h>
#include <replacement.h>
#include <trace.h>

using namespace std;

extern ofstream debug;

class Directory_entry;
class Mesi;
class Msg;
class Block;
class Set;
class OTT;
class pending_msgs;
class trace_buffer;
class evicted_blocks;
class L1Cache;
class L2Cache;
class Simulator;

typedef enum {
   GET,
   GETX,
   UPGR,
   PUT,
   PUTE,
   PUTX,
   INV,
   SWB,
   WB,
   INV_ACK,
   UPGR_ACK,
   NACK,
   NACKE,
   WB_ACK,

   NUM_MSG_TYPES
}msg_type;

typedef enum {
   MODIFIED,
   EXCLUSIVE,
   SHARED,
   INVALID,
   PSH,
   PDEX,
   UNOWNED,
}state;

class Msg {
public:
   // source
   int cache;
   int id;
   unsigned long long addr;
   msg_type type;
   int expected_invalidations;
   unsigned long long global_id;
};

class Directory_entry{
public:
   std::bitset<8> sharer;
   state curr_state;
   Directory_entry() {
       sharer.reset();
       curr_state = UNOWNED;
   }
};

// Define a state machine for MESI protocol

class Mesi {
public:
   std::vector<L1Cache *> l1_caches;
   L2Cache *l2_cache;
   Block *l1_block;
   Block *l2_block;
   Mesi();
   void process_l1_msg(Msg *_msg, int core);
   void process_l2_msg(Msg *_msg, int bank_id);
   void process_trace(Trace *_trace);

   bool check_ott_invalid(int core, unsigned long long addr);
   void perform_ott_entry_removal(int core);
   void handle_pending_msgs(int core);

   void handle_put_L1(int core, unsigned long long curr_msg_id, int expected_invalidations = 0);
   void handle_putx_L1(int core, unsigned long long curr_msg_id, int expected_invalidations = 0);
   void handle_pute_L1(int core, unsigned long long curr_msg_id, int expected_invalidations = 0);

   void handle_put_L1_inv_ack(int core, state put_state, unsigned long long curr_msg_id);
   void handle_victim_L1(int core, unsigned long long curr_msg_id);
   void handle_get_L1(int core, Msg* _msg);
   void handle_getx_L1(int core, Msg* _msg);
   void handle_UPGR_L1(int core, Msg* _msg, state final_state);
   void handle_INV_L1(int core, Msg* _msg);
   void handle_NACK_L1(int core, Msg *_msg);
   void handle_NACKE_L1(int core, Msg *_msg);
   void handle_INV_ACK_L1(int core, unsigned long long curr_msg_id);
   void handle_UPGR_ACK_L1(int core, unsigned long long curr_msg_id, int expected_invalidations = 0);
   void handle_WB_ACK_L1(int core, unsigned long long curr_msg_id);

   void handle_get_L2(int bank_id, int source_core, unsigned long long curr_msg_id);
   void handle_getx_L2(int bank_id, int source_core, unsigned long long curr_msg_id);
   void handle_upgr_L2(int bank_id, int source_core, unsigned long long curr_msg_id);
   void handle_swb_L2(int bank_id, int source_core, unsigned long long curr_msg_id);
   void handle_wb_L2(int bank_id, int source_core, unsigned long long curr_msg_id);
   void handle_inv_ack_L2(int bank_id, int source_core, unsigned long long curr_msg_id);
   void handle_victim_L2(int bank_id, int source_core, unsigned long long curr_msg_id);
};

class Block {
    public:
    unsigned long long addr;
    unsigned long tag;
    long index;
    int way;
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
    bool empty_pending_msgs(){
        if(buffer.empty() == 0) return true;
        for(auto &p: buffer){
            if(!p.second.empty()) return false;
        }
        return true;
    }
};

class trace_buffer {
public:
    map<unsigned long long, queue<Trace *>> buffer;
    bool empty_trace_buffer(){
        if(buffer.empty() == 0) return true;
        for(auto &p: buffer){
            if(!p.second.empty()) return false;
        }
        return true;
    }
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
    void execute_part2(bool &executed_something);
    void start_simulator();
    bool end_condition();
    void print_stats();
    void print_specs();
    void print_end_states();
};
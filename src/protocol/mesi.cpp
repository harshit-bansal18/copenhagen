#include  <protocol/mesi.h>

Mesi::Mesi() {
    l1_block = new Block();
    l2_block = new Block();
}

static inline int get_home_node(unsigned long long addr, L2Cache *l2_cache) {
    int node;
    addr = addr << l2_cache->no_block_bits;
    node = addr & l2_cache->bank_mask;
    return node;
}

static inline Msg *make_msg_from_L1(int source_core, unsigned long long addr, msg_type _type) {
    Msg *new_msg;
    new_msg = new Msg();
    new_msg->cache = L1;
    new_msg->id = source_core;
    new_msg->type = _type;
    new_msg->addr = addr;
    
    return new_msg;

}

void Mesi::process_trace(Trace *trace_entry) {
    L1Cache *l1_cache;
    int core = trace_entry->thread_id;
    int home_node;
    Msg *new_msg;
    l1_cache = l1_caches[core];
    l1_cache->accesses ++;

    l1_cache->get_block(trace_entry->address, l1_block);
    l1_cache->lookup(l1_block);
    if(l1_block->valid && l1_block->block_state != INVALID) {
        // Block is present but need to check state of block

        // If this is a READ then we really dont care if the block is in S or E or M state
        if (trace_entry->request == READ)
            goto l1_ret;
        
        /*
            From this point forward all the checks will assume that this is a store request
        */

        if (l1_block->block_state == MODIFIED)
            goto l1_ret;
        
        // If the block is in Exclusive state, silently transition to modified state
        if (l1_block->block_state == EXCLUSIVE) {
            l1_cache->set_block_state(l1_block->index, l1_block->way, MODIFIED);
            goto l1_ret;
        }

        // If the block is in shared state, we need to send Upgr request to home node
        if (l1_block->block_state == SHARED) {
            home_node = get_home_node(l1_block->addr, l2_cache);
            new_msg =  make_msg_from_L1(core, l1_block->addr, UPGR);
            l2_cache->queue_msg(new_msg, home_node);
        }

        goto l1_ret;
    }

    // We have a L1 miss. Now we need to request block from home.
    l1_cache->misses++;

    home_node = get_home_node(l1_block->addr, l2_cache);

    if (trace_entry->request == READ)
        new_msg = make_msg_from_L1(core, l1_block->addr, GET);
    else if (trace_entry->request == WRITE)
        new_msg = make_msg_from_L1(core, l1_block->addr, GETX);

    l2_cache->queue_msg(new_msg, home_node);


l1_ret:
    l1_block->block_state = INVALID;
    l1_block->valid = false;

}

// Process the msg in queue of L1 Cache
/****************TODO************/
void Mesi::process_l1_msg(Msg *_msg) {
    
    switch(_msg->type) {
        case PUT:
            break;

        default:
            break;
    }
}
#include  <protocol/mesi.h>
#include <bitset>

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

static inline Msg *make_msg_from_L2(int bank_id, unsigned long long addr, msg_type _type) {
    Msg *new_msg;
    new_msg = new Msg();
    new_msg->cache = L2;
    new_msg->id = bank_id;
    new_msg->type = _type;
    new_msg->addr = addr;
    return new_msg;
}

static inline void handle_put_L1(L1Cache *l1_cache, L2Cache *l2_cache, Block *l1_block, state put_state) {
    
    Block _victim;
    Msg *new_msg;
    int home_node; 

    l1_block->block_state = put_state;
    l1_cache->copy(l1_block);
    l1_cache->update_repl_params(l1_block->index, l1_block->way);
    
    // Some block got evicted
    if (l1_cache->victim) {
        // Copenhegan: Too much copying here.
        _victim = *l1_cache->victim;
        free(l1_cache->victim);
        l1_cache->victim = NULL;
        //
        // There wont be any writeback or msg sent to home node
        if (_victim.block_state == SHARED)
            goto ret;
        // Directory has block in modified state to need to send SWB to notify the directory
        else if (_victim.block_state == EXCLUSIVE || _victim.block_state == MODIFIED) {
            home_node = get_home_node(l1_block->addr, l2_cache);
            new_msg =  make_msg_from_L1(l1_cache->ID, l1_block->addr, SWB);
            l2_cache->queue_msg(new_msg, home_node);
        }
    }

ret:
    return;
}

/*
handle_get_L1:
    Get request to L1 cache only occurs when L1 cache of another core requests the block which is owned by this L1 cache.
    @final_state: final state of the block in this cache
    @requester_cache: Cache pointer of the L1 cache which requested this block
    
*/
static inline void handle_get_L1(L1Cache *l1_cache, L2Cache *l2_cache, L1Cache *requester_cache, Block *l1_block, state final_state) {
    Msg *new_msg;

    l1_cache->lookup(l1_block);
    // If the block is not present in the cache, means evicted already.
    // Case of late intervention
    if (!l1_block->valid) {
        // TODO
        // dont do anything for now
        return;
    }

    // Assumed that block is present in the cache. what a fool I am
    if (final_state == SHARED) {
        l1_cache->set_block_state(l1_block->index, l1_block->way, SHARED);
        new_msg = make_msg_from_L1(l1_cache->ID, l1_block->addr, PUT);
        requester_cache->queue_msg(new_msg);
    }
    else if (final_state == INVALID) {
        l1_cache->invalidate(l1_block);
        new_msg = make_msg_from_L1(l1_cache->ID, l1_block->addr, PUTX);
        requester_cache->queue_msg(new_msg);
    }

}

static inline int get_owner(bitset<CORES> bitvec) {
    return (int) (bitvec.to_ulong());
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
        
        l1_cache->update_repl_params(l1_block->index, l1_block->way);
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
void Mesi::process_l1_msg(Msg *_msg, int core) {
    L1Cache *l1_cache = l1_caches[core];
    Block _victim;
    l1_cache->get_block(_msg->addr, l1_block);
    switch(_msg->type) {
    
    case PUT:
        // copy the block in S state
        handle_put_L1(l1_cache, l2_cache, l1_block, SHARED);
        break;

    case PUTE:
        handle_put_L1(l1_cache, l2_cache, l1_block, EXCLUSIVE);
        break;
    
    case PUTX:
        handle_put_L1(l1_cache, l2_cache, l1_block, MODIFIED);
        break;
    // If this L1 cache is the owner of some block.
    case GET:
        handle_get_L1(l1_cache, l2_cache, l1_caches[_msg->id], l1_block, SHARED);
        break;
    case GETX:
    // q: will the parameter be invalid or modified?
        handle_get_L1(l1_cache, l2_cache, l1_caches[_msg->id], l1_block, INVALID);
        break;
    case UPGR:
        handle_get_L1(l1_cache, l2_cache, l1_caches[_msg->id], l1_block, INVALID);
        break;
    // TODO: Need more hardware support :(
    case ACK:
        break;
    case INV:
        break;
    case NACK:
        break;
    default:
        break;
    }
}

void Mesi::process_l2_msg(Msg *_msg, int bank_id) {
    l2_cache->get_block(_msg->addr, l2_block);
    l2_cache->lookup(l2_block);
    
    
    switch(_msg->type) {
    
    case GET:
        handle_get_L2(l2_cache, l1_caches, l2_block, bank_id, _msg->id);
        break;
    
    case GETX:
        handle_getx_L2(l2_cache, l1_caches, l2_block, bank_id, _msg->id);
        break;
    
    case UPGR:
        handle_upgr_L2(l2_cache, l1_caches, l2_block, bank_id, _msg->id);
        break;

    // TODO: How to handle the PSH state when the block got evicted. It sent out invalidations, SWB expected.
    /*
     One approach-> ignore the SWB, we only care about invalidation acknowledgements
    */
    case SWB:

        break;
    
    /*
        Serious problem here. Now the home node is responsible for serving the block if it is in PSH state.
    */
    case WB:
        break;

    // Invalidation acknowledgements
    case ACK:
        break;
    }
}


/*
    TODO: If the block is not present in L2 Cache, also neet to check the buffer. If it is present there, do not install new one
    from the memory, just send NACK!

*/
static inline void handle_get_L2(L2Cache *l2_cache, vector<L1Cache *> &l1_caches, Block *l2_block, int bank_id, int source_core) {
    Block _victim;
    Msg *new_msg;
    int owner;
    vector<int> sharers;

    // If the block is not present in L2 then install new one from memory !!!!! WRONG
    if (!l2_block->valid) {

        /**** SPECIAL CASE **********/
        l2_cache->lookup_evicted_blocks(l2_block);
        // send NACK
        if (l2_block->valid) {
            new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACK);
            return;    
        }
        /*******************************/
        l2_block->dir_entry.curr_state = UNOWNED;
        l2_cache->copy(l2_block);
        l2_cache->update_repl_params(l2_block->index, l2_block->way);
        if (l2_cache->victim) {
            _victim = *l2_cache->victim;
            free(l2_cache->victim);
            l2_cache->victim = NULL;
            // some block got evicted.
            // send invalidations
            switch(_victim.dir_entry.curr_state) {
            
            // Simply drop the block
            case UNOWNED:
                break;
            
            // Need to send invalidations and put the victim in a buffer to collect invalidation acknowledgements
            case SHARED:
                for(int i = 0; i < CORES; i++) {
                    if (_victim.dir_entry.sharer[i] == 1) {
                        new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                        l1_caches[i]->queue_msg(new_msg);

                    }
                }
                // Put the block in buffer now. Set the num invalidations also.
                break;

            // Get the owner and then invalidate it
            case MODIFIED:
                owner = (int)(_victim.dir_entry.sharer.to_ulong());
                new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                l1_caches[owner]->queue_msg(new_msg);
                // put block in buffer and set num inv
                break;
   
            case PSH:
                for(int i = 0; i < CORES; i++) {
                    if (_victim.dir_entry.sharer[i] == 1) {
                        new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                        l1_caches[i]->queue_msg(new_msg);

                    }
                }
                // Put the block in buffer now. Set the num invalidations also.
                break;
            
            case PDEX:
                owner = (int)(_victim.dir_entry.sharer.to_ulong());
                new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                l1_caches[owner]->queue_msg(new_msg);
                // put block in buffer and set num inv
                break;
                
            
            }
        }
    }

    // Now the block is there
    switch(l2_block->dir_entry.curr_state) {
    
    // Set directory state to M, set the owner, and send the PUTE response
    case UNOWNED:
        l2_cache->set_directory_state(MODIFIED, l2_block->index, l2_block->way);
        l2_cache->set_owner(source_core, l2_block->index, l2_block->way);
        new_msg =  make_msg_from_L2(bank_id, l2_block->addr, PUTE);
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    
    case SHARED:
        l2_cache->add_sharer(source_core, l2_block->index, l2_block->way);
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, PUT);
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    
    // Need to forward request to owner
    case MODIFIED:
        owner = get_owner(l2_block->dir_entry.sharer);
        new_msg = make_msg_from_L1(source_core, l2_block->addr, GET);
        sharers.push_back(owner);
        sharers.push_back(source_core);
        l2_cache->set_directory_state(PSH, l2_block->index, l2_block->way);
        l2_cache->set_sharers(sharers, l2_block->index, l2_block->way);
        l1_caches[owner]->queue_msg(new_msg);
        break;
    
    // Send NACK
    case PSH:
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACK);
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    
    // Send NACK
    case PDEX:
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACK);
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    }

}

static inline void handle_getx_L2(L2Cache *l2_cache, vector<L1Cache *> &l1_caches, Block *l2_block, int bank_id, int source_core) {
    Block _victim;
    Msg *new_msg;
    int owner;
    vector<int> sharers;

    // If the block is not present in L2 then install new one from memory !!!!! WRONG
    if (!l2_block->valid) {

        /**** SPECIAL CASE **********/
        l2_cache->lookup_evicted_blocks(l2_block);
        // send NACK
        if (l2_block->valid) {
            new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACK);
            return;    
        }
        /*******************************/
        l2_block->dir_entry.curr_state = UNOWNED;
        l2_cache->copy(l2_block);
        l2_cache->update_repl_params(l2_block->index, l2_block->way);
        if (l2_cache->victim) {
            _victim = *l2_cache->victim;
            free(l2_cache->victim);
            l2_cache->victim = NULL;
            // some block got evicted.
            // send invalidations
            switch(_victim.dir_entry.curr_state) {
            
            // Simply drop the block
            case UNOWNED:
                break;
            
            // Need to send invalidations and put the victim in a buffer to collect invalidation acknowledgements
            case SHARED:
                for(int i = 0; i < CORES; i++) {
                    if (_victim.dir_entry.sharer[i] == 1) {
                        new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                        l1_caches[i]->queue_msg(new_msg);

                    }
                }
                // Put the block in buffer now. Set the num invalidations also.
                break;

            // Get the owner and then invalidate it
            case MODIFIED:
                owner = (int)(_victim.dir_entry.sharer.to_ulong());
                new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                l1_caches[owner]->queue_msg(new_msg);
                // put block in buffer and set num inv
                break;
   
            case PSH:
                for(int i = 0; i < CORES; i++) {
                    if (_victim.dir_entry.sharer[i] == 1) {
                        new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                        l1_caches[i]->queue_msg(new_msg);

                    }
                }
                // Put the block in buffer now. Set the num invalidations also.
                break;
            
            case PDEX:
                owner = (int)(_victim.dir_entry.sharer.to_ulong());
                new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                l1_caches[owner]->queue_msg(new_msg);
                // put block in buffer and set num inv
                break;
                
            
            }
        }
    }

    /***********SERVING GETX REQUEST NOW**********************/
    // Now the block is there
    switch(l2_block->dir_entry.curr_state) {
    
    // Set directory state to M, set the owner, and send the PUTE response
    case UNOWNED:
        l2_cache->set_directory_state(MODIFIED, l2_block->index, l2_block->way);
        l2_cache->set_owner(source_core, l2_block->index, l2_block->way);
        new_msg =  make_msg_from_L2(bank_id, l2_block->addr, PUTX);
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    
    // Send invalidations to all the sharers with source as L1 requester. Set DIR State to M, set the owner, set PUTX response to requester along with
    // invalidation count
    case SHARED:
        for(int i=0; i < CORES; i++) {
            if(l2_block->dir_entry.sharer[i]) {
                new_msg = make_msg_from_L1(source_core, l2_block->addr, INV);
                l1_caches[i]->queue_msg(new_msg);
            }
        }
        l2_cache->set_owner(source_core, l2_block->index, l2_block->way);
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, PUTX);
        new_msg->expected_invalidations = l2_block->dir_entry.sharer.count();
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    
    // Need to forward request to owner
    case MODIFIED:
        owner = get_owner(l2_block->dir_entry.sharer);
        new_msg = make_msg_from_L1(source_core, l2_block->addr, GETX);
        l2_cache->set_directory_state(PDEX, l2_block->index, l2_block->way);
        l2_cache->set_owner(source_core, l2_block->index, l2_block->way);
        l1_caches[owner]->queue_msg(new_msg);
        break;
    
    // Send NACK
    case PSH:
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACK);
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    
    // Send NACK
    case PDEX:
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACK);
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    }

}

static inline void handle_upgr_L2(L2Cache *l2_cache, vector<L1Cache *> &l1_caches, Block *l2_block, int bank_id, int source_core) {
    Block _victim;
    Msg *new_msg;
    int owner;
    vector<int> sharers;

    // If the block is not present in L2 then install new one from memory !!!!! WRONG
    if (!l2_block->valid) {

        /**** SPECIAL CASE **********/
        l2_cache->lookup_evicted_blocks(l2_block);
        // send NACK
        if (l2_block->valid) {
            new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACK);
            return;    
        }
        /*******************************/
        l2_block->dir_entry.curr_state = UNOWNED;
        l2_cache->copy(l2_block);
        l2_cache->update_repl_params(l2_block->index, l2_block->way);
        if (l2_cache->victim) {
            _victim = *l2_cache->victim;
            free(l2_cache->victim);
            l2_cache->victim = NULL;
            // some block got evicted.
            // send invalidations
            switch(_victim.dir_entry.curr_state) {
            
            // Simply drop the block
            case UNOWNED:
                break;
            
            // Need to send invalidations and put the victim in a buffer to collect invalidation acknowledgements
            case SHARED:
                for(int i = 0; i < CORES; i++) {
                    if (_victim.dir_entry.sharer[i] == 1) {
                        new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                        l1_caches[i]->queue_msg(new_msg);

                    }
                }
                // Put the block in buffer now. Set the num invalidations also.
                break;

            // Get the owner and then invalidate it
            case MODIFIED:
                owner = (int)(_victim.dir_entry.sharer.to_ulong());
                new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                l1_caches[owner]->queue_msg(new_msg);
                // put block in buffer and set num inv
                break;
   
            case PSH:
                for(int i = 0; i < CORES; i++) {
                    if (_victim.dir_entry.sharer[i] == 1) {
                        new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                        l1_caches[i]->queue_msg(new_msg);

                    }
                }
                // Put the block in buffer now. Set the num invalidations also.
                break;
            
            case PDEX:
                owner = (int)(_victim.dir_entry.sharer.to_ulong());
                new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                l1_caches[owner]->queue_msg(new_msg);
                // put block in buffer and set num inv
                break;
                
            
            }
        }
    }

    /***********SERVING UPGR REQUEST NOW**********************/
    // Now the block is there
    switch(l2_block->dir_entry.curr_state) {
    
    // Treat UPGR request as GETX.
    case UNOWNED:
        l2_cache->set_directory_state(MODIFIED, l2_block->index, l2_block->way);
        l2_cache->set_owner(source_core, l2_block->index, l2_block->way);
        new_msg =  make_msg_from_L2(bank_id, l2_block->addr, PUTX);
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    
    // Send invalidations to all the sharers except the requester with source as L1 requester. Set DIR State to M, set the owner, set PUTX response to requester along with
    // invalidation count
    case SHARED:
        for(int i=0; i < CORES; i++) {
            if (i == source_core) continue;
            if(l2_block->dir_entry.sharer[i]) {
                new_msg = make_msg_from_L1(source_core, l2_block->addr, INV);
                l1_caches[i]->queue_msg(new_msg);
            }
        }
        l2_cache->set_owner(source_core, l2_block->index, l2_block->way);
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, PUTX);
        new_msg->expected_invalidations = l2_block->dir_entry.sharer.count() - 1;
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    
    // Need to forward request to owner
    case MODIFIED:
        owner = get_owner(l2_block->dir_entry.sharer);
        new_msg = make_msg_from_L1(source_core, l2_block->addr, UPGR);
        l2_cache->set_directory_state(PDEX, l2_block->index, l2_block->way);
        l2_cache->set_owner(source_core, l2_block->index, l2_block->way);
        l1_caches[owner]->queue_msg(new_msg);
        break;
    
    // Send NACK
    case PSH:
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACK);
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    
    // Send NACK
    case PDEX:
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACK);
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    }

}


/*
TODO: still pending
*/
static inline void handle_swb_L2(L2Cache *l2_cache, vector<L1Cache *> &l1_caches, Block *l2_block, int bank_id, int source_core) {
    Block _victim;
    Msg *new_msg;
    int owner;
    vector<int> sharers;

    // If the block is not present in L2 then install new one from memory !!!!! WRONG
    if (!l2_block->valid) {

        /**** SPECIAL CASE **********/
        l2_cache->lookup_evicted_blocks(l2_block);
        // send NACK
        if (l2_block->valid) {
            // new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACK);
            // depends on the state of block
            // should only be either PSH/PDEX
            // ignore it in both cases, preferably change the state to S/M as appropriate ???????

            return;    
        }
        /*******************************/
        l2_block->dir_entry.curr_state = UNOWNED;
        l2_cache->copy(l2_block);
        l2_cache->update_repl_params(l2_block->index, l2_block->way);
        if (l2_cache->victim) {
            _victim = *l2_cache->victim;
            free(l2_cache->victim);
            l2_cache->victim = NULL;
            // some block got evicted.
            // send invalidations
            switch(_victim.dir_entry.curr_state) {
            
            // Simply drop the block
            case UNOWNED:
                break;
            
            // Need to send invalidations and put the victim in a buffer to collect invalidation acknowledgements
            case SHARED:
                for(int i = 0; i < CORES; i++) {
                    if (_victim.dir_entry.sharer[i] == 1) {
                        new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                        l1_caches[i]->queue_msg(new_msg);

                    }
                }
                // Put the block in buffer now. Set the num invalidations also.
                break;

            // Get the owner and then invalidate it
            case MODIFIED:
                owner = (int)(_victim.dir_entry.sharer.to_ulong());
                new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                l1_caches[owner]->queue_msg(new_msg);
                // put block in buffer and set num inv
                break;
   
            case PSH:
                for(int i = 0; i < CORES; i++) {
                    if (_victim.dir_entry.sharer[i] == 1) {
                        new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                        l1_caches[i]->queue_msg(new_msg);

                    }
                }
                // Put the block in buffer now. Set the num invalidations also.
                break;
            
            case PDEX:
                owner = (int)(_victim.dir_entry.sharer.to_ulong());
                new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
                l1_caches[owner]->queue_msg(new_msg);
                // put block in buffer and set num inv
                break;
                
            
            }
        }
    }

    /***********SERVING UPGR REQUEST NOW**********************/
    // Now the block is there
    switch(l2_block->dir_entry.curr_state) {
    
    // Treat UPGR request as GETX.
    case UNOWNED:
        l2_cache->set_directory_state(MODIFIED, l2_block->index, l2_block->way);
        l2_cache->set_owner(source_core, l2_block->index, l2_block->way);
        new_msg =  make_msg_from_L2(bank_id, l2_block->addr, PUTX);
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    
    // Send invalidations to all the sharers except the requester with source as L1 requester. Set DIR State to M, set the owner, set PUTX response to requester along with
    // invalidation count
    case SHARED:
        for(int i=0; i < CORES; i++) {
            if (i == source_core) continue;
            if(l2_block->dir_entry.sharer[i]) {
                new_msg = make_msg_from_L1(source_core, l2_block->addr, INV);
                l1_caches[i]->queue_msg(new_msg);
            }
        }
        l2_cache->set_owner(source_core, l2_block->index, l2_block->way);
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, PUTX);
        new_msg->expected_invalidations = l2_block->dir_entry.sharer.count() - 1;
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    
    // Need to forward request to owner
    case MODIFIED:
        owner = get_owner(l2_block->dir_entry.sharer);
        new_msg = make_msg_from_L1(source_core, l2_block->addr, UPGR);
        l2_cache->set_directory_state(PDEX, l2_block->index, l2_block->way);
        l2_cache->set_owner(source_core, l2_block->index, l2_block->way);
        l1_caches[owner]->queue_msg(new_msg);
        break;
    
    // Send NACK
    case PSH:
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACK);
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    
    // Send NACK
    case PDEX:
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACK);
        l1_caches[source_core]->queue_msg(new_msg);
        break;
    }

}

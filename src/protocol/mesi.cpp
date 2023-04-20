#include <stdlib.h>
#include <bitset>
#include <simulator.h>

using namespace std;

extern map<msg_type, string> msg_names;
extern map<state, string> state_names;

Mesi::Mesi() {
    l1_block = new Block();
    l2_block = new Block();
}

static inline int get_home_node(unsigned long long addr, L2Cache *l2_cache) {
    int node;
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

bool Mesi::check_ott_invalid(int core, unsigned long long addr){
    L1Cache *l1_cache = l1_caches[core];
    Ott_entry* ott_entry = l1_cache->ott->table[addr];
    return ott_entry->invalid;
}

void Mesi::perform_ott_entry_removal(int core){
    int home_node;
    Msg *new_msg;
    L1Cache *l1_cache = l1_caches[core];
    Block* temp_l1_block;
    state curr_state = l1_block->block_state;
    log("curr_state " << state_names[curr_state]);
    Ott_entry* new_ott_entry;

    if(curr_state == MODIFIED){
        handle_pending_msgs(core);

        l1_caches[core]->miss_trace_buffer->buffer.erase(l1_block->addr);
        l1_cache->ott->remove_entry(l1_block->addr);
    }
    else if(curr_state == EXCLUSIVE){
        bool modified  = false;
        queue<Trace*>& tr_queue = l1_cache->miss_trace_buffer->buffer[l1_block->addr];
        while(!tr_queue.empty()){
            if(tr_queue.front()->request == 'w' && !modified){
                l1_cache->set_block_state(l1_block->index, l1_block->way, MODIFIED);
                modified = true;
            }
            tr_queue.pop();
        }
        log("Before handling msgs");
        handle_pending_msgs(core);

        l1_caches[core]->miss_trace_buffer->buffer.erase(l1_block->addr);
        l1_caches[core]->ott->remove_entry(l1_block->addr);

        
    }
    else if(curr_state == SHARED){
        queue<Trace*>& tr_queue = l1_caches[core]->miss_trace_buffer->buffer[l1_block->addr];
        
        while(!tr_queue.empty()){
            if(tr_queue.front()->request == 'w'){
                l1_cache->upgrade_misses++;
                home_node = get_home_node(l1_block->addr, l2_cache);
                new_msg =  make_msg_from_L1(core, l1_block->addr, UPGR);
                l2_cache->queue_msg(new_msg, home_node);
                l1_cache->ott->table[l1_block->addr]->_msg.type = UPGR;
                l1_cache->ott->table[l1_block->addr]->invalid = false;
                tr_queue.pop();
                return;
            }
            tr_queue.pop();
        }
        l1_caches[core]->miss_trace_buffer->buffer.erase(l1_block->addr);
        l1_cache->ott->remove_entry(l1_block->addr);

    }
    else{
        queue<Trace*>& tr_queue = l1_caches[core]->miss_trace_buffer->buffer[l1_block->addr];
        
        while(!tr_queue.empty()){
            process_trace(tr_queue.front());
            break;
        }
        while(!tr_queue.empty()){
            l1_caches[core]->miss_trace_buffer->buffer[temp_l1_block->addr].push(tr_queue.front());
            tr_queue.pop();
        }
        handle_pending_msgs(core);

        l1_caches[core]->miss_trace_buffer->buffer.erase(l1_block->addr);
        l1_cache->ott->remove_entry(l1_block->addr);
    }
}

//ALER: To check this if needed or not
void Mesi::handle_pending_msgs(int core){
    int home_node;
    Msg *new_msg;
    L1Cache *l1_cache = l1_caches[core];
    home_node = get_home_node(l1_block->addr, l2_cache);
    Ott_entry *ott_entry = l1_cache->find_ott_entry(l1_block->addr);
    if(ott_entry != nullptr){
        queue<Msg>& pending_queue =  l1_cache->pending_msgs_buffer->buffer[l1_block->addr];
        if(ott_entry->_msg.type == GET || ott_entry->_msg.type == GETX || ott_entry->_msg.type == UPGR){
            log("Pending queue size: " << pending_queue.size());
            assert(pending_queue.size()  <= 1);
            while(!pending_queue.empty()){
                Msg temp_msg = pending_queue.front();
                pending_queue.pop();

                assert(temp_msg.type != INV);
                // DEAD CODE
                if(temp_msg.type == INV){
                    l1_cache->lookup(l1_block);
                    l1_cache->invalidate(l1_block);

                    new_msg = make_msg_from_L1(core, l1_block->addr, INV_ACK);
                    l1_caches[temp_msg.id]->queue_msg(new_msg);
                }

                switch(temp_msg.type) {
                    case GET:
                        l1_cache->set_block_state(l1_block->index, l1_block->way, SHARED);
                        new_msg = make_msg_from_L1(core, l1_block->addr, PUT);
                        assert(temp_msg.cache == L1);
                        l1_caches[temp_msg.id]->queue_msg(new_msg);
                        new_msg = make_msg_from_L1(core, l1_block->addr, SWB);
                        l2_cache->queue_msg(new_msg, get_home_node(l1_block->addr, l2_cache));
                        break;
                    case GETX:
                        l1_cache->invalidate(l1_block);
                        new_msg = make_msg_from_L1(core, l1_block->addr, PUTX);
                        assert(temp_msg.cache == L1);
                        l1_caches[temp_msg.id]->queue_msg(new_msg);
                        new_msg = make_msg_from_L1(core, l1_block->addr, SWB);
                        l2_cache->queue_msg(new_msg, get_home_node(l1_block->addr, l2_cache));
                    default:
                        log("received msg type " << msg_names[temp_msg.type]);
                        throw_error("wrong msg type in pending queue on core %d\n", core);
                }
//                assert(temp_msg.type == GET);
//                assert(temp_msg.type == GETX);
            }
        }

    }
}

void Mesi::handle_victim_L1(int core){
    Block _victim;
    int home_node;
    Msg *new_msg;
    L1Cache *l1_cache = l1_caches[core];
    
    if (l1_cache->victim) {
        _victim = *l1_cache->victim;
        delete l1_cache->victim;
        l1_cache->victim = NULL;

        if(l1_cache->ott->check_entry(_victim.addr)){
            assert(_victim.block_state == SHARED);
            //set ott invalid to true
            l1_cache->ott->table[_victim.addr]->invalid = true;
        }
        
        // Copenhegan: Too much copying here.
//        _victim = *l1_cache->victim;
//        delete l1_caches[core]->victim;
//        l1_cache->victim = NULL;
        //
        // There wont be any writeback or msg sent to home node
        if (_victim.block_state == SHARED)
            goto ret;
        // Directory has block in modified state to need to send WB to notify the directory
        else if (_victim.block_state == EXCLUSIVE || _victim.block_state == MODIFIED) {
            home_node = get_home_node(l1_block->addr, l2_cache);
            new_msg =  make_msg_from_L1(l1_cache->ID, l1_block->addr, WB);
            //Done: create OTT entry
            create_ott_entry(new_msg, &_victim, 0, false);
            l2_cache->queue_msg(new_msg, home_node);
        }
    }
ret:
    return;
}


void Mesi::handle_put_L1(int core, int expected_invalidations) {
    Block _victim;
    Msg *new_msg;
    int home_node; 

    assert(l1_caches[core]->ott->table[l1_block->addr]->invalid == false);

    l1_block->block_state = SHARED;
    l1_caches[core]->copy(l1_block);
    log("after copy");
    l1_caches[core]->update_repl_params(l1_block->index, l1_block->way);
    log("after update_repl");
    // Some block got evicted
    handle_victim_L1(core);
    log("handle_victim_L1 done");
    perform_ott_entry_removal(core);
    log("perform ott done");
ret:
    return;
}


void Mesi::handle_pute_L1(int core, int expected_invalidations) {
//    log("Handle pute l1 started");
    Block _victim;
    Msg *new_msg;
    int home_node; 
    log("Before assertion, address: " << l1_block->addr << " ,core: " << core);
    assert(l1_caches[core]->ott->check_entry(l1_block->addr));
    //Here the ott doesn't have the entry? why
    assert(l1_caches[core]->ott->table[l1_block->addr]->invalid == false);

    log("before setting block state");
    l1_block->block_state = EXCLUSIVE;
    log("Before copy core: " << core);
    l1_caches[core]->copy(l1_block);
    l1_caches[core]->update_repl_params(l1_block->index, l1_block->way);
    log("Before victim");
    // Some block got evicted
    handle_victim_L1(core);

    perform_ott_entry_removal(core);
    log("Ott entry removed");
ret:
    return;
}


void Mesi::handle_putx_L1(int core, int expected_invalidations) {
    Block _victim;
    Msg *new_msg;
    int home_node; 
    Ott_entry *ottentry = l1_caches[core]->find_ott_entry(l1_block->addr);
    if (ottentry != nullptr) {
        if (ottentry->invalid) {
            switch(ottentry->_msg.type){
                // PUTX response to UPGR -> copy the new block
                case UPGR:
                    assert(expected_invalidations == 0);
                default:
                    throw_error("%s: PUTX responded to wrong type in invalid ott\n", __func__);
            }
        }
    }
    // assert(l1_caches[core]->ott->table[l1_block->addr]->invalid == false);

    update_ott_entry(l1_caches[core], l1_block->addr, true, expected_invalidations);
    //if invals become zero already
    if(l1_caches[core]->ott->table[l1_block->addr]->pending_invals != 0)
        goto ret;

    l1_block->block_state = MODIFIED;
    l1_caches[core]->copy(l1_block);
    l1_caches[core]->update_repl_params(l1_block->index, l1_block->way);
    
    // Some block got evicted
    handle_victim_L1(core);

    perform_ott_entry_removal(core);

ret:
    return;
}


void Mesi::handle_put_L1_inv_ack(int core, state put_state) {
    Block _victim;
    Msg *new_msg;
    int home_node; 

    // assert(l1_caches[core]->ott->table[l1_block->addr]->invalid == false);

    l1_block->block_state = put_state;
    l1_caches[core]->copy(l1_block);
    l1_caches[core]->update_repl_params(l1_block->index, l1_block->way);
    
    handle_victim_L1(core);

    perform_ott_entry_removal(core);
}


/*
handle_get_L1:
    Get request to L1 cache only occurs when L1 cache of another core requests the block which is owned by this L1 cache.
    @final_state: final state of the block in this cache
    @requester_cache: Cache pointer of the L1 cache which requested this block
    
*/
void Mesi::handle_get_L1(int core, Msg* _msg) {
    Msg *new_msg;
    int requester_id = _msg->id;
    L1Cache *l1_cache = l1_caches[core];

    Ott_entry *ott_entry = l1_cache->find_ott_entry(_msg->addr);
    if(ott_entry != nullptr){

        //send the inval request to pending requests
        assert(ott_entry->invalid == false);
        
        switch(ott_entry->_msg.type) {
            // GET is possible when this core is the only sharer and home send PUTE response
            case GET:
            case GETX:
            case UPGR:
                l1_caches[core]->insert_to_pending(_msg);
                break;
            // ignore the GET Request? Will be served by home node.
            case WB:
                break;  
            
            default:
                log("Sender: Level: " << _msg->cache << " id: " << _msg->id);
                log( "core " << core << " ott entry msg type: " << msg_names[ott_entry->_msg.type]);
                throw_error("%s: ott entry type invalid\n", __func__);
        }

        return;   
    }

    l1_cache->lookup(l1_block);
    assert(l1_block->valid);
    switch(l1_block->block_state) {
        case MODIFIED:
        case EXCLUSIVE:
            l1_cache->set_block_state(l1_block->index, l1_block->way, SHARED);
            new_msg = make_msg_from_L1(core, l1_block->addr, SWB);
            l2_cache->queue_msg(new_msg, get_home_node(l1_block->addr, l2_cache));
            new_msg = make_msg_from_L1(core, l1_block->addr, PUT);
            l1_caches[requester_id]->queue_msg(new_msg);
            break;
        default:
            throw_error("GET request received in state %s\n", state_names[l1_block->block_state].c_str());
    }

//    // Assumed that block is present in the cache. what a fool I am
//    l1_caches[core]->set_block_state(l1_block->index, l1_block->way, SHARED);
//    new_msg = make_msg_from_L1(core, l1_block->addr, PUT);
//    l1_caches[requester_id]->queue_msg(new_msg);
    

}

void Mesi::handle_getx_L1(int core, Msg *_msg){
    Msg *new_msg;
    int requester_id = _msg->id;
    L1Cache *l1_cache = l1_caches[core];
    
    Ott_entry *ott_entry = l1_cache->find_ott_entry(_msg->addr);
    if(ott_entry != nullptr){
        
        assert(ott_entry->invalid == false);
        
        switch(ott_entry->_msg.type) {
            case GET:
            case GETX:
            case UPGR:
                l1_caches[core]->insert_to_pending(_msg);
                break;
            // ignore the GET Request? Will be served by home node.
            case WB:
                break;
            
            default:
                throw_error("%s: ott entry type invalid\n", __func__);
        }

        return;   
    }

    l1_cache->lookup(l1_block);
    assert(l1_block->valid);
    switch(l1_block->block_state) {
        case MODIFIED:
        case EXCLUSIVE:
            l1_cache->invalidate(l1_block);
            new_msg = make_msg_from_L1(core, l1_block->addr, SWB);
            l2_cache->queue_msg(new_msg, get_home_node(l1_block->addr, l2_cache));
            new_msg = make_msg_from_L1(core, l1_block->addr, PUTX);
            l1_caches[requester_id]->queue_msg(new_msg);
            break;
        default:
            throw_error("GET request received in state %s\n", state_names[l1_block->block_state].c_str());
    }
//    // Assumed that block is present in the cache. what a fool I am
//    l1_caches[core]->invalidate(l1_block);
//    new_msg = make_msg_from_L1(l1_caches[core]->ID, l1_block->addr, PUTX);
//    l1_caches[requester_id]->queue_msg(new_msg);
}

/*****************************DANGER******************************************/
//ALERT:: THIS FUNCTION SHOULD NEVER BE CALLED
void Mesi::handle_UPGR_L1(int core, Msg* _msg, state final_state){
    //similar to L1
    Msg *new_msg, *new_msg_1, *new_msg_2;
    int requester_id = _msg->id;

    Ott_entry *ott_entry = l1_caches[core]->find_ott_entry(_msg->addr);
    if(ott_entry != nullptr){
        //send the inval request to pending requests
        if(ott_entry->_msg.type == UPGR || ott_entry->_msg.type == GETX){
            //send home msg to roll_back
            new_msg_1 = make_msg_from_L1(l1_caches[core]->ID, _msg->addr, NACK); /// ROLL_BACK
            l2_cache->queue_msg(new_msg_1, get_home_node(_msg->addr, l2_cache));
            //send nack to L2;
            new_msg_2 = make_msg_from_L1(l1_caches[core]->ID, _msg->addr, NACK);
            l1_caches[requester_id]->queue_msg(new_msg_2);
        }
    }

    l1_caches[core]->lookup(l1_block);
    // If the block is not present in the cache, means evicted already.
    // Case of late intervention[
    if (!l1_block->valid) {
        // dont do anything, simply ignore.
        return;
    }


    // Assumed that block is present in the cache. what a fool I am
    if (final_state == SHARED) {
        l1_caches[core]->set_block_state(l1_block->index, l1_block->way, SHARED);
        new_msg = make_msg_from_L1(l1_caches[core]->ID, l1_block->addr, PUT);
        l1_caches[requester_id]->queue_msg(new_msg);
    }
    else if (final_state == INVALID) {
        l1_caches[core]->invalidate(l1_block);
        new_msg = make_msg_from_L1(l1_caches[core]->ID, l1_block->addr, PUTX);
        l1_caches[requester_id]->queue_msg(new_msg);
    }

}
/*******************************************************************/


// UPDATE inv to send inv ack and set invalid to ott entry if present
// ?? what if ott entry is already invalid ??
void Mesi::handle_INV_L1(int core, Msg* _msg){
    L1Cache *l1_cache = l1_caches[core];
    int source_id = _msg->id;
    int cache_type = _msg->cache;
    Msg *new_msg;
    bool to_perform = true;
    bool ignore_block_state_change = false;
    //Check OTT entries to see if there is any for the current block
    // for normal requests we don't need to do anything
    Ott_entry *ott_entry = l1_cache->find_ott_entry(_msg->addr);
    if(ott_entry != nullptr){
        if (ott_entry->invalid) {
            switch(ott_entry->_msg.type){
                case GET:
                case GETX:
                    throw_error("%s: ott invalid on GET/GETX\n", __func__);
                    break;
                case UPGR:
                    ignore_block_state_change = true;
                    break;
                case WB:
                    throw_error("%s: second invalidation on WB request\n", __func__);
                default:
                    break;
            }
        }
        else {
            switch(ott_entry->_msg.type){
                case GET:
                case GETX:
                    ignore_block_state_change = true;
                    break;
                case UPGR:
                    ott_entry->invalid = true;
                    ignore_block_state_change = ott_entry->invalid;
                    break;
                case WB:
                    ott_entry->invalid = true;
                    to_perform = false;
                default:
                    break;
            }
        }
        l1_cache->ott->table[_msg->addr]->invalid = true;
    }
    
    if(to_perform){
        if(!ignore_block_state_change){
            l1_cache->lookup(l1_block);
            assert(l1_block->valid);
            l1_cache->invalidate(l1_block);
        }
        new_msg = make_msg_from_L1(l1_cache->ID, l1_block->addr, INV_ACK);

        if(cache_type == L1)
                l1_caches[source_id]->queue_msg(new_msg);
        else
            l2_cache->queue_msg(new_msg, source_id);
    }
}

//done
void Mesi::handle_NACK_L1(int core, Msg* _msg){
    Ott_entry *ott_entry = l1_caches[core]->find_ott_entry(_msg->addr);
    if(ott_entry != nullptr){
        if (ott_entry->invalid) {

            switch(ott_entry->_msg.type){
                // Change UPGR to GETX now
                case UPGR:
                    ott_entry->_msg.type = GETX;
                    break;
                default:
                    throw_error("%s: ott entry invalid\n", __func__);
                    break;
            }
        }
    }

    l1_caches[core]->ott->add_nacked(l1_block->addr);
}


// NACKE request will 
void Mesi::handle_NACKE_L1(int core, Msg *_msg){
    //TODO: handle pending msgs

    Ott_entry *ott_entry = l1_caches[core]->find_ott_entry(_msg->addr);
    if(ott_entry != nullptr){
        if (ott_entry->invalid) {

            switch(ott_entry->_msg.type){
                // Change UPGR to GETX now
                case UPGR:
                    ott_entry->_msg.type = GETX;
                    break;
                default:
                    throw_error("%s: ott entry invalid\n", __func__);
                    break;
            }
        }
    }
    
    l1_caches[core]->ott->add_nacked(l1_block->addr);
    l1_caches[core]->ott->nacke_addrs.insert(l1_block->addr);
}


void Mesi::handle_INV_ACK_L1(int core){
    l1_caches[core]->ott->table[l1_block->addr]->pending_invals--;

    //just copy the block the the l1 cache and remove the ott entry;
    if(l1_caches[core]->ott->table[l1_block->addr]->pending_invals == 0){
        handle_put_L1_inv_ack(core, MODIFIED);
//        perform_ott_entry_removal(core);
    }
}

/*
    Lookup the entry in OTT. 
*/
void Mesi::handle_WB_ACK_L1(int core) {
    //Pending finish ott entry removal
    // Ott_entry* ott_entry = l1_caches[core]->find_ott_entry(l1_block->addr);
    l1_caches[core]->ott->remove_entry(l1_block->addr);

}

void Mesi::handle_UPGR_ACK_L1(int core, int expected_invalidations){
    // Check if ott entry in invalid
    // if yes, then nack it
    // else putx it


    Block _victim;
    Msg *new_msg;
    int home_node; 
    L1Cache *l1_cache = l1_caches[core];
    Ott_entry* ott_entry = l1_cache->ott->table[l1_block->addr];

    update_ott_entry(l1_caches[core], l1_block->addr, true, expected_invalidations);

    if(l1_caches[core]->ott->table[l1_block->addr]->pending_invals != 0)
        goto ret;


    if(ott_entry->invalid){
        l1_block->block_state = MODIFIED;
        l1_caches[core]->copy(l1_block);
        l1_caches[core]->update_repl_params(l1_block->index, l1_block->way);
        handle_victim_L1(core);
    }
    else{
        //do similar to putx;
        l1_caches[core]->set_block_state(l1_block->index, l1_block->way, MODIFIED);
    }
    
    perform_ott_entry_removal(core);
ret:
    return;
}


static inline int get_owner(bitset<CORES> bitvec) {
    return (int) (bitvec.to_ulong());
}



////////////
////////Main processing
///////////


void Mesi::process_trace(Trace *trace_entry) {
    L1Cache *l1_cache;
//    log("Thread id: " << trace_entry->thread_id << ", Address: " << trace_entry->address << ", global count: " << trace_entry->global_id);
//    log((trace_entry->address << 6));
    int core = trace_entry->thread_id;
    int home_node;
    unsigned long long shifted_addr;
    Ott_entry* new_ott_entry;
    Msg *new_msg;
    l1_cache = l1_caches[core];
    l1_cache->accesses ++;
    shifted_addr = trace_entry->address;
    l1_cache->get_block(shifted_addr, l1_block);

    //check ott entry
    if(l1_cache->ott->check_entry(shifted_addr)){
//        log("ott entry present");
        l1_cache->miss_trace_buffer->buffer[shifted_addr].push(trace_entry);
        goto l1_ret;
    }

    l1_cache->lookup(l1_block);
    if(l1_block->valid){
        l1_cache->update_repl_params(l1_block->index, l1_block->way);
        if(trace_entry->request == 'r')
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
        if (l1_block->block_state == SHARED) {
            l1_cache->upgrade_misses++;
            home_node = get_home_node(l1_block->addr, l2_cache);
            new_msg =  make_msg_from_L1(core, l1_block->addr, UPGR);
            l2_cache->queue_msg(new_msg, home_node);
            
            //Create new OTT entry
            Ott_entry* new_ott_entry = create_ott_entry(new_msg, l1_block, 0, false);
            l1_cache->ott->add_entry(l1_block->addr, new_ott_entry);                        

        }
        goto l1_ret;
    }
//    log("l1 miss");
    // We have a L1 miss. Now we need to request block from home.
    l1_cache->misses++;

    home_node = get_home_node(l1_block->addr, l2_cache);

    if (trace_entry->request == READ)
        new_msg = make_msg_from_L1(core, l1_block->addr, GET);
    else if (trace_entry->request == WRITE)
        new_msg = make_msg_from_L1(core, l1_block->addr, GETX);

    l2_cache->queue_msg(new_msg, home_node);
//    log("after l2 queue");
    //Create new OTT entry
    new_ott_entry = create_ott_entry(l2_cache->msg_queues[home_node].back(), l1_block, 0, false);
//    log("after create ott");
    l1_cache->ott->add_entry(l1_block->addr, new_ott_entry);
//    log("after ott add");

l1_ret:
//    log("process trace finished");
    l1_block->block_state = INVALID;
    l1_block->valid = false;
    l2_block->valid = false;

}


// Process the msg in queue of L1 Cache
/****************TODO************/
void Mesi::process_l1_msg(Msg *_msg, int core) {
    L1Cache *l1_cache = l1_caches[core];
    
    l1_cache->get_block(_msg->addr, l1_block);

    l1_cache->num_msgs[_msg->type]++;

    log("msg type: " << msg_names[_msg->type]);

    switch(_msg->type) {
    
    case PUT:
        // copy the block in S state
        handle_put_L1(core);
        break;

    case PUTE:
        handle_pute_L1(core);
        break;
    
    case PUTX:
        handle_putx_L1(core, _msg->expected_invalidations);
        break;
    // If this L1 cache is the owner of some block.
    case GET:
        handle_get_L1(core, _msg);
        break;
    case GETX:
        handle_getx_L1(core, _msg);
        break;
    
    // This can never happen
    case UPGR:
        throw_error("tag: %lld: L1 received UPGR msg\n",l1_block->tag);
        handle_UPGR_L1(core, _msg, INVALID);
        break;
    
    // TODO: Need more hardware support :(
    case INV_ACK:
        handle_INV_ACK_L1(core);
        break;
    case UPGR_ACK:
        handle_UPGR_ACK_L1(core, _msg->expected_invalidations);
        break;
    case WB_ACK:
        handle_WB_ACK_L1(core);
    case INV:
        handle_INV_L1(core, _msg);
        break;
    case NACK:
        handle_NACK_L1(core, _msg);
        break;
    case NACKE:
        handle_NACKE_L1(core, _msg);
        break;
    default:
        break;
    }

    l1_block->valid = false;
    l1_block->block_state = INVALID;
    l2_block->valid = false;
}

void Mesi::process_l2_msg(Msg *_msg, int bank_id) {
    l2_cache->get_block(_msg->addr, l2_block);
    l2_cache->lookup(l2_block);
    log("l2_block dir state: " << state_names[l2_block->dir_entry.curr_state]);
    l2_cache->num_msgs[bank_id][_msg->type]++;

    log("sender: " << _msg->id << " addr: " << _msg->addr << " type: " << msg_names[_msg->type]);

    switch(_msg->type) {
    
    case GET:
        log("in case GET");
        handle_get_L2( bank_id, _msg->id);
        break;
    
    case GETX:
        log("in case GETX");
        handle_getx_L2( bank_id, _msg->id);
        break;
    
    case UPGR:
        handle_upgr_L2(bank_id, _msg->id);
        break;

    // TODO: How to handle the PSH state when the block got evicted. It sent out invalidations, SWB expected.
    /*
     One approach-> ignore the SWB, we only care about invalidation acknowledgements
    */
    case SWB:
        handle_swb_L2(bank_id, _msg->id);
        break;
    
    /*
        Serious problem here. Now the home node is responsible for serving the block if it is in PSH state.
    */
    case WB:
        handle_wb_L2(bank_id, _msg->id);
        break;

    // Invalidation acknowledgements
    case INV_ACK:
        handle_inv_ack_L2(bank_id, _msg->id);
        break;
    }

    l1_block->valid = false;
    l1_block->block_state = INVALID;
    l2_block->valid = false;
}


/*
    TODO: If the block is not present in L2 Cache, also neet to check the buffer. If it is present there, do not install new one
    from the memory, just send NACK!

*/
void Mesi:: handle_get_L2(int bank_id, int source_core) {
    Block _victim;
    Msg *new_msg;
    int owner;
    vector<int> sharers;

    // If the block is not present in L2 then install new one from memory !!!!! WRONG
    if (!l2_block->valid) {
        log("l2 miss");
        /**** SPECIAL CASE **********/
        l2_cache->lookup_evicted_blocks(l2_block);
        // send NACK
        if (l2_block->valid) {
            new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACKE);
            return;    
        }
        /*******************************/
        l2_block->dir_entry.curr_state = UNOWNED;
        log("copying addr: " << l2_block->addr);
        l2_cache->copy(l2_block);
        l2_cache->update_repl_params(l2_block->index, l2_block->way);
        handle_victim_L2(bank_id, source_core);
        log("block copied in l2 cache");
    }

    log("dir state: " << l2_block->dir_entry.curr_state);
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
        log("owner: " << owner);
        new_msg = make_msg_from_L1(source_core, l2_block->addr, GET);
        sharers.push_back(owner);
        sharers.push_back(source_core);
        l2_cache->set_directory_state(PSH, l2_block->index, l2_block->way);
        l2_cache->set_sharers(sharers, l2_block->index, l2_block->way);
        log("GET request sent to core " << owner);
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

void Mesi::handle_getx_L2(int bank_id, int source_core) {
    Block _victim;
    Msg *new_msg;
    int owner;
    vector<int> sharers;

//    assert(l2_block->valid); // DEBUG
    // If the block is not present in L2 then install new one from memory !!!!! WRONG
    if (!l2_block->valid) {

        /**** SPECIAL CASE **********/
        l2_cache->lookup_evicted_blocks(l2_block);
        // send NACK
        if (l2_block->valid) {
            new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACKE);
            return;    
        }
        /*******************************/
        l2_block->dir_entry.curr_state = UNOWNED;
        l2_cache->copy(l2_block);
        l2_cache->update_repl_params(l2_block->index, l2_block->way);
        handle_victim_L2(bank_id, source_core);
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
        log("in case shared");
        for(int i=0; i < CORES; i++) {
            if (i == source_core) continue;
            if(l2_block->dir_entry.sharer[i]) {
                new_msg = make_msg_from_L1(source_core, l2_block->addr, INV);
                l1_caches[i]->queue_msg(new_msg);
            }
        }
        l2_cache->set_owner(source_core, l2_block->index, l2_block->way);
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, PUTX);
        new_msg->expected_invalidations = l2_block->dir_entry.sharer.count();
        log("expt invals: " << new_msg->expected_invalidations);
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

        default:
            log("in default case. state: " << state_names[l2_block->dir_entry.curr_state]);
            throw_error("%s: invalid dir state\n", __func__);
    }


}

void Mesi::handle_upgr_L2(int bank_id, int source_core) {
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
            new_msg = make_msg_from_L2(bank_id, l2_block->addr, NACKE);
            return;    
        }
        /*******************************/
        l2_block->dir_entry.curr_state = UNOWNED;
        l2_cache->copy(l2_block);
        l2_cache->update_repl_params(l2_block->index, l2_block->way);
        handle_victim_L2(bank_id, source_core);
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
    // invalidation count.
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

void Mesi::handle_swb_L2(int bank_id, int source_core) {
    Block _victim;
    Msg *new_msg;
    int owner;
    vector<int> sharers;

    // If the block is not present in L2 then install new one from memory !!!!! WRONG
    if (!l2_block->valid) {

        /**** SPECIAL CASE **********/
        l2_cache->lookup_evicted_blocks(l2_block);
        // send NACK
        // SWB req is only to PSH/PDEX blocks, and they must be in evicted buffer
        assert(l2_block->valid);
        if (l2_block->valid) {
            // depends on the state of block
            // should only be either PSH/PDEX
            // ignore it in both cases, preferably change the state to S/M as appropriate ???????
            assert(l2_block->dir_entry.curr_state == PSH || l2_block->dir_entry.curr_state == PDEX);

            // send the invalidations now
            if (l2_block->dir_entry.curr_state == PSH) {
                for(int i = 0; i < CORES; i++) {
                    if (l2_block->dir_entry.sharer[i] == 1) {
                        new_msg = make_msg_from_L2(bank_id, l2_block->addr, INV);
                        l1_caches[i]->queue_msg(new_msg);

                    }
                }
                
            }
            else {
                owner = get_owner(l2_block->dir_entry.sharer);
                new_msg = make_msg_from_L2(bank_id, l2_block->addr, INV);
                l1_caches[owner]->queue_msg(new_msg);
            }
            
            return;
        }
        /********************************************/


    }

    /***********SERVING SWB REQUEST NOW**********************/
    // Now the block is there
    assert(l2_block->dir_entry.curr_state == PSH || l2_block->dir_entry.curr_state == PDEX);
    switch(l2_block->dir_entry.curr_state) {
    
    // Set the directory state to Shared
    case PSH:
        l2_cache->set_directory_state(SHARED, l2_block->index, l2_block->way);
        break;
    
    // Set directory state to Modified
    case PDEX:
        l2_cache->set_directory_state(MODIFIED, l2_block->index, l2_block->way);
        break;
    }

}

void Mesi::handle_wb_L2(int bank_id, int source_core) {
    Block _victim;
    Msg *new_msg;
    int owner;
    vector<int> sharers;

    if (!l2_block->valid) {

        /**** SPECIAL CASE **********/
        l2_cache->lookup_evicted_blocks(l2_block);
       
        assert(l2_block->valid);
        if (l2_block->valid) {
            switch(l2_block->dir_entry.curr_state) {
            
            case SHARED:
                throw_error("%s: dir state cannot be shared.", __func__);
                break;

            // Accept the WB. Send WB_ACK. Remove the block from the buffer.
            // Assuming that WB is in response to INV sent earlier.
            // If that is not the case, L1 should ignore the INV req. 
            case MODIFIED:
                new_msg = make_msg_from_L2(bank_id, l2_block->addr, WB_ACK);
                l1_caches[source_core]->queue_msg(new_msg);
                // remove the entry from buffer
                l2_cache->drop_evicted_block(l2_block);
                break;
            
            // Forward the WB block to second sharer. Then send the invalidations to it. Send WB_ACK back
            case PSH:
                l2_block->dir_entry.sharer[source_core] = false;
                for(int i =0; i < CORES; i++) {
                    if (l2_block->dir_entry.sharer[i]) owner = i;
                }
                new_msg = make_msg_from_L2(bank_id, l2_block->addr, PUTE);
                l1_caches[owner]->queue_msg(new_msg);
                new_msg = make_msg_from_L2(bank_id, l2_block->addr, INV);
                l1_caches[owner]->queue_msg(new_msg);
                new_msg = make_msg_from_L2(bank_id, l2_block->addr, WB_ACK);
                l1_caches[source_core]->queue_msg(new_msg);
                // Required because earlier pending invalidation was set to 2. But now only one is required.
                l2_cache->dec_pending_inv(l2_block);
                
            
            // send the invalidations now
            case PDEX:
                owner = get_owner(l2_block->dir_entry.sharer);
                new_msg = make_msg_from_L2(bank_id, l2_block->addr, PUTX);
                l1_caches[owner]->queue_msg(new_msg);
                new_msg = make_msg_from_L2(bank_id, l2_block->addr, INV);
                l1_caches[owner]->queue_msg(new_msg);
                new_msg = make_msg_from_L2(bank_id, l2_block->addr, WB_ACK);
                l1_caches[source_core]->queue_msg(new_msg);
                break;
            }
        }
        /********************************************/


        return;

    }

    /***********SERVING WB REQUEST NOW**********************/
    // Now the block is there
    switch(l2_block->dir_entry.curr_state) {
    
    case UNOWNED:
        throw_error("%s: dir state cannot be unowned\n", __func__);
        break;
    case SHARED:
        throw_error("%s: dir state cannot be shared\n", __func__);
        break;
    
    // set the directory state to UNOWNED
    case MODIFIED:
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, WB_ACK);
        l1_caches[source_core]->queue_msg(new_msg);
        l2_cache->set_directory_state(UNOWNED, l2_block->index, l2_block->way);
    // Set the directory state to MODIFIED
    case PSH:
        l2_block->dir_entry.sharer[source_core] = false;
        for(int i =0; i < CORES; i++) {
            if (l2_block->dir_entry.sharer[i]) owner = i;
        }
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, PUTE);
        l1_caches[owner]->queue_msg(new_msg);
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, WB_ACK);
        l1_caches[source_core]->queue_msg(new_msg);
        l2_cache->set_directory_state(MODIFIED, l2_block->index, l2_block->way);
        break;
    
    // Set directory state to Modified
    case PDEX:
        owner = get_owner(l2_block->dir_entry.sharer);
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, PUTX);
        l1_caches[owner]->queue_msg(new_msg);
        new_msg = make_msg_from_L2(bank_id, l2_block->addr, WB_ACK);
        l1_caches[source_core]->queue_msg(new_msg);
        l2_cache->set_directory_state(MODIFIED, l2_block->index, l2_block->way);
        break;
    }

}


/*
    This will only be required when a block in L2 gets evicted and it waits for all L1 caches to evict that block
*/
void Mesi::handle_inv_ack_L2(int bank_id, int source_core) {
   int inv;
   assert(!l2_block->valid);
   l2_cache->lookup_evicted_blocks(l2_block);
   assert(l2_block->valid);
   inv = l2_cache->dec_pending_inv(l2_block);
   if (inv == 0){
        l2_cache->drop_evicted_block(l2_block);
   }
}

void Mesi::handle_victim_L2(int bank_id, int source_core) {
    Block _victim;
    Msg *new_msg;
    int owner;
    if (l2_cache->victim) {
        _victim = *l2_cache->victim;
        delete l2_cache->victim;
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
            l2_cache->insert_evicted_block(&_victim, _victim.dir_entry.sharer.count());
            break;

        // Get the owner and then invalidate it
        case MODIFIED:
            owner = (int)(_victim.dir_entry.sharer.to_ulong());
            new_msg = make_msg_from_L2(bank_id, _victim.addr, INV);
            l1_caches[owner]->queue_msg(new_msg);
            l2_cache->insert_evicted_block(&_victim, 1);
            break;

        case PSH:
            // Delay the invalidations
            l2_cache->insert_evicted_block(&_victim, 2);
        
            break;
        
        case PDEX:
            // Delay the invalidation
            l2_cache->insert_evicted_block(&_victim, 1);
            break;
            
        
        }
    }
}

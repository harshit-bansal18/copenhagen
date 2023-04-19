#include<protocol/ott.h>


Ott_entry* create_ott_entry(Msg* msg, Block* _block, int pi, bool hmr){
    Ott_entry* new_entry = new Ott_entry;
    new_entry->_msg = *msg;
    new_entry->_block = *_block;
    new_entry->pending_invals = pi;
    new_entry->home_msg_received = hmr;
    new_entry->invalid = false;
    return new_entry;
}


OTT::OTT(){
    nackTimer.assign(6, {});
}

void OTT::add_entry(unsigned long long addr, Ott_entry* entry){
    table[addr] = entry;
}

void OTT::remove_entry(unsigned long long addr){
    table.erase(addr);
}

bool OTT::check_entry(unsigned long long addr){
    return table.find(addr) != table.end();
}

void OTT::add_nacked(unsigned long long addr){
    nackTimer.back().push_back(table[addr]);
}

void OTT::decrement_timer(){
    //decrement counter
    nackTimer.pop_front();
    nackTimer.push_back({});
}

void update_ott_entry(L1Cache* l1_cache, unsigned long long addr, bool hmr, int inval_exp){
    l1_cache->ott->table[addr]->home_msg_received = hmr;
    l1_cache->ott->table[addr]->pending_invals += inval_exp;
}

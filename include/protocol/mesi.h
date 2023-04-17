#pragma once
#include<bitset>
#include <cache/L1_cache.h>
#include <cache/L2_cache.h>
#include <protocol/ott.h>

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
   ROLL_BACK,
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
};

class Directory_entry{
public:
   bitset<8> sharer;
   state curr_state;
};

// Define a state machine for MESI protocol

class Mesi {
public:
   vector<L1Cache *> l1_caches;
   L2Cache *l2_cache;
   Block *l1_block;
   Block *l2_block;
   Mesi();
   void process_l1_msg(Msg *_msg, int core);
   void process_l2_msg(Msg *_msg, int bank_id);
   void process_trace(Trace *_trace);

   void perform_ott_entry_removal(int core);
   void handle_pending_msgs(int core);

   void handle_put_L1(int core, state put_states, int expected_invalidations = 0);
   void handle_put_L1_inv_ack(int core, state put_state);
   void handle_get_L1(int core, Msg* _msg, state final_state);
   void handle_UPGR_L1(int core, Msg* _msg, state final_state);
   void handle_INV_L1(int core, Msg* _msg);
   void handle_NACK_L1(int core);
   void handle_NACKE_L1(int core);
   void handle_INV_ACK_L1(int core);
   void handle_UPGR_ACK_L1(int core);
   void handle_WB_ACK_L1(int core);
   void handle_get_L2(int bank_id, int source_core);
   void handle_getx_L2(int bank_id, int source_core);
   void handle_upgr_L2(int bank_id, int source_core);
   void handle_swb_L2(int bank_id, int source_core);
   void handle_wb_L2(int bank_id, int source_core);
   void handle_inv_ack_L2(int bank_id, int source_core);
   void handle_victim_L2(int bank_id, int source_core); 
};
#ifndef __MESI_H__
#define __MESI_H__

#include "trace.h"
#include "cache/cache.h"

class L1Cache;
class L2Cache;
class Block;

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
};

class Directory_entry{
public:
   std::bitset<8> sharer;
   state curr_state;

   Directory_entry() {
       curr_state = INVALID;
       sharer.reset();
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

   void handle_put_L1(int core, int expected_invalidations = 0);
   void handle_putx_L1(int core,  int expected_invalidations = 0);
   void handle_pute_L1(int core,  int expected_invalidations = 0);

   void handle_put_L1_inv_ack(int core, state put_state);
   void handle_victim_L1(int core);
   void handle_get_L1(int core, Msg* _msg);
   void handle_getx_L1(int core, Msg* _msg);
   void handle_UPGR_L1(int core, Msg* _msg, state final_state);
   void handle_INV_L1(int core, Msg* _msg);
   void handle_NACK_L1(int core, Msg *_msg);
   void handle_NACKE_L1(int core, Msg *_msg);
   void handle_INV_ACK_L1(int core);
   void handle_UPGR_ACK_L1(int core, int expected_invalidations = 0);
   void handle_WB_ACK_L1(int core);

   void handle_get_L2(int bank_id, int source_core);
   void handle_getx_L2(int bank_id, int source_core);
   void handle_upgr_L2(int bank_id, int source_core);
   void handle_swb_L2(int bank_id, int source_core);
   void handle_wb_L2(int bank_id, int source_core);
   void handle_inv_ack_L2(int bank_id, int source_core);
   void handle_victim_L2(int bank_id, int source_core); 
};

#endif
#pragma once
#include<bitset>
#include <cache/L1_cache.h>
#include <cache/L2_cache.h>

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
   ACK,
   NACK
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
};

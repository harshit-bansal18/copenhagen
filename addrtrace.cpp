#include <stdio.h>
#include <string.h>
#include "pin.H"

FILE *f_trace, *f_trace_count;
FILE *f_traces[8];
PIN_LOCK pLock;

UINT64 trace_count = 0;
UINT64 global_count = 0;


void atomizedTrace(THREADID tid, UINT64 addr){
    fprintf(f_trace[tid], "%d %lu\n", tid, addr);
    trace_count++;
    fflush(f_trace);
}

void RecordMem(THREADID tid, void * taddr, UINT32 size){
    PIN_GetLock(&pLock, tid+1);
    UINT64 addr = (UINT64) taddr;
    UINT64 end_addr = addr + size - 1;

    UINT64 first_block = addr / 64;
    UINT64 last_block = end_addr / 64;
    if(first_block == last_block){
        //if the complete memory access is in single block
        UINT64 count8 = size / 8;
        size %= 8;
        UINT64 count4 = size / 4;
        size %= 4;
        UINT64 count2 = size / 2;
        size %= 2;
        UINT64 count1 = size;

        UINT64 curr_offset = 0;
        for(UINT64 i = 0; i < count8; i++, curr_offset += 8)
            atomizedTrace(tid, addr + curr_offset);
        for(UINT64 i = 0; i < count4; i++, curr_offset += 4)
            atomizedTrace(tid, addr + curr_offset);
        for(UINT64 i = 0; i < count2; i++, curr_offset += 2)
            atomizedTrace(tid, addr + curr_offset);
        for(UINT64 i = 0; i < count1; i++, curr_offset += 1)
            atomizedTrace(tid, addr + curr_offset);

    }

    else{
        UINT64 curr_offset = 0;

        UINT64 first_block_remain = 64 - (addr % 64);
        UINT64 last_block_remain = end_addr % 64;

        UINT64 count8 = first_block_remain / 8;
        first_block_remain %= 8;
        UINT64 count4 = first_block_remain / 4;
        first_block_remain %= 4;
        UINT64 count2 = first_block_remain / 2;
        first_block_remain %= 2;
        UINT64 count1 = first_block_remain;

        for(UINT64 i = 0; i < count8; i++, curr_offset += 8)
            atomizedTrace(tid, addr + curr_offset);
        for(UINT64 i = 0; i < count4; i++, curr_offset += 4)
            atomizedTrace(tid, addr + curr_offset);
        for(UINT64 i = 0; i < count2; i++, curr_offset += 2)
            atomizedTrace(tid, addr + curr_offset);
        for(UINT64 i = 0; i < count1; i++, curr_offset += 1)
            atomizedTrace(tid, addr + curr_offset);

        for(UINT64 block = first_block+1; block < last_block; block++){
            UINT64 count8 = 64 / 8;
            for(UINT64 i = 0; i < count8; i++, curr_offset += 8)
                atomizedTrace(tid, addr + curr_offset);
        }

        count8 = last_block_remain / 8;
        last_block_remain %= 8;
        count4 = last_block_remain / 4;
        last_block_remain %= 4;
        count2 = last_block_remain / 2;
        last_block_remain %= 2;
        count1 = last_block_remain;

        for(UINT64 i = 0; i < count8; i++, curr_offset += 8)
            atomizedTrace(tid, addr + curr_offset);
        for(UINT64 i = 0; i < count4; i++, curr_offset += 4)
            atomizedTrace(tid, addr + curr_offset);
        for(UINT64 i = 0; i < count2; i++, curr_offset += 2)
            atomizedTrace(tid, addr + curr_offset);
        for(UINT64 i = 0; i < count1; i++, curr_offset += 1)
            atomizedTrace(tid, addr + curr_offset);

    }

	PIN_ReleaseLock(&pLock);
}


void Instruction(INS ins, void *v){
    UINT32 mem_operands = INS_MemoryOperandCount(ins);
    
    for(UINT32 mem_op = 0; mem_op < mem_operands; mem_op++){
        UINT32 mem_op_size = INS_MemoryOperandSize(ins, mem_op);
        int to_record = INS_MemoryOperandIsRead(ins, mem_op) + INS_MemoryOperandIsWritten(ins, mem_op);
        while(to_record--){
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
                IARG_THREAD_ID,
                IARG_MEMORYOP_EA, mem_op,
                IARG_UINT32, mem_op_size,
                IARG_END
            );
        }
    }
}

void ThreadStart(THREADID tid, CONTEXT *ctx, INT32 flags, void *v){
    PIN_GetLock(&pLock, tid+1);
    fprintf(stdout, "Begin thread %d\n", tid);
    fflush(stdout);
    PIN_ReleaseLock(&pLock);
}

void ThreadFini(THREADID tid, const CONTEXT *ctx, INT32 code, void *v){
    PIN_GetLock(&pLock, tid+1);
    fprintf(stdout, "End thread %d\n", tid);
    fflush(stdout);
    PIN_ReleaseLock(&pLock);
}

void Fini(INT32 code, void * v){
    fprintf(f_trace_count, " : %lu\n", trace_count);
    fflush(f_trace_count);
    fclose(f_trace);
    fclose(f_trace_count);
}


/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char* argv[]){
    char *filename = argv[6];
    strcat(filename, ".out");
    filename += 2;
    for(int i = 0; i < 8; i++){
        f_traces[i] = fopen(filename + "." + string(i), "w");
    }
    // f_trace = fopen(filename, "w");
    f_trace_count = fopen("part1_output.txt", "a");
    fprintf(f_trace_count, "Trace count for %s program is ", filename);
    fflush(f_trace_count);

    PIN_InitLock(&pLock);
    if(PIN_Init(argc, argv)) return Usage();

    INS_AddInstrumentFunction(Instruction, 0);
    
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();

    return 0;
}
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "pin.H"



FILE *f_trace, *f_trace_count;
FILE *f_traces[8];
PIN_LOCK pLock;

UINT64 trace_count = 0;
UINT64 global_count = 0;


void atomizedTrace(THREADID tid, UINT64 addr, bool type){
    fprintf(f_traces[tid], "%d %lu %c %lu\n", tid, addr, type?'s':'l', global_count);
    global_count++;
    trace_count++;
    fflush(f_trace);
}

void RecordMem(THREADID tid, void * taddr, UINT32 size, bool type){
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
            atomizedTrace(tid, addr + curr_offset, type);
        for(UINT64 i = 0; i < count4; i++, curr_offset += 4)
            atomizedTrace(tid, addr + curr_offset, type);
        for(UINT64 i = 0; i < count2; i++, curr_offset += 2)
            atomizedTrace(tid, addr + curr_offset, type);
        for(UINT64 i = 0; i < count1; i++, curr_offset += 1)
            atomizedTrace(tid, addr + curr_offset, type);

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
            atomizedTrace(tid, addr + curr_offset, type);
        for(UINT64 i = 0; i < count4; i++, curr_offset += 4)
            atomizedTrace(tid, addr + curr_offset, type);
        for(UINT64 i = 0; i < count2; i++, curr_offset += 2)
            atomizedTrace(tid, addr + curr_offset, type);
        for(UINT64 i = 0; i < count1; i++, curr_offset += 1)
            atomizedTrace(tid, addr + curr_offset, type);

        for(UINT64 block = first_block+1; block < last_block; block++){
            UINT64 count8 = 64 / 8;
            for(UINT64 i = 0; i < count8; i++, curr_offset += 8)
                atomizedTrace(tid, addr + curr_offset, type);
        }

        count8 = last_block_remain / 8;
        last_block_remain %= 8;
        count4 = last_block_remain / 4;
        last_block_remain %= 4;
        count2 = last_block_remain / 2;
        last_block_remain %= 2;
        count1 = last_block_remain;

        for(UINT64 i = 0; i < count8; i++, curr_offset += 8)
            atomizedTrace(tid, addr + curr_offset, type);
        for(UINT64 i = 0; i < count4; i++, curr_offset += 4)
            atomizedTrace(tid, addr + curr_offset, type);
        for(UINT64 i = 0; i < count2; i++, curr_offset += 2)
            atomizedTrace(tid, addr + curr_offset, type);
        for(UINT64 i = 0; i < count1; i++, curr_offset += 1)
            atomizedTrace(tid, addr + curr_offset, type);

    }

	PIN_ReleaseLock(&pLock);
}


void Instruction(INS ins, void *v){
    UINT32 mem_operands = INS_MemoryOperandCount(ins);
    
    for(UINT32 mem_op = 0; mem_op < mem_operands; mem_op++){
        UINT32 mem_op_size = INS_MemoryOperandSize(ins, mem_op);
        if(INS_MemoryOperandIsRead(ins, mem_op)){
            bool type = false;
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
                IARG_THREAD_ID,
                IARG_MEMORYOP_EA, mem_op,
                IARG_UINT32, mem_op_size,
                IARG_BOOL, type,
                IARG_END
            );
        }

        if(INS_MemoryOperandIsWritten(ins, mem_op)){
            bool type = true;
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
                IARG_THREAD_ID,
                IARG_MEMORYOP_EA, mem_op,
                IARG_UINT32, mem_op_size,
                IARG_BOOL, type,
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
    // fclose(f_trace);
    for(int i = 0 ; i < 8; i++){
        fclose(f_traces[i]);
    }
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
    filename += 2;


    for(int i = 0; i < 8; i++){
        char nf[100];
        sprintf(nf, "./%s_output/%s.out.%d", filename, filename,i);
        f_traces[i] = fopen(nf, "w");
    }



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
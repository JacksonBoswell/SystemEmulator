/**************************************************************************
 * STUDENTS: DO NOT MODIFY.
 * 
 * C S 429 system emulator
 * 
 * instr_base.c - Common helper routines for instruction processing.
 * 
 * Stage details are written in the instr_*.c files in this directory.
 * 
 * Copyright (c) 2022, 2023. 
 * Authors: S. Chatterjee, P. Jamadagni, Z. Leeper. 
 * All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "err_handler.h"
#include "instr.h"
#include "instr_pipeline.h"
#include "machine.h"
#include "hw_elts.h"


extern machine_t guest;
extern mem_status_t dmem_status;


typedef struct {
    char opcode[6];
    int64_t pcbits;
    uint32_t insnbits;
    uint64_t seqsuccpc;
    uint64_t predpc;
    char status[4];
} FetchStageData;

typedef struct {
    char opcode[6];
    uint64_t X_in_val_a;
    uint64_t X_in_val_b;
    uint64_t X_in_val_imm;
    char* alu_op;
    char* cond;
    int dst;
    char status[4];
} DecodeStageData;

typedef struct {
    char opcode[6];
    uint64_t X_out_val_a;
    uint64_t X_out_val_b;
    uint64_t X_out_val_imm;
    uint64_t X_out_val_hw;
    char* alu_op;
    char status[4];
} ExecuteStageData;

typedef struct {
    char opcode[6];
    uint64_t M_out_val_ex;
    uint64_t M_out_val_b;
    uint64_t M_out_val_mem;
    char status[4];
} MemoryStageData;

typedef struct {
    char opcode[6];
    int dst;
    uint64_t W_out_val_ex;
    uint64_t W_out_val_mem;
    char status[4];   
} WritebackStageData;


// Function to serialize FetchStageData into a JSON string
char* serialize_fetch_stage_data(const FetchStageData* data) {
    // Estimate the size of the JSON string
    size_t bufferSize = 200; // Adjust based on expected data size
    char* jsonStr = malloc(bufferSize);

    if (jsonStr == NULL) {
        perror("Failed to allocate memory");
        return NULL; // Memory allocation failed
    }

    // Construct the JSON string
    snprintf(jsonStr, bufferSize, "{\"stage\": \"fetch\", \"opcode\": \"%-6s\", \"pcbits\": \"%08lX\", \"insnbits\": \"%08X\", \"seqsuccpc\": \"%lX\", \"predpc\": \"%lX\", \"status\": \"%s\"}",
             data->opcode, data->pcbits, data->insnbits, data->seqsuccpc, data->predpc, data->status);
    return jsonStr;
}

// Function to serialize DecodeStageData into a JSON string
char* serialize_decode_stage_data(const DecodeStageData* data) {
    // Estimate the size of the JSON string
    size_t bufferSize = 200; // Adjust based on expected data size
    char* jsonStr = malloc(bufferSize);

    if (jsonStr == NULL) {
        perror("Failed to allocate memory");
        return NULL; // Memory allocation failed
    }

    // Construct the JSON string
    snprintf(jsonStr, bufferSize, "{\"stage\": \"decode\", \"opcode\": \"%-6s\", \"X_in_val_a\": \"0x%lX\", \"X_in_val_b\": \"0x%lX\", \"X_in_val_imm\": \"0x%lX\", \"alu_op\": \"%s\", \"cond\": \"%s\", \"dst\": \"X%d\", \"status\": \"%s\"}",
             data->opcode, data->X_in_val_a, data->X_in_val_b, data->X_in_val_imm, data->alu_op, data->cond, data->dst, data->status);
    return jsonStr;
}

// Function to serialize ExecuteStageData into a JSON string
char* serialize_execute_stage_data(const ExecuteStageData* data) {
    // Estimate the size of the JSON string
    size_t bufferSize = 200; // Adjust based on expected data size
    char* jsonStr = malloc(bufferSize);

    if (jsonStr == NULL) {
        perror("Failed to allocate memory");
        return NULL; // Memory allocation failed
    }

    // Construct the JSON string
    snprintf(jsonStr, bufferSize, "{\"opcode\": \"%-6s\", \"X_out_val_a\": \"0x%lX\", \"X_out_val_b\": \"0x%lX\", \"X_out_val_imm\": \"0x%lX\", \"X_out_val_hw\": \"0x%lX\", \"alu_op\": \"%s\", \"status\": \"%s\"}",
             data->opcode, data->X_out_val_a, data->X_out_val_b, data->X_out_val_imm, data->X_out_val_hw, data->alu_op, data->status);
    return jsonStr;
}

// Function to serialize MemoryStageData into a JSON string
char* serialize_memory_stage_data(const MemoryStageData* data) {
    // Estimate the size of the JSON string
    size_t bufferSize = 200; // Adjust based on expected data size
    char* jsonStr = malloc(bufferSize);

    if (jsonStr == NULL) {
        perror("Failed to allocate memory");
        return NULL; // Memory allocation failed
    }

    // Construct the JSON string
    snprintf(jsonStr, bufferSize, "{\"opcode\": \"%-6s\", \"M_out_val_ex\": \"0x%lX\", \"M_out_val_b\": \"0x%lX\", \"M_out_val_mem\": \"0x%lX\", \"status\": \"%s\"}",
             data->opcode, data->M_out_val_ex, data->M_out_val_b, data->M_out_val_mem, data->status);
    return jsonStr;
}

// Function to serialize WritebackStageData into a JSON string
char* serialize_writeback_stage_data(const WritebackStageData* data) {
    // Estimate the size of the JSON string
    size_t bufferSize = 200; // Adjust based on expected data size
    char* jsonStr = malloc(bufferSize);

    if (jsonStr == NULL) {
        perror("Failed to allocate memory");
        return NULL; // Memory allocation failed
    }

    // Construct the JSON string
    snprintf(jsonStr, bufferSize, "{\"opcode\": \"%-6s\", \"dst\": \"X%d\", \"W_out_val_ex\": \"0x%lX\", \"W_out_val_mem\": \"0x%lX\", \"status\": \"%s\"}",
             data->opcode, data->dst, data->W_out_val_ex, data->W_out_val_mem, data->status);
    return jsonStr;
}

uint64_t F_PC;
int64_t W_wval;

/*
 * Extracts the bitfield src[frompos+width-1:frompos] and returns it
 * as an unsigned 32-bit integer.
 * 
 * Use this function for register specifiers, condition specifiers, 
 * and immediates in I1- and RI-format instructions. Also use it to
 * extract the 11 bits used to determine the opcode.
 */
uint32_t bitfield_u32(int32_t src, unsigned frompos, unsigned width) {
    return (uint32_t) ((src >> frompos) & ((1 << width) - 1));
}

/*
 * Extracts the bitfield src[frompos+width-1:frompos] and returns it
 * as an signed 64-bit integer.
 * 
 * Use this function for 64-bit offsets needed in M-, I2-, B1-, and 
 * B2-format instructions.
 */
int64_t bitfield_s64(int32_t src, unsigned frompos, unsigned width) {
    return ((int64_t) ((((int64_t)src >> frompos) & ((1 << width) - 1)) << (64 - width))) >> (64 - width);
}

static inline void init_itable_entry(opcode_t op, unsigned idx) {
    assert(OP_ERROR == itable[idx]);
    itable[idx] = op;
}

static inline void init_itable_range(opcode_t op, unsigned idx1, unsigned idx2) {
    for (unsigned i = idx1; i <= idx2; i++) {
        assert(OP_ERROR == itable[i]);
        itable[i] = op;
    }
}

/*
 * Initialize the itable. Called from interface.c.
 *
 * There are no entries for CMP, TST, LSL, and LSR because they are aliases of other instructions.
 * For extra credit there should be no entries for CSINC and CSINV because they are aliases of other instruction
 */

// init_itable_entry(opcode_t opcode, unsigned long encoding)
// init_itable_entry(opcode_t opcode, unsigned long start_encoding_range, end_enconding_range)
void 
init_itable(void) {
    for (int i = 0; i < 2<<11; i++) itable[i] = OP_ERROR;
    init_itable_entry(OP_LDUR, 0x7c2U);
    init_itable_entry(OP_STUR, 0x7c0U);
    init_itable_range(OP_MOVK, 0x794U, 0x797U);
    init_itable_range(OP_MOVZ, 0x694U, 0x697U);
    init_itable_range(OP_ADRP, 0x480U, 0x487U);
    init_itable_range(OP_ADRP, 0x580U, 0x587U);
    init_itable_range(OP_ADRP, 0x680U, 0x687U);
    init_itable_range(OP_ADRP, 0x780U, 0x787U);
    init_itable_range(OP_ADD_RI, 0x488U, 0x489U);
    init_itable_entry(OP_ADDS_RR, 0x558U);
    init_itable_range(OP_SUB_RI, 0x688U, 0x689U);
    init_itable_entry(OP_SUBS_RR, 0x758U);
    init_itable_entry(OP_MVN, 0x551U);
    init_itable_entry(OP_ORR_RR, 0x550U);
    init_itable_entry(OP_EOR_RR, 0x650U);
    init_itable_entry(OP_ANDS_RR, 0x750U);
    init_itable_range(OP_UBFM, 0x69aU, 0x69bU); // LSL, LSR share the same opcode
    init_itable_range(OP_ASR, 0x49aU, 0x49bU);
    init_itable_range(OP_B, 0x0a0U, 0x0bfU);
    init_itable_range(OP_B_COND, 0x2a0U, 0x2a7U);
    init_itable_range(OP_BL, 0x4a0U, 0x4bfU);
    init_itable_entry(OP_RET, 0x6b2U);
    init_itable_entry(OP_NOP, 0x6a8U);
    init_itable_entry(OP_HLT, 0x6a2U);

    //extra credit 
    init_itable_entry(OP_CSEL, 0x4d4U);
    init_itable_entry(OP_CSNEG, 0x6d4U);
    init_itable_range(OP_CBZ, 0x5a0U, 0x5a7U);
    init_itable_range(OP_CBNZ, 0x5a8U, 0x5afU);
    init_itable_entry(OP_BR, 0x6b0U);
    init_itable_entry(OP_BLR, 0x6b1U);
}

static char *opcode_names[] = {
    "NOP ",
    "LDUR ",
    "STUR ",
    "MOVK ",
    "MOVZ ",
    "ADRP ",
    "ADD ",
    "ADDS ",
    "SUB",
    "SUBS ",
    "CMP ",
    "MVN ",
    "ORR ",
    "EOR ",
    "ANDS ",
    "TST ",
    "LSL ",
    "LSR ",
    "UBFM ",
    "ASR ",
    "B ",
    "B.cond ",
    "BL ",
    "RET ",
    "HLT ",
    "CSEL",
    "CSINV",
    "CSINC",
    "CSNEG",
    "CBZ",
    "CBNZ",
    "BR",
    "BLR",
    "ERR "
};

static char *cond_names[] = {
    "EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", 
    "HI", "LS", "GE", "LT", "GT", "LE", "AL", "NV"
};

static char *alu_op_names[] = {
    "PLUS_OP",
    "MINUS_OP",
    "INV_OP",
    "OR_OP",
    "EOR_OP",
    "AND_OP",
    "MOV_OP",
    "LSL_OP",
    "LSR_OP",
    "ASR_OP",
    "PASS_A_OP",
    "CSEL_OP",
    "CSINV_OP",
    "CSINC_OP",
    "CSNEG_OP",
    "CBZ",
    "CBNZ",
    "BR",
    "BLR",
};


/*
 * A debugging aid to print out the fields of an instruction.
 * This routine is called from runElf().
 * Do not re-write.
 * 
 * The amount of detail printed is controlled by debug_level.
 *  0: No output.
 *  1: Medium output. Data signals only in the pipeline register.
 *  2: Full output. Data and control signals in the pipeline register.
 */

static void 
get_stat_str(char *str, stat_t status) {
    switch (status) {
        case STAT_AOK:
            strcpy(str, "AOK");
            break;
        case STAT_BUB:
            strcpy(str, "BUB");
            break;
        case STAT_HLT:
            strcpy(str, "HLT");
            break;
        case STAT_ADR:
            strcpy(str, "ADR");
            break;
        case STAT_INS:
            strcpy(str, "INS");
            break;
    }
}

void 
show_instr(const proc_stage_t stage, int debug_level, char** message) {
    
    if(debug_level < 1) {
        return;
    }
    char status[4];
    char *jsonString;

    switch (stage) {
    case S_FETCH:
        get_stat_str(status, F_out->status);
        printf("F: %-6s[PC, insn_bits] = [%08lX,  %08X], seq_succ_PC: 0x%lX, pred_PC: 0x%lX, adrp_val: 0x%lX, status: %s\n", 
            D_in->print_op != OP_ERROR ? opcode_names[D_in->print_op] : "NOP",
            guest.proc->PC, 
            D_in->insnbits,
            D_in->seq_succ_PC,
            F_in->pred_PC,
            D_in->adrp_val,
            status);
        FetchStageData fetch_data = {
            .pcbits=guest.proc->PC,
            .insnbits=D_in->insnbits,
            .seqsuccpc=D_in->seq_succ_PC,
            .predpc=F_in->pred_PC,
        };
        snprintf(fetch_data.opcode, 7, "%s", !OP_ERROR ? opcode_names[D_in->print_op] : "NOP");
        snprintf(fetch_data.status, 4, "%s", status);
        
        jsonString = serialize_fetch_stage_data(&fetch_data);
        *message = jsonString;
        // printf("Sending message: %s\n", message);
        break;
    case S_DECODE:
        get_stat_str(status, D_out->status);
        printf("D: %-6s[val_a, val_b, imm] = [0x%lX, 0x%lX, 0x%lX], alu_op: %s, cond: %s, dst: X%d, status: %s\n",
            D_out->print_op != OP_ERROR ? opcode_names[D_out->print_op] : "NOP",
            X_in->val_a,
            X_in->val_b,
            X_in->val_imm,
            alu_op_names[X_in->ALU_op],
            cond_names[X_in->cond],
            X_in->dst,
            status);
        DecodeStageData decode_data = {
        .X_in_val_a=X_in->val_a,
        .X_in_val_b=X_in->val_b,
        .X_in_val_imm=X_in->val_imm,
        .alu_op=alu_op_names[X_in->ALU_op],
        .cond=cond_names[X_in->cond],
        .dst=X_in->dst,
        };
        snprintf(decode_data.opcode, 7, "%s", !OP_ERROR ? opcode_names[D_out->print_op] : "NOP");
        snprintf(decode_data.status, 4, "%s", status);
        jsonString = serialize_decode_stage_data(&decode_data);
        *message = jsonString;
        // printf("Sending message: %s\n", message);

        if (debug_level == 1)
            break;
        
        printf("\t X_sigs: [valb_sel, set_CC] = [%s, %s]\n",
            X_in->X_sigs.valb_sel ? "true " : "false",
            X_in->X_sigs.set_CC ? "true" : "false");

        printf("\t M_sigs: [dmem_read, dmem_write] = [%s, %s]\n",
            X_in->M_sigs.dmem_read ? "true " : "false",
            X_in->M_sigs.dmem_write ? "true" : "false");

        printf("\t W_sigs: [dst_sel, wval_sel, w_enable] = [%s, %s, %s]\n",
            X_in->W_sigs.dst_sel ? "true " : "false",
            X_in->W_sigs.wval_sel ? "true " : "false",
            X_in->W_sigs.w_enable ? "true" : "false");

        break;

        case S_EXECUTE: 
            get_stat_str(status, X_out->status);
            printf("X: %-6s[val_ex, a, b, imm, hw] = [0x%lX, 0x%lX, 0x%lX, 0x%lX, 0x%X], alu_op: %s, status: %s\n",
                X_out->print_op != OP_ERROR ? opcode_names[X_out->print_op] : "NOP",
                M_in->val_ex,
                X_out->val_a,
                X_out->val_b,
                X_out->val_imm,
                X_out->val_hw,
                alu_op_names[X_out->ALU_op],
                status);
            printf("\t X_condval: %s\n", M_in->cond_holds ? "true" : "false");

            ExecuteStageData execute_data = {
            .X_out_val_a=X_out->val_a,
            .X_out_val_b=X_out->val_b,
            .X_out_val_imm=X_out->val_imm,
            .X_out_val_hw=X_out->val_hw,
            .alu_op=alu_op_names[X_out->ALU_op]
            };

            snprintf(execute_data.opcode, 7, "%s", !OP_ERROR ? opcode_names[X_out->print_op] : "NOP");
            snprintf(execute_data.status, 4, "%s", status);

            jsonString = serialize_execute_stage_data(&execute_data);
            *message = jsonString;
            // printf("Sending message: %s\n", message);

            if (debug_level == 1)
                break;
        
            // debug level 2: also print control signals

            printf("\t X_sigs: [valb_sel, set_CC] = [%s, %s]\n",
            X_out->X_sigs.valb_sel ? "true " : "false",
            X_out->X_sigs.set_CC ? "true" : "false");

            break;
        case S_MEMORY:
            get_stat_str(status, M_out->status); 
            printf("M: %-6s[val_ex, val_b, val_mem] = [0x%lX, 0x%lX, 0x%lX], status: %s\n",
                M_out->print_op != OP_ERROR ? opcode_names[M_out->print_op] : "NOP",
                M_out->val_ex,
                M_out->val_b,
                W_in->val_mem,
                status);

            MemoryStageData memory_data = {
            .M_out_val_ex=M_out->val_ex,
            .M_out_val_b=M_out->val_b,
            .M_out_val_mem=W_in->val_mem,
            };

            snprintf(memory_data.opcode, 7, "%s", !OP_ERROR ? opcode_names[M_out->print_op] : "NOP");
            snprintf(memory_data.status, 4, "%s", status);

            jsonString = serialize_memory_stage_data(&memory_data);
            *message = jsonString;
            // printf("Sending message: %s\n", message);

            if (debug_level == 1)
                break;
        
            // debug level 2: also print control signals

            printf("\t M_sigs: [dmem_read, dmem_write] = [%s, %s]\n",
                M_out->M_sigs.dmem_read ? "true " : "false",
                M_out->M_sigs.dmem_write ? "true" : "false");

            break;
        case S_WBACK:
            get_stat_str(status, W_out->status);
            printf("W: %-6s[dst, val_ex, val_mem] = [X%d, 0x%lX, 0x%lX], status: %s\n",
                W_out->print_op != OP_ERROR ? opcode_names[W_out->print_op] : "NOP",
                W_out->dst,
                W_out->val_ex,
                W_out->val_mem,
                status);

            WritebackStageData writeback_data = {
            .dst=W_out->dst,
            .W_out_val_ex=W_out->val_ex,
            .W_out_val_mem=W_out->val_mem,
            };

            snprintf(writeback_data.opcode, 7, "%s", !OP_ERROR ? opcode_names[W_out->print_op] : "NOP");
            snprintf(writeback_data.status, 4, "%s", status);

            jsonString = serialize_writeback_stage_data(&writeback_data);
            *message = jsonString;
            // printf("Sending message: %s\n", message);


            if (debug_level == 1)
                break;
        
            // debug level 2: also print control signals

            printf("\t W_sigs: [dst_sel, wval_sel, w_enable] = [%s, %s, %s], W_wval: 0x%lx\n",
                W_out->W_sigs.dst_sel ? "true " : "false",
                W_out->W_sigs.wval_sel ? "true " : "false",
                W_out->W_sigs.w_enable ? "true" : "false",
                W_wval);
            break;
        default: IMPOSSIBLE(); break;
    }
    return;
}

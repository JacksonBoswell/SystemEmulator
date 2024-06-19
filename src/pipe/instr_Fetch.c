/**************************************************************************
 * C S 429 system emulator
 * 
 * instr_Fetch.c - Fetch stage of instruction processing pipeline.
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

extern uint64_t F_PC;

/*
 * Select PC logic.
 * STUDENT TO-DO:
 * Write the next PC to *current_PC.
 */

static comb_logic_t 
select_PC(uint64_t pred_PC,                                       // The predicted PC
          opcode_t D_opcode, uint64_t val_a, uint64_t D_seq_succ, // Possible correction from RET
          opcode_t M_opcode, bool M_cond_val, uint64_t seq_succ,  // Possible correction from B.cond
          uint64_t *current_PC) {
    /* 
     * Students: Please leave this code
     * at the top of this function. 
     * You may modify below it. 
     */
    if (D_opcode == OP_RET && val_a == RET_FROM_MAIN_ADDR) {
        *current_PC = 0; // PC can't be 0 normally.
        return;
    }
    // Modify starting here.

    if (D_opcode == OP_RET || D_opcode == OP_BR || D_opcode == OP_BLR) {
        *current_PC = val_a;
    }
    else {
        switch (M_opcode) {
            case OP_B_COND:
            case OP_CBZ:
            case OP_CBNZ:
                if (M_cond_val) {
                    *current_PC = pred_PC;
                }
                else {
                    *current_PC = seq_succ;
                }
                break;
            default:
                *current_PC = pred_PC;
                break;
        }
    }
    return;
}

/*
 * Predict PC logic. Conditional branches are predicted taken.
 * STUDENT TO-DO:
 * Write the predicted next PC to *predicted_PC
 * and the next sequential pc to *seq_succ.
 */

static comb_logic_t 
predict_PC(uint64_t current_PC, uint32_t insnbits, opcode_t op, 
           uint64_t *predicted_PC, uint64_t *seq_succ) {
    /* 
     * Students: Please leave this code
     * at the top of this function. 
     * You may modify below it. 
     */
    if (!current_PC) {
        return; // We use this to generate a halt instruction.
    }
    // Modify starting here.
    *seq_succ = current_PC + 4;
    
    switch (op) {
        // conditional branch
        case OP_B_COND:
        case OP_CBNZ:
        case OP_CBZ:;
            int64_t mask_1 = bitfield_s64(insnbits, 5, 19);
            // multiply by two because each offset is 4 byte aligned
            mask_1 *= 4;
            *predicted_PC = current_PC + mask_1;
            break;
        case OP_B:
        case OP_BL:;
            int64_t mask_2 = bitfield_s64(insnbits, 0, 26);
            mask_2 *= 4;
            *predicted_PC = current_PC + mask_2;
            break;
        default:
            *predicted_PC = *seq_succ;
            break;

    }
    return;
}

/*
 * Helper function to recognize the aliased instructions:
 * LSL, LSR, CMP, and TST. We do this only to simplify the 
 * implementations of the shift operations (rather than having
 * to implement UBFM in full).
 * STUDENT TO-DO
 */

static
void fix_instr_aliases(uint32_t insnbits, opcode_t *op) {
    switch (*op) {
        case OP_UBFM:;
            uint32_t mask_1 = bitfield_u32(insnbits, 10, 6);
            if (mask_1 == 63) {
                *op = OP_LSR;
            }
            else {
                *op = OP_LSL;
            }
            break;
        case OP_ANDS_RR:;
            uint32_t mask_2 = bitfield_u32(insnbits, 0, 5);
            if (mask_2 == 31) {
                *op = OP_TST_RR;
            }
            break;
        case OP_SUBS_RR:;
            uint32_t mask_3 = bitfield_u32(insnbits, 0, 5);
            if (mask_3 == 31) {
                *op = OP_CMP_RR;
            }
            break;
        default:
            break;
    }
    return;
}

/*
 * Fetch stage logic.
 * STUDENT TO-DO:
 * Implement the fetch stage.
 * 
 * Use in as the input pipeline register,
 * and update the out pipeline register as output.
 * Additionally, update F_PC for the next
 * cycle's predicted PC.
 * 
 * You will also need the following helper functions:
 * select_pc, predict_pc, and imem.
 */

comb_logic_t fetch_instr(f_instr_impl_t *in, d_instr_impl_t *out) {
    bool imem_err = 0;
    uint64_t current_PC;
    select_PC(in->pred_PC, X_out->op, X_out->val_a, X_out->seq_succ_PC,
              M_out->op, M_out->cond_holds, M_out->seq_succ_PC, 
              &current_PC);
    /* 
     * Students: This case is for generating HLT instructions
     * to stop the pipeline. Only write your code in the **else** case. 
     */
    if (!current_PC || F_in->status == STAT_HLT) {
        out->insnbits = 0xD4400000U;
        out->op = OP_HLT;
        out->print_op = OP_HLT;
        imem_err = false;
    }
    else {
        // Students modify from here
        imem(current_PC, &(out->insnbits), &(imem_err));
        opcode_t operation = itable[bitfield_u32(out->insnbits, 21, 11)];

        if (operation == OP_CSEL && bitfield_u32(out->insnbits, 10, 2) == 1) {
            operation = OP_CSINC;
        }

        out->print_op = operation;
        out->op = operation;
        fix_instr_aliases(out->insnbits, &(out->print_op));
        out->op = out->print_op;


        predict_PC(current_PC, out->insnbits, out->op, &(F_PC), &(out->seq_succ_PC));

        if (operation == OP_ADRP) {
            out->adrp_val = current_PC - (current_PC % 4096);
        }
    }

    if (imem_err || out->op == OP_ERROR) {
        in->status = STAT_INS;
        F_in->status = in->status;
    }
    else if (out->op == OP_HLT) {
        in->status = STAT_HLT;
        F_in->status = in->status;
    }
    else {
        in->status = STAT_AOK;
    }
    out->status = in->status;

    return;
}

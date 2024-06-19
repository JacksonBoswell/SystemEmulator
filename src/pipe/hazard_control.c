/**************************************************************************
 * C S 429 system emulator
 * 
 * Bubble and stall checking logic.
 * STUDENT TO-DO:
 * Implement logic for hazard handling.
 * 
 * handle_hazards is called from proc.c with the appropriate
 * parameters already set, you must implement the logic for it.
 * 
 * You may optionally use the other three helper functions to 
 * make it easier to follow your logic.
 **************************************************************************/ 

#include "machine.h"
#include "hazard_control.h"

extern machine_t guest;
extern mem_status_t dmem_status;

/* Use this method to actually bubble/stall a pipeline stage.
 * Call it in handle_hazards(). Do not modify this code. */
void pipe_control_stage(proc_stage_t stage, bool bubble, bool stall) {
    pipe_reg_t *pipe;
    switch(stage) {
        case S_FETCH: pipe = F_instr; break;
        case S_DECODE: pipe = D_instr; break;
        case S_EXECUTE: pipe = X_instr; break;
        case S_MEMORY: pipe = M_instr; break;
        case S_WBACK: pipe = W_instr; break;
        default: printf("Error: incorrect stage provided to pipe control.\n"); return;
    }
    if (bubble && stall) {
        printf("Error: cannot bubble and stall at the same time.\n");
        pipe->ctl = P_ERROR;
    }
    // If we were previously in an error state, stay there.
    if (pipe->ctl == P_ERROR) return;

    if (bubble) {
        pipe->ctl = P_BUBBLE;
    }
    else if (stall) {
        pipe->ctl = P_STALL;
    }
    else { 
        pipe->ctl = P_LOAD;
    }
}

bool check_ret_hazard(opcode_t D_opcode) {
    if (D_opcode == OP_RET || D_opcode == OP_BR || D_opcode == OP_BLR) {
        return true;
    }
    return false;
}

bool check_mispred_branch_hazard(opcode_t X_opcode, bool X_condval) {
    if (!X_condval && (X_opcode == OP_B_COND || X_opcode == OP_CBZ || X_opcode == OP_CBNZ)) {
        return true;
    }
    return false;
}

bool check_load_use_hazard(opcode_t D_opcode, uint8_t D_src1, uint8_t D_src2,
                           opcode_t X_opcode, uint8_t X_dst) {
    if (X_opcode == OP_LDUR && (X_dst == D_src1 || X_dst == D_src2)) {
        return true;
    }
    return false;
}

comb_logic_t handle_hazards(opcode_t D_opcode, uint8_t D_src1, uint8_t D_src2, uint64_t D_val_a, 
                            opcode_t X_opcode, uint8_t X_dst, bool X_condval) {
    /* Students: Change this code */
    bool F_bubble = false; 
    bool F_stall = false; 

    bool D_bubble = false;
    bool D_stall = false; 

    bool X_bubble = false;
    bool X_stall = false;

    bool M_bubble = false;
    bool M_stall = false;

    bool W_bubble = false; 
    bool W_stall = false;


    if (W_out->status == STAT_ADR || W_out->status == STAT_INS || W_out->status == STAT_HLT) {
        F_stall = true;
        D_stall = true;
        X_stall = true;
        M_stall = true;
        W_stall = true;
    }
    else if (M_out->status == STAT_ADR || M_out->status == STAT_INS || M_out->status == STAT_HLT) {
        F_stall = true;
        D_stall = true;
        X_stall = true;
        M_stall = true;
    }
    else if (X_out->status == STAT_ADR || X_out->status == STAT_INS || X_out->status == STAT_HLT) {
        F_stall = true;
        D_stall = true;
        X_stall = true;
    }
    else if (D_out->status == STAT_ADR || D_out->status == STAT_INS || D_out->status == STAT_HLT) {
        F_stall = true;
        D_stall = true;
    }
    else if (dmem_status == IN_FLIGHT) {
        F_stall = true;
        D_stall = true;
        X_stall = true;
        M_stall = true;
        W_bubble = true;
    }
    else if (check_mispred_branch_hazard(X_opcode, X_condval)) {
        D_bubble = true;
        X_bubble = true;
    }
    else if (check_load_use_hazard(D_opcode, D_src1, D_src2, X_opcode, X_dst)) {
        F_stall = true;
        D_stall = true;
        X_bubble = true;
    }
    else if (check_ret_hazard(D_opcode)) {
        F_stall = true;
        D_bubble = true;
    }

    pipe_control_stage(S_FETCH, F_bubble, F_stall);
    pipe_control_stage(S_DECODE, D_bubble, D_stall);
    pipe_control_stage(S_EXECUTE, X_bubble, X_stall);
    pipe_control_stage(S_MEMORY, M_bubble, M_stall);
    pipe_control_stage(S_WBACK, W_bubble, W_stall);
}



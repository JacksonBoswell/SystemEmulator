/**************************************************************************
 * C S 429 system emulator
 * 
 * instr_Execute.c - Execute stage of instruction processing pipeline.
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

extern bool X_condval;

extern comb_logic_t copy_m_ctl_sigs(m_ctl_sigs_t *, m_ctl_sigs_t *);
extern comb_logic_t copy_w_ctl_sigs(w_ctl_sigs_t *, w_ctl_sigs_t *);

/*
 * Execute stage logic.
 * STUDENT TO-DO:
 * Implement the execute stage.
 * 
 * Use in as the input pipeline register,
 * and update the out pipeline register as output.
 * 
 * You will need the following helper functions:
 * copy_m_ctl_signals, copy_w_ctl_signals, and alu.
 */

comb_logic_t execute_instr(x_instr_impl_t *in, m_instr_impl_t *out) {
    out->op = in->op;
    out->print_op = in->print_op;
    out->seq_succ_PC = in->seq_succ_PC;
    out->val_b = in->val_b;
    uint64_t temp_val_b = in->X_sigs.valb_sel ? in->val_b : in->val_imm;
    uint64_t temp_val_a = (in->op == OP_BL || in->op == OP_BLR) ? in->seq_succ_PC : in->val_a;
    out->dst = in->dst;
    out->status = in->status;
    copy_m_ctl_sigs(&(out->M_sigs), &(in->M_sigs));
    copy_w_ctl_sigs(&(out->W_sigs), &(in->W_sigs));


    alu(temp_val_a, temp_val_b, in->val_hw, in->ALU_op, in->X_sigs.set_CC, in->cond, &(out->val_ex), &(out->cond_holds), &(guest.proc->NZCV));

    return;
}

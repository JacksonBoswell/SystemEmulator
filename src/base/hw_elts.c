/**************************************************************************
* C S 429 system emulator
*
* Student TODO: AE
*
* hw_elts.c - Module for emulating hardware elements.
*
* Copyright (c) 2022, 2023.
* Authors: S. Chatterjee, Z. Leeper.
* All rights reserved.
* May not be used, modified, or copied without permission.
**************************************************************************/


#include <assert.h>
#include "hw_elts.h"
#include "mem.h"
#include "machine.h"
#include "forward.h"
#include "err_handler.h"


extern machine_t guest;


comb_logic_t
imem(uint64_t imem_addr,
    uint32_t *imem_rval, bool *imem_err) {
   // imem_addr must be in "instruction memory" and a multiple of 4
   *imem_err = (!addr_in_imem(imem_addr) || (imem_addr & 0x3U));
   *imem_rval = (uint32_t) mem_read_I(imem_addr);
}


/*
* Regfile logic.
* STUDENT TO-DO:
* Read from source registers and write to destination registers when enabled.
*/
comb_logic_t
regfile(uint8_t src1, uint8_t src2, uint8_t dst, uint64_t val_w,
       // bool src1_31isSP, bool src2_31isSP, bool dst_31isSP,
       bool w_enable,
       uint64_t *val_a, uint64_t *val_b) {


  
   if (src1 == 31) {
       *val_a = (guest.proc)->SP;
   }
   else if (src1 < 31) {
       *val_a = (guest.proc)->GPR[src1];
   }
   else {
       *val_a = 0;
   }


   if (src2 == 31) {
       *val_b = (guest.proc)->SP;
   }
   else if (src2 < 31) {
       *val_b = (guest.proc)->GPR[src2];
   }
   else {
       *val_b = 0;
   }




   if (w_enable) {
       if (dst == 31) {
           (guest.proc)->SP = val_w;
       }
       else if (dst < 31) {
           (guest.proc)->GPR[dst] = val_w;
       }
   }
}


/*
* cond_holds logic.
* STUDENT TO-DO:
* Determine if the condition is true or false based on the condition code values
*/
static bool
cond_holds(cond_t cond, uint8_t ccval) {
   bool n = GET_NF(ccval);
   bool z = GET_ZF(ccval);
   bool c = GET_CF(ccval);
   bool v = GET_VF(ccval);
   switch (cond) {
       case C_EQ:
           return z == 1;
           break;
       case C_NE:
           return z == 0;
           break;
       case C_CS:
           return c == 1;
           break;
       case C_CC:
           return c == 0;
           break;
       case C_MI:
           return n == 1;
           break;
       case C_PL:
           return n == 0;
           break;
       case C_VS:
           return v == 1;
           break;
       case C_VC:
           return v == 0;
           break;
       case C_HI:
           return (c == 1 && z == 0);
           break;
       case C_LS:
           return !(c == 1 && z == 0);
           break;
       case C_GE:
           return n == v;
           break;
        case C_LT:
            return n != v;
            break;
        case C_GT:
            return n == v && z == 0;
            break;
       case C_LE:
           return !(n == v && z == 0);
           break;
       case C_AL:
       case C_NV:
           return true;;
           break;
       default:
           return false;
           break;
   }
   return false;
}


/*
* alu logic.
* STUDENT TO-DO:
* Compute the result of a bitwise or mathematial operation (all operations in alu_op_t).
* Additionally, apply hw or compute condition code values as necessary.
* Finally, compute condition values with cond_holds.
*/
comb_logic_t
alu(uint64_t alu_vala, uint64_t alu_valb, uint8_t alu_valhw, alu_op_t ALUop, bool set_CC, cond_t cond,
   uint64_t *val_e, bool *cond_val, uint8_t *nzcv) {
   uint64_t res = 0xFEEDFACEDEADBEEF;  // To make it easier to detect errors.


   switch (ALUop) {
       case MINUS_OP:
           res = alu_vala - (alu_valb << alu_valhw);
           break;
       case PLUS_OP:
           res = alu_vala + (alu_valb << alu_valhw);
           break;
       case INV_OP:
           res = alu_vala | (~alu_valb);
           break;
       case OR_OP:
           res = alu_vala | alu_valb;
           break;
       case EOR_OP:
           res = alu_vala ^ alu_valb;
           break;
       case AND_OP:
           res = alu_vala & alu_valb;
           break;
       case MOV_OP:
           res = alu_vala | (alu_valb << alu_valhw);
           break;
       case LSL_OP:
           res = alu_vala << (alu_valb & 0x3F);
           break;
       case LSR_OP:
           res = alu_vala >> (alu_valb & 0x3F);
           break;
       case ASR_OP:;
            res = ((int64_t) alu_vala) >> (alu_valb & 0x3F);
           break;
       case PASS_A_OP:
           res = alu_vala;
           break;
        case CSEL_OP:
            res = cond_holds(cond, *nzcv) ? alu_vala : alu_valb;
            break;
        case CSINV_OP:
            res = cond_holds(cond, *nzcv) ? alu_vala : ~alu_valb;
            break;
        case CSINC_OP:
            res = cond_holds(cond, *nzcv) ? alu_vala : alu_valb + 1;
            break;
        case CSNEG_OP:
            res = cond_holds(cond, *nzcv) ? alu_vala : -(alu_valb + 1);
            break;
        case CBZ_OP:
            *cond_val = (alu_vala == 0);
            break;
        case CBNZ_OP:
            *cond_val = (alu_vala != 0);
            break;
       default:
           break;
   }
   *val_e = res;

   if (ALUop == CBZ_OP || ALUop == CBNZ_OP) {
        return;
   }

   if (set_CC) {
        bool n = res >> 63;
        bool z = res == 0;
        bool c = 0;
        bool v = 0;

        switch (ALUop) {
            case MINUS_OP:
                c = alu_vala >= (alu_valb << alu_valhw);
                v = ((alu_vala ^ (alu_valb << alu_valhw)) & (alu_vala ^ res)) >> 63;
                break;
            case PLUS_OP:
                c = res < alu_vala;
                v = ((alu_vala ^ res) & (alu_valb ^ res)) >> 63;
                break;
            case AND_OP:
                break;
            default:
                n = 0;
                z = 0;
                break;
        }
        *nzcv = PACK_CC(n, z, c, v);
   }

   *cond_val = cond_holds(cond, *nzcv);
}


comb_logic_t
dmem(uint64_t dmem_addr, uint64_t dmem_wval, bool dmem_read, bool dmem_write,
    uint64_t *dmem_rval, bool *dmem_err) {
   // dmem_addr must be in "data memory" and a multiple of 8
   *dmem_err = (!addr_in_dmem(dmem_addr) || (dmem_addr & 0x7U));
   if (is_special_addr(dmem_addr)) *dmem_err = false;
   if (dmem_read) *dmem_rval = (uint64_t) mem_read_L(dmem_addr);
   if (dmem_write) mem_write_L(dmem_addr, dmem_wval);
}


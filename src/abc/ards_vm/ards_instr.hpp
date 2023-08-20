#pragma once

namespace ards
{

enum sysfunc_t : uint16_t
{
    SYS_DISPLAY,
    SYS_DRAW_PIXEL,
    SYS_DRAW_FILLED_RECT,
    SYS_SET_FRAME_RATE,
    SYS_NEXT_FRAME,
    SYS_IDLE,
    SYS_DEBUG_BREAK,

    SYS_NUM
};

enum instr_t : uint8_t
{
    I_NOP,

    I_PUSH,  // push imm
    I_P0,    // push 0
    I_P1,    // push 1
    I_P2,    // push 2
    I_P3,    // push 3
    I_P4,    // push 4
    I_P00,   // push 0; push 0

    I_SEXT,  // push 0x00 or 0xff to sign extend TOS

    I_GETL,  // push stack[top - imm] (imm=1 is TOS)
    I_GETLN, // pop N then same as GETL but push N bytes
    I_SETL,  // pop, then store to stack[top - imm]
    I_SETLN, // pop N then same as SETL but pop/store N bytes
    I_GETG,  // push globals[imm]
    I_GETGN, // pop N then same as GETG but push N bytes
    I_SETG,  // pop, then store to globals[imm]
    I_SETGN, // pop N then same as SETG but pop/store N bytes
    I_POP,   // a |
    
    I_ADD,   // a b | a+b
    I_ADD2,  // a0 a1 b0 b1 | (a+b)0 (a+b)1
    I_ADD3,  //
    I_ADD4,  //
    I_SUB,   // a b | a-b
    I_SUB2,  // a0 a1 b0 b1 | (a-b)0 (a-b)1
    I_SUB3,  //
    I_SUB4,  //

    I_MUL,
    I_MUL2,
    I_MUL3,
    I_MUL4,

    I_BOOL,  // a | (a!=0)
    I_BOOL2, // a b | (a!=0 && b!=0)
    I_BOOL3, // a b c | (a!=0 && b!=0 && c!=0)
    I_BOOL4, // a b c d | (a!=0 && b!=0 && c!=0 && d!=0)
    I_CULT,  // a b | a < b (unsigned)
    I_CULT2,
    I_CULT3,
    I_CULT4,
    I_CSLT,  // a b | a < b (signed)
    I_CSLT2,
    I_CSLT3,
    I_CSLT4,
    I_CULE,  // a b | a <= b (unsigned)
    I_CULE2,
    I_CULE3,
    I_CULE4,
    I_CSLE,  // a b | a <= b (signed)
    I_CSLE2,
    I_CSLE3,
    I_CSLE4,
    I_NOT,   // a | !a
    I_BZ,    // pop, branch if zero to imm3
    I_BNZ,   // pop, branch if nonzero to imm3
    I_JMP,   // jmp imm3
    I_CALL,
    I_RET,
    I_SYS,   // call sysfunc

    I_REMOVE, // dummy value used to indicate an optimized-out instruction
};

}

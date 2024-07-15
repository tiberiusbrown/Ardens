; Assembly code for core of instruction test. Accepts pointer to instruction
; code to test, and returns 16-bit CRC of registers after each iteration.

#define __SFR_OFFSET 0
#include <avr/io.h>

#define TESTDDR DDRB
#define TESTPORT PORTB

; Maximal-length (65535 period) and good detection of all 1-bit and most
; 2-, 3-, and 4-bit errors:
; 1 bit: 100.00%, 2 bits: 99.28%, 3 bits: 99.97%, 4 bits: 99.98%, 
crc_seed = 0xF529

	.section .bss

; Temporary stack that we fill with random data,
; then put all registers on to and checksum
			.skip 48
stack_top:	.skip 1

saved_sp:	.skip 2	; caller sp
crc:		.skip 2 ; accumulating CRC
phase:		.skip 2 ; pointers into table of data
phase2:		.skip 2
instr_addr:	.skip 2	; instruction being tested

	.section .data
	.global table
table:
	.byte 0,1,0x0F,0x10,0x11,0x7F,0x80,0x81,0xFF
table_end:
	.byte 0,1,0x0F,0x10,0x11,0x7F,0x80,0x81,0xFF
	.byte 0,1,0x0F,0x10,0x11,0x7F,0x80,0x81,0xFF
	.byte 0,1,0x0F,0x10,0x11,0x7F,0x80,0x81,0xFF
	.byte 0,1,0x0F,0x10,0x11,0x7F,0x80,0x81,0xFF
	
	.section .text

	.global test_instr
test_instr:
	sts instr_addr+0,r24
	sts instr_addr+1,r25
	
	; Save registers
	ldi	r30,1
	ldi	r31,0
save_regs:
	ld	r0,Z+
	push r0
	cpi r30,30
	brne save_regs
	
	; Save SP
	in	r30,SPL
	in	r31,SPH
	sts	saved_sp+0,r30
	sts	saved_sp+1,r31
	
	; Clear CRC
	ldi	r30,0
	sts	crc+0,r30
	sts	crc+1,r30
	
	; Loop through various register values
	ldi	r30,lo8(table)
	ldi	r31,hi8(table)
1:	sts	phase+0,r30
	sts	phase+1,r31
	ldi	r28,lo8(table)
	ldi	r29,hi8(table)
2:	sts	phase2+0,r28
	sts	phase2+1,r29
	rjmp run_instr
	
run_instr_ret:
	; Next combination of values
	lds	r28,phase2+0
	lds	r29,phase2+1
	lds	r30,phase+0
	lds	r31,phase+1
	
	adiw r28,1
	cpi	r28,lo8(table_end)
	brne 2b
	
	adiw r30,1
	cpi	r30,lo8(table_end)
	brne 1b
	
	; Restore SP
	lds	r30,saved_sp+0
	lds	r31,saved_sp+1
	out	SPL,r30
	out	SPH,r31
	
	; Restore registers
	ldi	r30,30
	ldi	r31,0
1:	pop r0
	st	-Z,r0
	cpi r30,1
	brne 1b
	
	; Result
	lds r24,crc+0
	lds r25,crc+1
	
	ret

; Y=phase2
; Z=phase
run_instr:
	; Set up registers and stack with values from table
	; r26 and r1 get values from phase2, so that every combination
	; of values comes up between them and the other registers
	
	; phase2 values
	ld	r26,Y+
	ld	r1,Y+
	
	; Use our stack
	ldi	r28,lo8(stack_top)
	ldi	r29,hi8(stack_top)
	out	SPL,r28
	out	SPH,r29
	
	; Fill stack
	ld	r0,Z+
	push r0
	ld	r0,Z+
	push r0
	lds	r28,instr_addr+0 ; we ret to this to run instr
	lds r29,instr_addr+1
	push r28
	push r29
	ld	r0,Z+			; values for r30 and r31 later
	push r0
	ld	r0,Z+
	push r0
	
	; Fill r2-r27
	ldi	r28,2
	ldi	r29,0
1:	ld	r0,Z+
	st	Y+,r0
	cpi	r28,28
	brne 1b
	
	; TESTPORT
	ld	r28,Z+
	out	TESTPORT,r28
	
	lds	r28,instr_addr+0
	lds r29,instr_addr+1
	subi r28,lo8(pm(readonly_instrs))
	sbci r29,hi8(pm(readonly_instrs))
	brlo mem_setup

	; Status flags
	ld	r28,Z+
	out	SREG,r28
	
	; r28-r31
	ld	r28,Z+
	ld	r29,Z+
	pop r30
	pop r31
	
	; r0 has whatever was left from fill loop above, which is fine
	
	; Run instruction
	ret

mem_setup:
	; Status flags
	ld	r28,Z+
	out	SREG,r28
	
	pop r0		; we don't fill r30/r31 with these so pop now
	pop r0
	
	; X/Y/Z = 1/2/3
	ldi	r26,1
	ldi	r27,0
	ldi	r28,2
	ldi	r29,0
	ldi	r30,3
	ldi	r31,0
	
	; Run instruction
	ret
	
	inc r0	; catch off-by-one errors for jumps
instr_done:
	; Push misc registers
	push r31
	push r30
	push r29
	in	r29,SREG
	push r29
	in	r29,TESTPORT
	push r29
	
	; Push rest of registers
	ldi	r30,0
	ldi	r31,0
1:	ld	r29,Z+
	push r29
	cpi r30,29
	brne 1b
	
	; Checksum stack
	lds r28,crc+0
	lds r29,crc+1
1:	pop r27
	add	r28,r27
	adc r29,r27
	lsl r28
	rol r29
	brcs 2f
	subi r28,lo8(-crc_seed)
	sbci r29,hi8(-crc_seed)
2:	in r27,SPL
	cpi	r27,lo8(stack_top)
	brne 1b
	sts crc+0,r28
	sts crc+1,r29
	
	rjmp run_instr_ret
	
lpm_data:
	.byte 0,1,2,3

test_lpm:
	ldi r30,lo8(lpm_data+1)
	ldi r31,hi8(lpm_data+1)
	ret

test_lpm_end:
	sts stack_top,r29
	in r29,SREG
	subi r30,lo8(lpm_data+1)
	sbci r31,hi8(lpm_data+1)
	add r20,r30
	add r21,r31
	rjmp instr_done
	
	.global instrs
	.global instrs_end
instrs:

#define op(...) \
	__VA_ARGS__ $\
	rjmp instr_done

#define op_lpm(...) \
	rcall test_lpm $\
	__VA_ARGS__ $\
	rjmp test_lpm_end $\
	nop

	op_lpm(lpm)
	op_lpm(lpm r26,Z)
	op_lpm(lpm r26,Z+)

#define op32(...) \
	__VA_ARGS__ $\
	rjmp instr_done $\
	nop

	op32(lds r24,0x15)
	op(ld r24,X)
	op(ld r24,X+)
	op(ld r24,-X)
	op(ld r26,Y)
	op(ld r26,Y+)
	op(ld r26,-Y)
	op(ldd r26,Y+5)
	op(ld r26,Z)
	op(ld r26,Z+)
	op(ld r26,-Z)
	op(ldd r26,Z+5)
	
	op32(sts 0x15,r24)
	op(st X,r24)
	op(st X+,r24)
	op(st -X,r24)
	op(st Y,r26)
	op(st Y+,r26)
	op(st -Y,r26)
	op(std Y+5,r26)
	op(st Z,r26)
	op(st Z+,r26)
	op(st -Z,r26)
	op(std Z+5,r26)
	
#define op_r5(o) \
	op(o r26) $\
	op(o r1)

	op_r5(push)
	op_r5(pop)

readonly_instrs:

	op(nop)
	op_r5(swap)
	op_r5(neg)
	op_r5(dec)
	op_r5(inc)
	op_r5(com)
	op_r5(asr)
	op_r5(lsr)
	op_r5(ror)

	op(movw r24,r26)
	op(movw r0,r2)

#define op_r5_r5(o) \
	op(o r26,r24) $\
	op(o r1,r3)

	op_r5_r5(mov)
	op_r5_r5(cp)
	op_r5_r5(cpc)
	op_r5_r5(add)
	op_r5_r5(adc)
	op_r5_r5(sub)
	op_r5_r5(sbc)
	op_r5_r5(and)
	op_r5_r5(eor)
	op_r5_r5(or)

#define op_r4_k8(o) \
	op(o r26,0) $\
	op(o r26,1) $\
	op(o r26,0x0F) $\
	op(o r26,0x10) $\
	op(o r26,0x7F) $\
	op(o r26,0x80) $\
	op(o r26,0x81) $\
	op(o r26,0xFF)

	op_r4_k8(ldi)
	op_r4_k8(cpi)
	op_r4_k8(subi)
	op_r4_k8(sbci)
	op(andi r26,0x55)
	op(andi r26,0xAA)
	op(ori r26,0x55)
	op(ori r26,0xAA)
	op(adiw r26,1)
	op(adiw r26,32)
	op(sbiw r26,1)
	op(sbiw r26,32)
	
#define op_mul(o) \
	mov r23,r26 $\
	op32(o r23,r16)

	op_mul(mul)
	op_mul(muls)
	op_mul(mulsu)
	op_mul(fmul)
	op_mul(fmuls)
	op_mul(fmulsu)
	
#define op_bit(o) \
	op(o 0) $\
	op(o 1) $\
	op(o 2) $\
	op(o 3) $\
	op(o 4) $\
	op(o 5) $\
	op(o 6) $\
	op(o 7)

	op_bit(bclr)
	op_bit(bset)
	op(bld r26,0)
	op(bld r26,7)
	op(bld r1,0)
	op(bst r26,0)
	op(bst r26,7)
	op(bst r1,0)
	op(cbi TESTPORT,0)
	op(cbi TESTPORT,7)
	op(sbi TESTPORT,0)
	op(sbi TESTPORT,7)
	op(in r26,TESTPORT)
	op(in r1,TESTPORT)
	op(out TESTPORT,r26)
	op(out TESTPORT,r1)
	
#define op_skip(...) \
	__VA_ARGS__ $\
	com r1 $\
	rjmp instr_done $\
	nop
	
	op_skip(sbic TESTPORT,7)
	op_skip(sbic TESTPORT,7)
	op_skip(sbis TESTPORT,0)
	op_skip(sbis TESTPORT,7)
	op_skip(sbrc r1,0)
	op_skip(sbrc r1,7)
	op_skip(sbrc r24,0)
	op_skip(sbrs r1,0)
	op_skip(sbrs r1,7)
	op_skip(sbrs r24,0)
	op_skip(cpse r24,r26)
	op_skip(cpse r1,r3)
	
#define op_br(o) \
	op(o 0,taken) $\
	op(o 1,taken) $\
	op(o 2,taken) $\
	op(o 3,taken) $\
	op(o 4,taken) $\
	op(o 5,taken) $\
	op(o 6,taken) $\
	op(o 7,taken)

	op_br(brbc)
	op_br(brbs)
	
	op(rjmp instr_done)
	
	rcall call_done
call_instr:
	rjmp instr_done

	ldi r30,lo8(pm(ijmp_done))
	ldi r31,hi8(pm(ijmp_done))
	ijmp
	nop
	
	ldi r30,lo8(pm(icall_done))
	ldi r31,hi8(pm(icall_done))
	icall
icall_instr:
	nop
	
	op(rcall test_ret)
	op(rcall test_reti)

instrs_end:

taken:
	com r26
	rjmp instr_done

test_ret:
	ret

test_reti:
	reti

ijmp_done:
	sts stack_top,r29
	in r29,SREG
	subi r30,lo8(pm(ijmp_done))
	sbci r31,hi8(pm(ijmp_done))
	rjmp instr_done
	
icall_done:
	sts stack_top,r29
	in r29,SREG
	subi r30,lo8(pm(icall_done))
	sbci r31,hi8(pm(icall_done))
	add r20,r30
	add r21,r31
	
	pop r31
	pop r30
	subi r30,lo8(pm(icall_instr))
	sbci r31,hi8(pm(icall_instr))
	rjmp instr_done
	
call_done:
	sts stack_top,r29
	in r29,SREG
	add r20,r30
	add r21,r31
	
	pop r31
	pop r30
	subi r30,lo8(pm(call_instr))
	sbci r31,hi8(pm(call_instr))
	rjmp instr_done

/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifdef	__ASSEMBLER__

/* Local label name for asm code. */
#ifndef L
#define L(name)		.L##name
#endif

#define __NR_sigprocmask 0x7e

#define oLINK           4
#define oSS_SP          8
#define oSS_SIZE        16
#define oGS             20
#define oFS             24
#define oEDI            36
#define oESI            40
#define oEBP            44
#define oESP            48
#define oEBX            52
#define oEDX            56
#define oECX            60
#define oEAX            64
#define oEIP            76
#define oFPREGS         96
#define oSIGMASK        108
#define oFPREGSMEM      236

#define SIG_BLOCK       0	/* for blocking signals */
#define SIG_UNBLOCK     1	/* for unblocking signals */
#define SIG_SETMASK     2	/* for setting the signal mask */

# define ENTER_KERNEL int $0x80


# define cfi_startproc			.cfi_startproc
# define cfi_endproc			.cfi_endproc

# define cfi_def_cfa(reg, off)		.cfi_def_cfa reg, off
# define cfi_def_cfa_register(reg)	.cfi_def_cfa_register reg
# define cfi_def_cfa_offset(off)	.cfi_def_cfa_offset off
# define cfi_adjust_cfa_offset(off)	.cfi_adjust_cfa_offset off
# define cfi_offset(reg, off)		.cfi_offset reg, off
# define cfi_rel_offset(reg, off)	.cfi_rel_offset reg, off
# define cfi_register(r1, r2)		.cfi_register r1, r2
# define cfi_return_column(reg)	.cfi_return_column reg
# define cfi_restore(reg)		.cfi_restore reg
# define cfi_same_value(reg)		.cfi_same_value reg
# define cfi_undefined(reg)		.cfi_undefined reg
# define cfi_remember_state		.cfi_remember_state
# define cfi_restore_state		.cfi_restore_state
# define cfi_window_save		.cfi_window_save
# define cfi_personality(enc, exp)	.cfi_personality enc, exp
# define cfi_lsda(enc, exp)		.cfi_lsda enc, exp


#define END2(name)							      \
  cfi_endproc;								      \
  ASM_SIZE_DIRECTIVE(name)


#ifndef C_LABEL2

/* Define a macro we can use to construct the asm name for a C symbol.  */
# define C_LABEL2(name)	name##:

#endif

/* Used by some assembly code.  */
#define C_SYMBOL_NAME(name)	name

/* Syntactic details of assembler.  */

/* ELF uses byte-counts for .align, most others use log2 of count of bytes.  */
#define ALIGNARG(log2) 1<<log2
#define ASM_SIZE_DIRECTIVE(name) .size name,.-name;


/* Define an entry point visible from C.

   There is currently a bug in gdb which prevents us from specifying
   incomplete stabs information.  Fake some entries here which specify
   the current source file.  */
#define	ENTRY2(name)							      \
  .globl C_SYMBOL_NAME(name);						      \
  .type C_SYMBOL_NAME(name),@function;					      \
  .align ALIGNARG(4);							      \
  C_LABEL2(name)								      \
  cfi_startproc;							      
  
#undef	END2
#define END2(name)							      \
  cfi_endproc;								      \
  ASM_SIZE_DIRECTIVE(name)

#endif


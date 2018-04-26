#include "swap.h"


ENTRY2(swapcontext_safe)
	/* Load address of the context data structure we save in.  */
	movl	4(%esp), %eax

	/* Return value of swapcontext.  EAX is the only register whose
	   value is not preserved.  */
	movl	$0, oEAX(%eax)

	/* Save the 32-bit register values and the return address.  */
	movl	%ecx, oECX(%eax)
	movl	%edx, oEDX(%eax)
	movl	%edi, oEDI(%eax)
	movl	%esi, oESI(%eax)
	movl	%ebp, oEBP(%eax)
	movl	(%esp), %ecx
	movl	%ecx, oEIP(%eax)
	leal	4(%esp), %ecx
	movl	%ecx, oESP(%eax)
	movl	%ebx, oEBX(%eax)

	/* Save the FS segment register.  */
	xorl	%edx, %edx
	movw	%fs, %dx
	movl	%edx, oFS(%eax)

	/* We have separate floating-point register content memory on the
	   stack.  We use the __fpregs_mem block in the context.  Set the
	   links up correctly.  */
	leal	oFPREGSMEM(%eax), %ecx
	movl	%ecx, oFPREGS(%eax)
	/* Save the floating-point context.  */
	fnstenv	(%ecx)

	/* Load address of the context data structure we have to load.  */
	movl	8(%esp), %ecx

	/* Save the current signal mask and install the new one.  */
	pushl	%ebx
	leal	oSIGMASK(%eax), %edx
	leal	oSIGMASK(%ecx), %ecx
	movl	$SIG_SETMASK, %ebx
	movl	$__NR_sigprocmask, %eax
	ENTER_KERNEL
	popl	%ebx
	cmpl	$-4095, %eax		/* Check %eax for error.  */
	jae	syscall_error_label	/* Jump to error handler if error.  */
    
	/* EAX was modified, reload it.  */
	movl	8(%esp), %eax

	/* Restore the floating-point context.  Not the registers, only the
	   rest.  */
	movl	oFPREGS(%eax), %ecx
	fldenv	(%ecx)

	/* Restore the FS segment register.  We don't touch the GS register
	   since it is used for threads.  */
	movl	oFS(%eax), %edx
	movw	%dx, %fs

	/* Fetch the address to return to.  */
	movl	oEIP(%eax), %ecx

	/* Load the new stack pointer.  */
	movl	oESP(%eax), %esp
	

	/* Push the return address on the new stack so we can return there.  */
	pushl	%ecx

	/* Load the values of all the 32-bit registers (except ESP).
	   Since we are loading from EAX, it must be last.  */
	movl	oEDI(%eax), %edi
	movl	oESI(%eax), %esi
	movl	oEBP(%eax), %ebp
	movl	oEBX(%eax), %ebx
	movl	oEDX(%eax), %edx
	movl	oECX(%eax), %ecx
	movl	oEAX(%eax), %eax

    jmp    unlock_spin

	/* The following 'ret' will pop the address of the code and jump
	   to it.  */
	ret
END2(swapcontext_safe)



#if 0
ENTRY2(getcontext2)
	/* Load address of the context data structure.  */
	movl	4(%esp), %eax

	/* Return value of getcontext.  EAX is the only register whose
	   value is not preserved.  */
	movl	$0, oEAX(%eax)

	/* Save the 32-bit register values and the return address.  */
	movl	%ecx, oECX(%eax)
	movl	%edx, oEDX(%eax)
	movl	%edi, oEDI(%eax)
	movl	%esi, oESI(%eax)
	movl	%ebp, oEBP(%eax)
	movl	(%esp), %ecx
	movl	%ecx, oEIP(%eax)
	leal	4(%esp), %ecx		/* Exclude the return address.  */
	movl	%ecx, oESP(%eax)
	movl	%ebx, oEBX(%eax)

	/* Save the FS segment register.  We don't touch the GS register
	   since it is used for threads.  */
	xorl	%edx, %edx
	movw	%fs, %dx
	movl	%edx, oFS(%eax)

	/* We have separate floating-point register content memory on the
	   stack.  We use the __fpregs_mem block in the context.  Set the
	   links up correctly.  */
	leal	oFPREGSMEM(%eax), %ecx
	movl	%ecx, oFPREGS(%eax)
	/* Save the floating-point context.  */
	fnstenv	(%ecx)
	/* And load it right back since the processor changes the mask.
	   Intel thought this opcode to be used in interrupt handlers which
	   would block all exceptions.  */
	fldenv	(%ecx)

	/* Save the current signal mask.  */
	pushl	%ebx
	cfi_adjust_cfa_offset (4)
	cfi_rel_offset (ebx, 0)
	leal	oSIGMASK(%eax), %edx
	xorl	%ecx, %ecx
	movl	$SIG_BLOCK, %ebx
	movl	$__NR_sigprocmask, %eax
	ENTER_KERNEL
	popl	%ebx
	cfi_adjust_cfa_offset (-4)
	cfi_restore (ebx)
	cmpl	$-4095, %eax		/* Check %eax for error.  */
	jae	syscall_error_label	/* Jump to error handler if error.  */

	/* All done, return 0 for success.  */
	xorl	%eax, %eax
	ret
END2(getcontext2)

ENTRY2(setcontext2)
    /* Load address of the context data structure.  */
    movl    4(%esp), %eax

    /* Get the current signal mask.  Note that we preserve EBX in case
       the system call fails and we return from the function with an
       error.  */
    pushl   %ebx
    cfi_adjust_cfa_offset (4)
    xorl    %edx, %edx
    leal    oSIGMASK(%eax), %ecx
    movl    $SIG_SETMASK, %ebx
    cfi_rel_offset (ebx, 0)
    movl    $__NR_sigprocmask, %eax
    ENTER_KERNEL
    popl    %ebx
    cfi_adjust_cfa_offset (-4)
    cfi_restore (ebx)
    cmpl    $-4095, %eax        /* Check %eax for error.  */
    jae syscall_error_label /* Jump to error handler if error.  */

    /* EAX was modified, reload it.  */
    movl    4(%esp), %eax

    /* Restore the floating-point context.  Not the registers, only the
       rest.  */
    movl    oFPREGS(%eax), %ecx
    fldenv  (%ecx)

    /* Restore the FS segment register.  We don't touch the GS register
       since it is used for threads.  */
    movl    oFS(%eax), %ecx
    movw    %cx, %fs

    /* Fetch the address to return to.  */
    movl    oEIP(%eax), %ecx

    /* Load the new stack pointer.  */
    cfi_def_cfa (eax, 0)
    cfi_offset (edi, oEDI)
    cfi_offset (esi, oESI)
    cfi_offset (ebp, oEBP)
    cfi_offset (ebx, oEBX)
    cfi_offset (edx, oEDX)
    cfi_offset (ecx, oECX)
    movl    oESP(%eax), %esp

    /* Push the return address on the new stack so we can return there.  */
    pushl   %ecx

    /* Load the values of all the 32-bit registers (except ESP).
       Since we are loading from EAX, it must be last.  */
    movl    oEDI(%eax), %edi
    movl    oESI(%eax), %esi
    movl    oEBP(%eax), %ebp
    movl    oEBX(%eax), %ebx
    movl    oEDX(%eax), %edx
    movl    oECX(%eax), %ecx
    movl    oEAX(%eax), %eax

    /* End FDE here, we fall into another context.  */
    cfi_endproc
    cfi_startproc

    /* The following 'ret' will pop the address of the code and jump
       to it.  */

    ret
END2(setcontext2)


ENTRY2(makecontext2)

    movl    4(%esp), %eax

    /* Load the address of the function we are supposed to run.  */
    movl    8(%esp), %ecx

    /* Compute the address of the stack.  The information comes from
       to us_stack element.  */
    movl    oSS_SP(%eax), %edx
    movl    %ecx, oEIP(%eax)
    addl    oSS_SIZE(%eax), %edx

    /* Remember the number of parameters for the exit handler since
       it has to remove them.  We store the number in the EBX register
       which the function we will call must preserve.  */
    movl    12(%esp), %ecx
    movl    %ecx, oEBX(%eax)

    /* Make room on the new stack for the parameters.
       Room for the arguments, return address (== L(exitcode)) and
       oLINK pointer is needed.  One of the pointer sizes is subtracted
       after aligning the stack.  */
    negl    %ecx
    leal    -4(%edx,%ecx,4), %edx
    negl    %ecx

    /* Align the stack.  */
    andl    $0xfffffff0, %edx
    subl    $4, %edx

    /* Store the future stack pointer.  */
    movl    %edx, oESP(%eax)

    /* Put the next context on the new stack (from the uc_link
       element).  */
    movl    oLINK(%eax), %eax
    movl    %eax, 4(%edx,%ecx,4)

    /* Copy all the parameters.  */
    jecxz   2f
1:  movl    12(%esp,%ecx,4), %eax
    movl    %eax, (%edx,%ecx,4)
    decl    %ecx
    jnz 1b
2:

    /* If the function we call returns we must continue with the
       context which is given in the uc_link element.  To do this
       set the return address for the function the user provides
       to a little bit of helper code which does the magic (see
       below).  */
#ifdef PIC
    call    1f
    cfi_adjust_cfa_offset (4)
1:  popl    %ecx
    cfi_adjust_cfa_offset (-4)
    addl    $L(exitcode)-1b, %ecx
    movl    %ecx, (%edx)
#else
    movl    $L(exitcode), (%edx)
#endif
    /* We need to terminate the FDE here instead of after ret because
       the unwinder looks at ra-1 for unwind information.  */
    cfi_endproc

    /* 'makecontext' returns no value.  */
    ret

    /* This is the helper code which gets called if a function which
       is registered with 'makecontext' returns.  In this case we
       have to install the context listed in the uc_link element of
       the context 'makecontext' manipulated at the time of the
       'makecontext' call.  If the pointer is NULL the process must
       terminate.  */
L(exitcode):
    /* This removes the parameters passed to the function given to
       'makecontext' from the stack.  EBX contains the number of
       parameters (see above).  */
    leal    (%esp,%ebx,4), %esp

    cmpl    $0, (%esp)      /* Check the next context.  */
    je  2f          /* If it is zero exit.  */

    call    setcontext2
    /* If this returns (which can happen if the syscall fails) we'll
       exit the program with the return error value (-1).  */

    movl    %eax, (%esp)
2: 
    /* The 'exit' call should never return.  In case it does cause
       the process to terminate.  */
    hlt
    cfi_startproc
END2(makecontext2)
#endif


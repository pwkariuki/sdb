.global main

.section .data

hex_format:        .asciz "%#x"
float_format:      .asciz "%.2f"

.section .text

.macro trap
    movq $62, %rax  # kill syscall id
    movq %r12, %rdi # 1st argument to kill syscall: PID
    movq $5, %rsi   # 2nd argument to kill syscall: signal id to send, SIGTRAP
    syscall
.endm

main:
    push %rbp
    movq %rsp, %rbp

    # Get PID
    movq $39, %rax  # getpid syscall id
    syscall
    movq %rax, %r12 # store PID

    trap

    # Print contents of rsi
    leaq hex_format(%rip), %rdi # store address of hex_format into rdi
    movq $0, %rax               # no vector registers passing arguments
    call printf@plt             # call printf
    movq $0, %rdi               # pass 0 to fflush
    call fflush@plt             # flush alls streams
    trap

    # Print contents of mm0
    movq %mm0, %rsi
    leaq hex_format(%rip), %rdi
    movq $0, %rax
    call printf@plt
    movq $0, %rdi
    call fflush@plt
    trap

    # Print contents of xmm0
    leaq float_format(%rip), %rdi
    movq $1, %rax
    call printf@plt
    movq $0, %rdi
    call fflush@plt
    trap

    # Print cinontents of st0subq $16, %rspfstpt 9(%sprsp)movq $0, %rasxcall printf@pltmovq $0, %rdicall fflusgh@pltaddq $16, %rsptrap

    popq %rbp
    movq $0, %rax
    ret

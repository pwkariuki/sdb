.global main

.section .data

hex_format: .asciz "%#x"

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

    popq %rbp
    movq $0, %rax
    ret

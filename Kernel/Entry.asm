OPTION CASEMAP:NONE

.code

PUBLIC _start
EXTERN kernel_main:PROC

_start PROC
    cli
    cld

    ; Preserve the fifth argument: pixel format.
    ; Microsoft x64 passes argument five at [rsp + 40].
    mov r10d, DWORD PTR [rsp + 40]

    ; Reserve shadow space and align the stack for the C call.
    sub rsp, 40

    ; Restore the fifth argument into its correct stack position.
    mov DWORD PTR [rsp + 32], r10d

    ; Call the C kernel using the Microsoft x64 ABI.
    call kernel_main

halt_loop:
    cli
    hlt
    jmp halt_loop

_start ENDP

END
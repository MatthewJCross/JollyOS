OPTION CASEMAP:NONE

.code

PUBLIC _start
EXTERN kernel_main:PROC

_start PROC
    cli
    cld

    ; RCX already contains the JOLLYOS_BOOT_INFO pointer.
    ; Reserve 32 bytes of Microsoft x64 shadow space and
    ; align the stack for the C function call.
    sub rsp, 40

    ; Call the C kernel using the Microsoft x64 ABI.
    call kernel_main

    ; The kernel should never return.
halt_loop:
    cli
    hlt
    jmp halt_loop

_start ENDP

END
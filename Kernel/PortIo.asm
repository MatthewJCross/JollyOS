OPTION CASEMAP:NONE

.code

PUBLIC IoRead8
PUBLIC IoWrite8

IoRead8 PROC
    mov dx, cx
    in al, dx
    ret
IoRead8 ENDP

IoWrite8 PROC
    mov r8b, dl
    mov dx, cx
    mov al, r8b
    out dx, al
    ret
IoWrite8 ENDP

END
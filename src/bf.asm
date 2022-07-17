bits 64

; parameters - rdi, rsi, rdx, rcx, r8, r9
; preserve - rbx, rsp, rbp, r12, r13, r14, r15
; scratch - rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11
; return - rdx:rax

; void bfwrite(int c);
; int bfread();
extern bfwrite
extern bfread

section .text

nextrbrack:
    mov al, [rdi + rsi]
    cmp al, ']'
    je .found
    cmp al, '['
    je .dive
    inc rsi
    jmp nextrbrack
.dive:
    inc rsi
    call nextrbrack
    mov rsi, rax
    inc rsi
    jmp nextrbrack
.found:
    mov rax, rsi
    ret

prevlbrack:
    mov al, [rdi + rsi]
    cmp al, '['
    je .found
    cmp al, ']'
    je .dive
    dec rsi
    jmp prevlbrack
.dive:
    dec rsi
    call prevlbrack
    mov rsi, rax
    dec rsi
    jmp prevlbrack
.found:
    mov rax, rsi
    ret

; int interpret2(char *buf, int ip);
global interpret2
interpret2:
    
    ; prologue
    push rbp
    mov rbp, rsp

    ; r12 - source string
    ; r13 - instruction pointer
    ; r14b - source character
    ; r15 - data pointer
    sub rsp, 16 * 2
    mov [rbp - 8], r12
    mov [rbp - 16], r13
    mov [rbp - 24], r14
    mov [rbp - 32], r15
    mov r12, rdi
    mov r13, rsi

    mov rcx, 16 * 1875 ; 30000 bytes for data
.zerodata:
    dec rsp
    mov byte [rsp], 0
    loop .zerodata
    mov r15, rsp

.instrloop:
    mov r14b, byte [r12 + r13]
    cmp r14b, 0
    jz .instrloopend

; loop body
.casegt:
    cmp r14b, '>'
    jne .caselt
    inc r15
    jmp .instrlooptail

.caselt:
    cmp r14b, '<'
    jne .caseplus
    dec r15
    jmp .instrlooptail

.caseplus:
    cmp r14b, '+'
    jne .caseminus
    inc byte [r15]
    jmp .instrlooptail

.caseminus:
    cmp r14b, '-'
    jne .caseperiod
    dec byte [r15]
    jmp .instrlooptail

.caseperiod:
    cmp r14b, '.'
    jne .casecomma
    movzx rdi, byte [r15]
    call bfwrite
    jmp .instrlooptail

.casecomma:
    cmp r14b, ','
    jne .caselbrack
    call bfread
    mov [r15], al
    jmp .instrlooptail

.caselbrack:
    cmp r14b, '['
    jne .caserbrack
    cmp byte [r15], 0
    jnz .instrlooptail
    ; search right bracket
    ; recursive search
    ; inc r13
    ; mov rdi, r12
    ; mov rsi, r13
    ; call nextrbrack
    ; mov r13, rax
    ; inline search
    mov rdx, 0
    .srchrb:
    inc r13
    mov r14b, [r12 + r13]
    .srchrb.cmprb:
        cmp r14b, ']'
        jne .srchrb.cmplb
        cmp rdx, 0
        jz .instrlooptail
        dec rdx
        jmp .srchrb
    .srchrb.cmplb:
        cmp r14b, '['
        jne .srchrb
        inc rdx
        jne .srchrb
    ; not reachable
    jmp .instrlooptail

.caserbrack:
    cmp r14b, ']'
    jne .instrlooptail
    cmp byte [r15], 0
    jz .instrlooptail
    ; search left bracket
    ; recursive search
    ; dec r13
    ; mov rdi, r12
    ; mov rsi, r13
    ; call prevlbrack
    ; mov r13, rax
    ; inline search
    mov rdx, 0
    .srchlb: 
    dec r13
    mov r14b, [r12 + r13]
    .srchlb.cmplb:
        cmp r14b, '['
        jne .srchlb.cmprb
        cmp rdx, 0
        jz .instrlooptail
        dec rdx
        jmp .srchlb
    .srchlb.cmprb:
        cmp r14b, ']'
        jne .srchlb
        inc rdx
        jne .srchlb
    ; not reachable
    jmp .instrlooptail

.instrlooptail:
    inc r13
    jmp .instrloop
.instrloopend:

    mov r12, [rbp - 8]
    mov r13, [rbp - 16]
    mov r14, [rbp - 24]
    mov r15, [rbp - 32]

    mov rax, 0

    ; epilogue
    mov rsp, rbp
    pop rbp
    ret
     
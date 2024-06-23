[bits 64]

kernel_space_virtual_base_addr equ 0xFFFFF00000000000

; Integer registers

; System V ABI Description:
; Return value:
;   rax

; Arguments:
;   rdi
;   rsi
;   rdx
;   rcx
;   r8
;   r9

; Caller saved:
;   r10
;   r11

; Callee saved:
;   rbx
;   rbp
;   r12
;   r13
;   r14
;   r15

section .text
    global _start
_start:

    ; Zero general-purpose registers
    xor rax, rax

    xor rsi, rsi
    xor rdx, rdx
    xor rcx, rcx
    xor r8, r8
    xor r9, r9

    xor rbx, rbx
    xor rbp, rbp

    ; r8 stores pointer that points to machine_info struct
    mov r8, rdi

    ; get ccldr base addr
    mov r9, qword [r8 + 0x10]
    ; get ccldr size
    mov rbp, r9
    add rbp, 0x10000


    ; Set temporary kernel stack pointer
    ; rsp -> ccldr base + ccldr size
    mov rsp, rbp

    ; ccldr can map 2GBytes maximum physical memory;
    mov rdi, qword [r8 + 0x8]   ; get kernel space size
    push rdi                    ; kernel space size stored at [rbp - 0x8]

    ; 0x80000000 eq 2 GBytes
    mov rsi, 0x80000000

    ; if kernel space size >= 2 GBytes
    cmp rdi, rsi
    cmovnb rdi, rsi
    ; so we need to update kernel space size in machine_info
    mov qword [r8 + 0x8], rdi



    ; Determine how to construct ccldr page table based on it's base address
    ; calculate pml4_base
    add r9, 0x80000         ; ccldr base + 128 KBytes
    push r9                 ; pml4_base stored at [rbp - 0x10]

    ; calculate ccldr number of ptes
    push 0                  ; number of ptes of ccldr at [rbp - 0x18]
    push 0                  ; number of pdes of ccldr at [rbp - 0x20]
    push 0                  ; number of pdptes of ccldr at [rbp - 0x28]
    push 0                  ; number of pml4es of ccldr at [rbp - 0x30]
    mov rdi, qword [r8 + 0x18]
    mov rsi, qword [r8 + 0x10]
    mov rdx, rsp
    call func_calculate_num_of_ptes;

    ; calculate krnl number of ptes
    push 0                  ; number of ptes of krnl at [rbp - 0x38]
    push 0                  ; number of pdes of krnl at [rbp - 0x40]
    push 0                  ; number of pdptes of krnl at [rbp - 0x48]
    push 0                  ; number of pml4es of krnl at [rbp - 0x50]
    mov rdi, qword [r8 + 0x8]
    mov rsi, qword [r8]
    mov rdx, rsp
    call func_calculate_num_of_ptes;

    ; calculate ccldr_pdpt_base
    mov rdx, qword[rbp - 0x10] ; get pml4_base   
    add rdx, 0x1000             ; ccldr_pdpt_base = pml4_base + 0x1000
    push rdx                ; ccldr_pdpt_base at [rbp - 0x58]

    ; calculate krnl_pdpt_base
    mov rcx, qword [rbp - 0x30] ; get num_of_pml4es_of_ccldr
    shl rcx, 12
    add rdx, rcx            ; krnl_pdpt_base = (num_of_pml4es_of_ccldr) * 0x1000 + ccldr_pdpt_base
    push rdx                ; krnl_pdpt_base at [rbp - 0x60]

    ; calculate ccldr_pd_base
    mov rcx, qword [rbp - 0x50] ; get num_of_pml4es_of_krnl
    shl rcx, 12
    add rdx, rcx            ; ccldr_pd_base = (num_of_pml4es_of_krnl) * 0x1000 + krnl_pdpt_base
    push rdx                ; ccldr_pd_base at [rbp - 0x68]

    ; calculate krnl_pd_base
    mov rcx, qword [rbp - 0x28] ; get num_of_pdptes_of_ccldr
    shl rcx, 12
    add rdx, rcx            ; krnl_pd_base = (num_of_pdptes_of_ccldr) * 0x1000 + ccldr_pd_base
    push rdx                ; krnl_pd_base at [rbp - 0x70]

    ; calculate ccldr_pt_base
    mov rcx, qword [rbp - 0x40] ; get num_of_pdptes_of_krnl
    shl rcx, 12
    add rdx, rcx            ; ccldr_pt_base = (num_of_pdptes_of_krnl) * 0x1000 + krnl_pd_base
    push rdx                ; ccldr_pt_base at [rbp - 0x78]

    ; calculate krnl_pt_base
    mov rcx, qword [rbp - 0x20] ; get num_of_pdes_of_ccldr 
    shl rcx, 12
    add rdx, rcx
    push rdx                ; krnl_pt_base at [rbp - 0x80]


    ; Set PML4

    ; for ccldr
    ; locate offset of ccldr pdpt from beginning of pml4
    ; get ccldr base addr
    mov rbx, qword [r8 + 0x10]
    shr rbx, 39
    and rbx, 0x1FF
    shl rbx, 3
    mov rdi, qword[rbp - 0x10]
    add rdi, rbx            ; rdi: PML4[offset_of_ccldr]

    mov rsi, qword [rbp - 0x58] ; rsi: ccldr_pdpt_base

    mov rcx, qword [rbp - 0x30]              ; rcx: number of pml4es of ccldr

    call func_fill_pte


    ; for krnl
    mov rdi, 0x1E0          ; PML4[0x1E0 * 0x8] -> krnl_pdpt_base
    shl rdi, 3              
    mov rbx, qword [rbp - 0x10]
    add rdi, rbx             ; rdi: pml4_base + (0x1E0 * 0x8) 

    mov rsi, qword [rbp - 0x60] ; rsi: krnl_pdpt_base

    mov rcx, qword [rbp - 0x50]              ; rcx: number of pml4e of krnl

    call func_fill_pte


    ; Set PDPT

    ; Set Ccldr PDPT

    ; calculate offset_of_ccldr from beginning of pdpt
    mov rbx, qword [r8 + 0x10]
    shr rbx, 30
    and rbx, 0x1FF
    shl rbx, 3

    mov rdi, qword [rbp - 0x58] ; get ccldr_pdpt_base
    add rdi, rbx    ; rdi: ccldr_pdpt_base[offset_of_ccldr from beginning of pdpt]

    mov rsi, qword [rbp - 0x68]     ; rsi: ccldr_pd_base
    mov rcx, qword [rbp - 0x28]     ; rdi: number of pdptes of ccldr
    call func_fill_pte

    ; Set krnl pdpt
    mov rdi, qword [rbp - 0x60]     ; rdi: krnl_pdpt_base
    mov rsi, qword [rbp - 0x70]     ; rsi: krnl_pd_base
    mov rcx, qword [rbp - 0x48]     ; rcx: number of pdptes of krnl
    call func_fill_pte  

    ; Set PD

    ; Set Ccldr PD

    ; calculate offset_of_ccldr from beginning of pd
    mov rbx, qword [r8 + 0x10]
    shr rbx, 21
    and rbx, 0x1FF
    shl rbx, 3

    mov rdi, qword [rbp - 0x68]; get ccldr_pd_base
    add rdi, rbx    ; rdi: ccldr_pd_base[offset_of_ccldr from beginning of pd]

    mov rsi, qword [rbp - 0x78] ; get ccldr_pt_base
    mov rcx, qword [rbp - 0x20] ; get number of pdes of ccldr
    call func_fill_pte

    ; Set krnl PD
    mov rdi, qword [rbp - 0x70] ; get krnl_pd_base
    mov rsi, qword [rbp - 0x80] ; get krnl_pt_base
    mov rcx, qword [rbp - 0x40] ; get number of pdes of krnl
    call func_fill_pte


    ; Set PT

    ; set ccldr pt

    ; calculate offset of ccldr from beginning of pt
    mov rbx, qword [r8 + 0x10]
    shr rbx, 12
    and rbx, 0x1ff
    shl rbx, 3

    mov rdi, qword [rbp - 0x78] ; get ccldr_pt_base
    add rdi, rbx ; rdi: ccldr_pt_base[offset of ccldr from beginning of pt]

    mov rsi, qword [r8 + 0x10]  ; get ccldr_base

    mov rcx, qword [rbp - 0x18] ; get number of ptes of ccldr
    call func_fill_pte

    ; set krnl pt

    mov rdi, qword [rbp - 0x80] ; get krnl_pt_base
    mov rsi, qword [r8]         ; get krnl_base
    mov rcx, qword [rbp - 0x38] ; get number of ptes of krnl
    call func_fill_pte


    mov rcx, cr3
    and rcx, 0xfff
    mov rax, qword[rbp - 0x10]
    or rax, rcx
    mov cr3, rax

    ; enable IA-32e long mode, no matter
    mov ecx, 0xc0000080
    rdmsr
    or eax, 0x0101
    wrmsr

    mov rdi, qword[rbp - 8]
    mov qword [r8 + 0x8], rdi
    mov rdi, r8
    
    mov rsi, kernel_space_virtual_base_addr + 0x1000
    call rsi
    
    hlt


; Routine Description: 
;   Fill Page table entry
; Parameters:
;   rdi 1st parameter: pte address
;   rsi 2st parameter: pt_base
;   rcx 3st parameter: number of entrys
; Returned Value:
;   returned 0 
func_fill_pte:

    fill_pte:
        mov rax, rsi 
        or rax, 3
        mov [rdi], rax
        add rsi, 0x1000
        add rdi, 0x8
        loop fill_pte
    xor rax, rax   
    ret

; Routine Description: 

;   The routine calculates the number of pdptes, the number of
; pdes and the number of ptes with specified byte size respectively.

; Parameters:
;   rdi 2st parameter: byte size
;   rsi 1st parameter: base addr
;   rdx 3st parameter: uint64_t addr[4]
;       addr[0]: number of pml4es
;       addr[1]: number of pdptes
;       addr[2]: number of pdes
;       addr[3]: number of ptes

; Returned Value:
;   None.

func_calculate_num_of_ptes:
    push rbx
    push r8
    push rbp
    mov rbp, rsp
    
    mov rcx, rsi    ; rcx backup base
    mov rbx, rdi    ; rbx backup size

    ; rax: remainder
    mov rax, rdi
    shr rdi, 39
    mov rsi, 0x7fffffffff
    and rax, rsi
    cmp rax, 0
    je num_of_pml4es_calculation_completed
    inc rdi
num_of_pml4es_calculation_completed:
    mov qword[rdx], rdi     ; store num_of_pml4es




    mov r8, rcx
    shr r8, 30
    and r8, 0x1ff

    mov rdi, rbx
    mov rax, rdi
    shr rdi, 30
    and rax, 0x3fffffff
    cmp rax, 0
    je num_of_pdptes_calculation_completed
    inc rdi

num_of_pdptes_calculation_completed:
    mov qword[rdx+0x8], rdi

    ; if ((offset + (*p_num_of_pdptes)) > (0x1FF * (*p_num_of_pml4es))) {
    ;     *p_num_of_pml4es += 1;
    ; }
    add r8, rdi
    mov r9, qword[rdx]
    shl r9, 9
    cmp r8, r9
    jna done_for_num_of_pml4e
    mov rdi, qword[rdx]
    inc rdi
    mov qword[rdx], rdi
done_for_num_of_pml4e:



    mov r8, rcx
    shr r8, 21
    and r8, 0x1ff

    mov rdi, rbx
    mov rax, rdi
    shr rdi, 21
    and rax, 0x1FFFFF
    cmp rax, 0
    je num_of_pdes_calculation_completed
    inc rdi

num_of_pdes_calculation_completed:
    mov qword[rdx+0x10], rdi

    add r8, rdi
    mov r9, qword[rdx + 0x8]
    shl r9, 9
    cmp r8, r9
    jna done_for_num_of_pdptes
    mov rdi, qword[rdx + 0x8]
    inc rdi
    mov qword[rdx + 0x8], rdi
done_for_num_of_pdptes:

    mov r8, rcx
    shr r8, 12
    and r8, 0x1ff

    mov rdi, rbx
    mov rax, rdi
    shr rdi, 12
    and rax, 0xFFF
    cmp rax, 0
    je num_of_ptes_calculation_completed
    inc rdi

num_of_ptes_calculation_completed:
    mov qword[rdx+0x18], rdi

    add r8, rdi
    mov r9, qword[rdx + 0x10]
    shl r9, 9
    cmp r8, r9
    jna done_for_num_of_pdes
    mov rdi, qword[rdx + 0x10]
    inc rdi
    mov qword[rdx + 0x10], rdi
done_for_num_of_pdes:


    mov rsp, rbp
    pop rbp
    pop r8
    pop rbx
    ret
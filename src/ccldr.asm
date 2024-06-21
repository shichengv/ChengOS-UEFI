[bits 64]

temporary_kernel_stack_top  equ 0x200000
kernel_space_virtual_base_addr equ 0xfffff00000000000

; temporary page table

pml4t_base equ temporary_kernel_stack_top

pdpt_ccldr_space_base equ pml4t_base + 0x1000
pdpt_krnl_space_base equ pdpt_ccldr_space_base + 0x1000

pd_ccldr_space_base equ pdpt_krnl_space_base + 0x1000
pt_ccldr_space_base equ pd_ccldr_space_base + 0x1000

; pt_krnl_space_base and pd_krnl_space_base are need to calculate, following pt_ccldr_space_base

section .text
    global _start
_start:

    ; Set temporary kernel stack pointer
    mov rsp, temporary_kernel_stack_top

    ; Zero general-purpose registers
    xor rax, rax

    xor rsi, rsi
    xor rdx, rdx
    xor rcx, rcx
    xor r8, r8
    xor r9, r9
    xor rbp, rbp

    ; r8 stores pointer that points to machine_info struct
    mov r8, rdi

    ; ccldr can map 2GBytes maximum physical memory;
    mov rdi, qword [r8 + 0x8]
    push rdi
    mov rsi, 0x80000000
    cmp rdi, rsi
    cmovnb rdi, rsi
    mov qword [r8 + 0x8], rdi


    ; Set PML4

    mov rdi, pml4t_base     ; PML4[0]
    mov rsi, pdpt_ccldr_space_base
    mov rcx, 1
    call func_fill_pte

    mov rdi, 0x1E0          ; PML4[0x1E0 * 0x8]
    shl rdi, 3              
    add rdi, pml4t_base
    mov rsi, pdpt_krnl_space_base
    mov rcx, 1
    call func_fill_pte


    ; Set PDPT

    ; Set Ccldr PDPT
    mov rdi, pdpt_ccldr_space_base  ; pdpt_ccldr[0]
    mov rsi, pd_ccldr_space_base
    mov rcx, 1
    call func_fill_pte

    ; Set PD

    ; Set Ccldr PD
    ; Calculate the number of PDEs that ccldr needed
    ; 0x200000 bytes eq 2 MBytes
    mov rcx, qword [r8 + 0x18]  ; Get the size of the ccldr
    shr rcx, 21 ; ccldr_size / 2 MBytes
    add rcx, 1  ; the number of pds
    mov rax, rcx
    shl rax, 12 ; number of pds * 0x1000
    add rax, pt_ccldr_space_base
    push rax    ; backup pd_krnl_space_base

    mov rdi, pd_ccldr_space_base
    mov rsi, pt_ccldr_space_base
    call func_fill_pte

    ; Set Kernel Space PDPT
    ; Calculate the number of PDPTEs
    ; 0x40000000 bytes eq 1 GByte
    mov rcx, qword [r8 + 0x8] ; Get the size of the kernel space 
    shr rcx, 30               ; kernel space size / 1 GByte
    add rcx, 1                ; rcx stores number of PDPTEs

    mov rbx, rcx
    shl rbx, 12
    pop rsi                 ; rsi stores pd_krnl_space_base
    add rbx, rsi            ; backup pt_krnl_space_base
    push rbx               
    push rsi                ; backup pd_krnl_space_base

    mov rdi, pdpt_krnl_space_base
    call func_fill_pte


    ; Set Kernel Space PD
    ; Calculate the number of PDEs that kernel space needed
    mov rcx, qword [r8 + 0x8]   ; Get the size of the kernel space 
    shr rcx, 21
    add rcx, 1              ; rcx stores the number of PDEs

    pop rdi                 ; restore pd_krnl_space_base
    pop rsi                 ; restore pt_krnl_space_base
    push rsi                ; backup pt_krnl_space_base
    call func_fill_pte


    ; Set PT

    ; Set machine info pt
    mov rcx, 0xf    ; machine_info_size(0xF000) / 0x1000 = 0xF
    mov rsi, r8     ; get machine info address

    mov rdi, rsi
    shr rdi, 9
    add rdi, pt_ccldr_space_base
    call func_fill_pte

    ; Set Ccldr PT
    ; Calculate the number of PTEs that ccldr needed
    mov rcx, qword [r8 + 0x18]  
    shr rcx, 12
    mov rsi, qword [r8 + 0x10]

    mov rdi, rsi
    shr rdi, 9              ; ccldr_base_addr / page_size * sizeof(page table entry)
    add rdi, pt_ccldr_space_base   
    call func_fill_pte

    ; Set Kernel Space PT
    ; Calculate the number of PTEs that kernel space needed
    mov rcx, qword [r8 + 0x8]
    shr rcx, 12
    pop rdi             ; restore pt_krnl_space_base
    mov rsi, qword [r8]
    call func_fill_pte

    mov rcx, cr3
    and rcx, 0xfff
    mov rax, pml4t_base
    or rax, rcx
    mov cr3, rax

    ; enable IA-32e long mode, no matter
    mov ecx, 0xc0000080
    rdmsr
    or eax, 0x0101
    wrmsr

    pop rdi
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
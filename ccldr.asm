; Cheng Cheng Loader


; The ccldr will be loaded physical address 0x8000

; ┌────────────────────┐<- load address 0x8000
; │       0xE000       │                    
; ├────────────────────┤<- 0x10000          
; │     TEMPORARY      │                    
; │     PTE AREA       │ 
; ├────────────────────┤<- 0xa0000
; │     UNABLE MEM     │
; ├────────────────────┤<- 0x100000          
; │                    │                    
; │                    │                    
; │  TEMPORARY STACK   │
; │                    │                    
; │                    │                    
; ├────────────────────┤<- RSP ccldr_start 0x300000      
; │                    │                    
; │    CCLDR_CODE      │ 
; │                    │                    
; ├────────────────────┤<- ccldr_end 0x400000     
; │                    │                    
; │                    │                    
; │                    │                    
; │                    │                    
; │      0x600000      │                    
; │                    │                    
; │                    │                    
; │                    │                    
; │                    │                    
; │                    │                    
; │                    │                    
; └────────────────────┘<- 0x800000         


[bits 64]
; constants
ccldr_loaded_addr equ 0x8000
krnl_vip_data_size equ 0x8000

temporary_pte_area equ 0x10000
temporary_pte_area_size equ 0x90000 - ccldr_loaded_addr

temporary_stack_buttom equ 0x90000

temporary_stack_top equ 0x300000 - ccldr_loaded_addr

ccldr_code_start equ temporary_stack_top
ccldr_code_end equ 0x400000 - ccldr_loaded_addr


krnl_start equ 0x80000011000
gdt_descriptor_addr equ 0x80000002000

CurrentMachineInfoAddress equ 0x80000000000 + 0x1000

; tmp address area 0x10000 ~ 0x800000

; PML4 addr
pml4_entry_addr equ 0x10000
; PDPTE addr
pdpte_tmp_space_addr equ 0x11000
pdpte_krnl_space_addr equ 0x12000
; PDE addr
pde_tmp_space_addr equ 0x13000
pde_krnl_space_addr equ 0x14000

; PTE addr, the lower 8-MByte stored ccldr data and tmp pte. 
; Hence, it needs (8-MByte / 2-MByte) + (sizeof(krnl_image) + 0x10000-Byte) / 2-MByte PTEs
ptes_tmp_base_addr equ 0x15000
ptes_krnl_base_addr equ 0x19000


; ccldr data
gdt_descriptor:
    dw 0xffff
    dq 0x8000000200a

gdt_addr:
    dq 0
gdt_krnl_code:
    dd 0x0000ffff
    dw 0
    db 0 
    db 0x9a
    dw 0x00Af

gdt_krnl_data:
    dd 0x0000ffff
    dw 0
    db 0 
    db 0x92
    dw 0x00CF

gdt_user_code:
    dd 0x0000FFFF
    dw 0
    db 0 
    db 0xFA
    dw 0x00AF

gdt_user_data:
    dd 0x0000ffff
    dw 0
    db 0 
    db 0xF2
    dw 0x00CF

gdt_tss_seg:
;     dq 0, 0

times krnl_vip_data_size - ($ - $$) db 0

; temporary pte area

times temporary_pte_area_size - ($ - $$) db 0

; temporary stack

times temporary_stack_top - ($ - $$) db 0


; ccldr code
section .text
    global _start
_start:
    ; Set temporary stack pointer
    mov rsp, temporary_stack_top
    push rdi
    ; Calculate the number of PDEs 
    xor rax, rax
    xor rdx, rdx
    mov rax, qword [rdi + 8] ; Get the size of the Krnl Image 
    mov rsi, 0x200000           ; A PDE entry covers 2-MByte address space
    div rsi                     ; div 0x200000

    lea rcx, 1[rax]             ; the number of PDE entrys krnl_image needs

    ; Set PML4
    mov rax, pdpte_tmp_space_addr
    or rax, 0x3
    lea rsi, pml4_entry_addr
    mov [rsi], rax  ; PML4[0]
    mov rax, pdpte_krnl_space_addr
    or rax, 0x3
    add rsi, 0x80
    mov [rsi], rax  ; PML4[16]

    ; Set PDPTE
    mov rax, pde_tmp_space_addr
    or rax, 0x3
    mov rsi, pdpte_tmp_space_addr
    mov [rsi], rax  ; tmp_space PDPTE[0]
    mov rax, pde_krnl_space_addr
    or rax, 0x3
    mov rsi, pdpte_krnl_space_addr
    mov [rsi], rax  ; krnl_space PDPTE[0]
    
    ; Set PDE
    mov rax, ptes_tmp_base_addr
    or rax, 0x3
    mov rsi, pde_tmp_space_addr
    mov [rsi], rax  ; tmp_space PDE[0]
    
    mov rax, ptes_tmp_base_addr+0x1000
    or rax, 0x3
    add rsi, 0x8
    mov [rsi], rax ; tmp_space PDE[1]

    mov rax, ptes_tmp_base_addr+0x2000
    or rax, 0x3
    add rsi, 0x8
    mov [rsi], rax ; tmp_space PDE[2]

    mov rax, ptes_tmp_base_addr+0x3000
    or rax, 0x3
    add rsi, 0x8 
    mov [rsi], rax ; tmp_space PDE[3] 

 
    ; maping krnl_image pdes
    ; rcx, the number of PDE entrys krnl_image needs
    ; rsi, pde_entry counter
    ; rdx, pte address
    ; rdi, pde address
    ; rax, entry
    mov rsi, 0
    xor rdi, rdi
    mov rdi, pde_krnl_space_addr ; rdi <- krnl_image_PDE_address

fill_pde_krnl:
    mov rdx, rsi    ; rdx = rdi = rsi
    sal rdx, 12     ; rdx * 0x1000

    lea rdx, ptes_krnl_base_addr[rdx]   ; krnk_image_PTE address

    mov rax, rdx
    or rax, 0x3
    mov qword [rdi], rax    ; write to PDE entry

    inc rsi         ; counter++
    add rdi, 8      ; krnl_image_PDE_entry offset 
    loop fill_pde_krnl

    ; Set PDE[0] 0x10 ~ 0x1FF
    ; mapping 0x10000 ~ 0x200000 to physical address 0x10000 ~ 0x200000

    ; rcx, counter of page table entry 
    ; rdx, maximum entrys 0x200
    ; rsi, address of page table entry
    ; r8, page address
    ; r9, the number of PDE entrys

    mov rcx, 0x10
    mov rdx, 0x200
    mov rsi, ptes_tmp_base_addr + 0x80  ; start address of tmp_space PTE entry
    mov r8, 0x10000 ; PAGE address
filling_pte0:
    mov rax, r8
    or rax, 0x3
    mov [rsi], rax  ; write to PDE entry

    add r8, 0x1000
    add rsi, 0x8

    inc rcx         ; counter++
    cmp rcx, rdx    
    jnae filling_pte0

    ; Mapping 0x200000 ~ 0x800000 to physical address 0x200000 ~ 0x800000
    xor r9, r9

    ; Double layer loop nesting
filling_ptes:
    inc r9
    xor rcx, rcx

    mov rsi, r9
    sal rsi, 12
    lea rsi, ptes_tmp_base_addr[rsi]

filling_pte:
    mov rax, r8
    or rax, 0x3
    mov [rsi], rax

    add r8, 0x1000
    add rsi, 0x8

    inc rcx
    cmp rcx, rdx
    jnae filling_pte

    cmp r9, 3
    jnae filling_ptes


    ; Mapping 0x0~0x10000 to 0x800000000000 ~ 0x800000010000

    mov rsi, ptes_krnl_base_addr
    xor rdx, rdx
    xor rcx, rcx

mapping_krnl_gdt_data:
    mov rdx, rcx

    sal rdx, 0xc
    or rdx, 3
    mov [rsi], rdx

    ; update
    inc rcx
    add rsi, 8

    ; compare and jump
    cmp rcx, 0x10
    jb mapping_krnl_gdt_data

    mov rdi, rsi    ; backup Krnl_space_PDE_entry offset



    ; Mapping krnl_image to physcial address ( 0x0800 0001 0000 ~ 0x0800 0001 0000 + krnl_image_size )

    pop rsi ; Get Current_Machine_Info

    mov r8, qword[rsi]      ; krnl_start_addr  

    mov r9, qword[rsi+8]    ; krnl_image_size
    push r9                 ; save krnl_image_szie

    ; Calculate the number of pages required for krnl_image and store in RDX
    add r9, 0xfff
    and r9, 0xfffffffffffff000
    shr r9, 12
    mov rdx, r9                   

    pop r9                  ; restore 
    add r9, r8              ; krnl_image_end, Calculate only, do not use

    mov rsi, rdi    ; Recovery the offset
    and r8, 0xfffffffffffff000  ; do nothing 
    xor rcx, rcx    ; counter = 0

mapping_krnl_image:
    mov rax, r8
    or rax, 3
    mov [rsi], rax  ; write to PDE entry
    
    ; update
    add rsi, 8
    add r8, 0x1000
    inc rcx

    ; compare and jump
    cmp rcx, rdx
    jb mapping_krnl_image



    ; Set CR3
    mov rcx, cr3
    and rcx, 0xfff
    mov rax, pml4_entry_addr
    or rax, rcx
    mov cr3, rax

    ; enable IA-32e long mode, no matter
    mov ecx, 0xc0000080
    rdmsr
    or eax, 0x0101
    wrmsr


    ; Set Global Descriptor Table
    mov rcx, gdt_descriptor_addr
    lgdt [rcx] 

    ; pass manchine_info to krnl_start routine
    mov rdi, CurrentMachineInfoAddress

    ; transfer control to krnl_start(current_machine_info)
    mov rdx, krnl_start
    call rdx

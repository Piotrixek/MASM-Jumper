.CODE
_Clear_board_asm PROC
    push rdi
    mov rdi, rcx
    mov rcx, rdx
    mov rax, 2020202020202020h
    rep stosq
    pop rdi
    ret
_Clear_board_asm ENDP

_Reset_game_asm PROC
    mov dword ptr [rcx + 0], edx    ; _Player_x
    mov dword ptr [rcx + 4], 35     ; _Player_y
    mov dword ptr [rcx + 8], 0      ; _Velocity_y
    mov dword ptr [rcx + 12], 35    ; _Max_y
    mov dword ptr [rcx + 16], 0     ; _Score
    mov dword ptr [rcx + 20], 1     ; _Is_alive
    ret
_Reset_game_asm ENDP

_Initialize_saucers_asm PROC
    push rbx
    push r12

    mov r9d, 35                     ; _Current_y
    xor r10d, r10d                  ; _I = 0
    mov r12d, dword ptr [rcx + 24]  ; _Width
    sub r12d, 8                     ; _Width - 8

_init_loop:
    cmp r10d, 16
    jge _init_done

    ; --- Hardware Xorshift RNG ---
    mov eax, dword ptr [rcx + 160]  ;  _Rng_state
    mov r8d, eax
    shl r8d, 13
    xor eax, r8d
    mov r8d, eax
    shr r8d, 17
    xor eax, r8d
    mov r8d, eax
    shl r8d, 5
    xor eax, r8d
    mov dword ptr [rcx + 160], eax

    ; Modulo: rand % (_Width - 8)
    xor edx, edx
    div r12d                        
    add edx, 4

    mov dword ptr [rcx + 32 + r10*4], edx ; _Saucers_x[_I]
    mov dword ptr [rcx + 96 + r10*4], r9d ; _Saucers_y[_I]

    sub r9d, 4                      ; _Current_y -= 4
    inc r10d
    jmp _init_loop

_init_done:
    pop r12
    pop rbx
    ret
_Initialize_saucers_asm ENDP

_Recycle_saucers_asm PROC
    push rbx
    push r12

    mov r9d, 2147483647             ; _Highest_y
    xor r10d, r10d                  ; _I = 0

_find_highest_loop:
    cmp r10d, 16
    jge _find_highest_done
    mov eax, dword ptr [rcx + 96 + r10*4]
    cmp eax, r9d
    jge _skip_highest
    mov r9d, eax                    ; new _Highest_y
_skip_highest:
    inc r10d
    jmp _find_highest_loop

_find_highest_done:
    mov r11d, dword ptr [rcx + 12]  ; _Max_y
    sub r11d, 20                    ; _Camera_y
    xor r10d, r10d                  ; _I = 0
    mov r12d, dword ptr [rcx + 24]  ; _Width
    sub r12d, 8                     ; _Width - 8

_recycle_loop:
    cmp r10d, 16
    jge _recycle_done

    mov eax, dword ptr [rcx + 96 + r10*4]
    mov r8d, eax
    sub r8d, r11d                   ; _Saucers_y[_I] - _Camera_y
    cmp r8d, dword ptr [rcx + 28]   ; check vs _Height
    jle _skip_recycle

    ; --- Generatingg Y offset (rand % 5) + 3 ---
    mov eax, dword ptr [rcx + 160]
    mov r8d, eax
    shl r8d, 13
    xor eax, r8d
    mov r8d, eax
    shr r8d, 17
    xor eax, r8d
    mov r8d, eax
    shl r8d, 5
    xor eax, r8d
    mov dword ptr [rcx + 160], eax

    xor edx, edx
    mov r8d, 5
    div r8d
    add edx, 3
    sub r9d, edx                    ; _Highest_y -= offset
    mov dword ptr [rcx + 96 + r10*4], r9d ;  new _Saucers_y[_I]

    ; --- Generating X offset (rand % (_Width - 8)) + 4 ---
    mov eax, dword ptr [rcx + 160]
    mov r8d, eax
    shl r8d, 13
    xor eax, r8d
    mov r8d, eax
    shr r8d, 17
    xor eax, r8d
    mov r8d, eax
    shl r8d, 5
    xor eax, r8d
    mov dword ptr [rcx + 160], eax

    xor edx, edx
    div r12d
    add edx, 4
    mov dword ptr [rcx + 32 + r10*4], edx ;  new _Saucers_x[_I]

_skip_recycle:
    inc r10d
    jmp _recycle_loop

_recycle_done:
    pop r12
    pop rbx
    ret
_Recycle_saucers_asm ENDP

_Update_jumper_physics_asm PROC
    cmp edx, 97
    je _move_left
    cmp edx, 65
    je _move_left
    cmp edx, 100
    je _move_right
    cmp edx, 68
    je _move_right
    jmp _apply_gravity

_move_left:
    mov eax, dword ptr [rcx + 0]
    dec eax
    cmp eax, 0
    jl _apply_gravity
    mov dword ptr [rcx + 0], eax
    jmp _apply_gravity

_move_right:
    mov eax, dword ptr [rcx + 0]
    inc eax
    mov r8d, dword ptr [rcx + 24]
    dec r8d
    cmp eax, r8d
    jge _apply_gravity
    mov dword ptr [rcx + 0], eax

_apply_gravity:
    mov eax, dword ptr [rcx + 8]
    inc eax
    cmp eax, 1
    jle _update_y
    mov eax, 1
_update_y:
    mov dword ptr [rcx + 8], eax
    mov r8d, dword ptr [rcx + 4]
    add r8d, eax
    mov dword ptr [rcx + 4], r8d
    cmp eax, 0
    jle _check_camera

    mov r9d, 0
_collision_loop:
    cmp r9d, 16
    jge _check_camera
    mov r10d, dword ptr [rcx + 96 + r9*4]
    cmp r8d, r10d
    jne _next_saucer
    mov r11d, dword ptr [rcx + 32 + r9*4]
    mov eax, dword ptr [rcx + 0]
    sub r11d, 4
    cmp eax, r11d
    jl _next_saucer
    add r11d, 8
    cmp eax, r11d
    jg _next_saucer
    mov dword ptr [rcx + 8], -8
    jmp _check_camera
_next_saucer:
    inc r9d
    jmp _collision_loop

_check_camera:
    mov r8d, dword ptr [rcx + 4]
    mov eax, dword ptr [rcx + 12]
    cmp r8d, eax
    jge _check_death
    mov dword ptr [rcx + 12], r8d
    mov r10d, r8d
    neg r10d
    add r10d, 35
    cmp r10d, dword ptr [rcx + 16]
    jle _check_death
    mov dword ptr [rcx + 16], r10d

_check_death:
    mov r11d, eax
    add r11d, 20
    cmp r8d, r11d
    jl _done
    cmp eax, 20
    jl _die
    mov dword ptr [rcx + 8], -8
    jmp _done

_die:
    mov dword ptr [rcx + 20], 0
_done:
    ret
_Update_jumper_physics_asm ENDP
END
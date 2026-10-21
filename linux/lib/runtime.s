.intel_syntax noprefix
.section .data
    .align 4
    bytes_rw:      .long 0
    current_brk:   .long 0
    newline_str:   .asciz "\n"
    null_str:      .asciz "(null)"

    rand_seed:     .long 1

.section .bss
    .lcomm one_char_buf, 4
    .lcomm int_conv_buf, 32
    .lcomm printf_buf, 2048
    .lcomm printf_buf_len, 4

.section .text
# I/O y Consola
.globl _fundamental_printf
.globl fundamental_printf
.globl _printf
.globl _fundamental_puts
.globl fundamental_puts
.globl _puts
.globl _fundamental_putchar
.globl fundamental_putchar
.globl _putchar
.globl _fundamental_getchar
.globl fundamental_getchar
.globl _getchar
.globl _fundamental_print_str
.globl fundamental_print_str
.globl _fundamental_println_str
.globl fundamental_println_str
.globl _fundamental_println_empty
.globl fundamental_println_empty
.globl _fundamental_print_int
.globl fundamental_print_int
.globl _fundamental_println_int
.globl fundamental_println_int
.globl _fundamental_read_str
.globl fundamental_read_str
.globl _fundamental_read_int
.globl fundamental_read_int

# Memoria dinamica y buffers
.globl _fundamental_malloc
.globl fundamental_malloc
.globl _malloc
.globl _fundamental_free
.globl fundamental_free
.globl _free
.globl _fundamental_calloc
.globl fundamental_calloc
.globl _calloc
.globl _fundamental_realloc
.globl fundamental_realloc
.globl _realloc
.globl _fundamental_memset
.globl fundamental_memset
.globl _memset
.globl _fundamental_memcpy
.globl fundamental_memcpy
.globl _memcpy
.globl _fundamental_memmove
.globl fundamental_memmove
.globl _memmove
.globl _fundamental_memcmp
.globl fundamental_memcmp
.globl _memcmp

# Manipulacion de cadenas
.globl _fundamental_strlen
.globl fundamental_strlen
.globl _strlen
.globl _fundamental_strcpy
.globl fundamental_strcpy
.globl _strcpy
.globl _fundamental_strncpy
.globl fundamental_strncpy
.globl _strncpy
.globl _fundamental_strcat
.globl fundamental_strcat
.globl _strcat
.globl _fundamental_strncat
.globl fundamental_strncat
.globl _strncat
.globl _fundamental_strcmp
.globl fundamental_strcmp
.globl _strcmp
.globl _fundamental_strncmp
.globl fundamental_strncmp
.globl _strncmp
.globl _fundamental_strchr
.globl fundamental_strchr
.globl _strchr
.globl _fundamental_strrchr
.globl fundamental_strrchr
.globl _strrchr
.globl _fundamental_strstr
.globl fundamental_strstr
.globl _strstr
.globl _fundamental_strdup
.globl fundamental_strdup
.globl _strdup
.globl _fundamental_char_at
.globl fundamental_char_at
.globl _char_at
.globl _fundamental_set_char_at
.globl fundamental_set_char_at
.globl _set_char_at

# Clasificacion y conversion de caracteres (ctype)
.globl _fundamental_is_digit
.globl fundamental_is_digit
.globl _is_digit
.globl _fundamental_is_alpha
.globl fundamental_is_alpha
.globl _is_alpha
.globl _fundamental_is_alnum
.globl fundamental_is_alnum
.globl _is_alnum
.globl _fundamental_is_space
.globl fundamental_is_space
.globl _is_space
.globl _fundamental_is_upper
.globl fundamental_is_upper
.globl _is_upper
.globl _fundamental_is_lower
.globl fundamental_is_lower
.globl _is_lower
.globl _fundamental_is_xdigit
.globl fundamental_is_xdigit
.globl _is_xdigit
.globl _fundamental_to_upper
.globl fundamental_to_upper
.globl _to_upper
.globl _fundamental_to_lower
.globl fundamental_to_lower
.globl _to_lower

# Conversiones numericas
.globl _fundamental_atoi
.globl fundamental_atoi
.globl _atoi
.globl _fundamental_itoa
.globl fundamental_itoa
.globl _itoa
.globl _fundamental_htoi
.globl fundamental_htoi
.globl _htoi

# Matematicas y numeros aleatorios
.globl _fundamental_abs
.globl fundamental_abs
.globl _abs
.globl _fundamental_min
.globl fundamental_min
.globl _min
.globl _fundamental_max
.globl fundamental_max
.globl _max
.globl _fundamental_clamp
.globl fundamental_clamp
.globl _clamp
.globl _fundamental_pow
.globl fundamental_pow
.globl _pow
.globl _fundamental_sqrt
.globl fundamental_sqrt
.globl _sqrt
.globl _fundamental_gcd
.globl fundamental_gcd
.globl _gcd
.globl _fundamental_lcm
.globl fundamental_lcm
.globl _lcm
.globl _fundamental_rand
.globl fundamental_rand
.globl _rand
.globl _fundamental_srand
.globl fundamental_srand
.globl _srand

# Sistema, proceso y tiempo
.globl _fundamental_exit
.globl fundamental_exit
.globl _exit
.globl _fundamental_sleep
.globl fundamental_sleep
.globl _sleep
.globl _fundamental_get_tick_count
.globl fundamental_get_tick_count
.globl _get_tick_count
.globl _fundamental_time
.globl fundamental_time
.globl _time

# Operaciones con archivos nativas de Win32 (kernel32)
.globl _fundamental_file_exists
.globl fundamental_file_exists
.globl _file_exists
.globl _fundamental_file_size
.globl fundamental_file_size
.globl _file_size
.globl _fundamental_file_read_all
.globl fundamental_file_read_all
.globl _file_read_all
.globl _fundamental_file_write_all
.globl fundamental_file_write_all
.globl _file_write_all
.globl _fundamental_file_append
.globl fundamental_file_append
.globl _file_append
.globl _fundamental_file_remove
.globl fundamental_file_remove
.globl _file_remove

# Helper: Imprimir buffer con longitud en EDX (Linux sys_write syscall 4)
# EAX = buffer, EDX = longitud
.globl write_raw
.globl _write_raw
write_raw:
_write_raw:
    test edx, edx
    jle .write_raw_done

    push ebx
    push esi
    push edi

    mov esi, eax        # esi = buffer
    mov edi, edx        # edi = longitud

    mov eax, 4          # sys_write = 4
    mov ebx, 1          # stdout fd = 1
    mov ecx, esi        # ecx = buffer
    mov edx, edi        # edx = longitud
    int 0x80

    pop edi
    pop esi
    pop ebx
.write_raw_done:
    ret

# Helper: Lee 1 byte de STDIN en AL (Linux sys_read syscall 3)
.globl read_byte
.globl _read_byte
read_byte:
_read_byte:
    push ebx
    push ecx
    push edx

    mov eax, 3          # sys_read = 3
    mov ebx, 0          # stdin fd = 0
    mov ecx, offset one_char_buf
    mov edx, 1
    int 0x80

    test eax, eax
    jle .rb_eof

    mov al, [one_char_buf]
    clc
    pop edx
    pop ecx
    pop ebx
    ret

.rb_eof:
    xor al, al
    stc
    pop edx
    pop ecx
    pop ebx
    ret

# _fundamental_print_str(char* str)
_fundamental_print_str:
fundamental_print_str:
    push ebp
    mov ebp, esp
    push esi

    mov esi, [ebp + 8]   # puntero a str
    test esi, esi
    jz .print_str_done

    # Calcular longitud de la cadena terminada en 0
    xor edx, edx
.len_loop:
    cmp byte ptr [esi + edx], 0
    je .len_done
    inc edx
    jmp .len_loop
.len_done:
    test edx, edx
    jz .print_str_done

    mov eax, esi
    call _write_raw

.print_str_done:
    pop esi
    pop ebp
    ret

# _fundamental_println_empty()
_fundamental_println_empty:
fundamental_println_empty:
    push ebp
    mov ebp, esp
    mov eax, offset newline_str
    mov edx, 2
    call _write_raw
    pop ebp
    ret

# _fundamental_println_str(char* str)
_fundamental_println_str:
fundamental_println_str:
    push ebp
    mov ebp, esp
    push dword ptr [ebp + 8]
    call _fundamental_print_str
    add esp, 4
    call _fundamental_println_empty
    pop ebp
    ret

# _fundamental_puts(char* str)
# Imprime una cadena seguida de fin de linea y retorna 0 en EAX
_fundamental_puts:
fundamental_puts:
_puts:
    push ebp
    mov ebp, esp
    push dword ptr [ebp + 8]
    call _fundamental_println_str
    add esp, 4
    xor eax, eax
    pop ebp
    ret

# Helper: Convertir entero en EAX a string en int_conv_buf
# Retorna EAX = puntero al inicio del string, EDX = longitud
_int_to_str:
    push ebx
    push esi
    push edi

    mov edi, offset int_conv_buf + 30
    mov byte ptr [edi + 1], 0   # Terminado en 0
    xor ecx, ecx                # Contador de digitos
    mov ebx, 10

    # Verificar signo
    xor esi, esi                # esi = 1 si es negativo
    test eax, eax
    jns .positive
    neg eax
    mov esi, 1

.positive:
.div_loop:
    xor edx, edx
    div ebx                     # eax = cociente, edx = residuo
    add dl, '0'
    mov [edi], dl
    inc ecx
    dec edi
    test eax, eax
    jnz .div_loop

    # Si era negativo, anteponer '-'
    test esi, esi
    jz .sign_done
    mov byte ptr [edi], '-'
    inc ecx
    dec edi

.sign_done:
    inc edi                     # edi apunta al primer caracter
    mov eax, edi
    mov edx, ecx                # longitud en edx

    pop edi
    pop esi
    pop ebx
    ret

# Helper: Convertir puntero en EAX a string hexadecimal en int_conv_buf (ej. "0x00401000")
# Retorna EAX = offset int_conv_buf, EDX = 10
_ptr_to_hex:
    push ebx
    push ecx
    push edi

    mov edi, offset int_conv_buf
    mov byte ptr [edi], '0'
    mov byte ptr [edi + 1], 'x'

    mov ebx, eax        # ebx = direccion
    mov ecx, 8          # 8 digitos hex
.hex_loop:
    rol ebx, 4
    mov al, bl
    and al, 0x0F
    cmp al, 10
    jl .hex_digit
    add al, 'a' - 10
    jmp .hex_store
.hex_digit:
    add al, '0'
.hex_store:
    mov edx, 10
    sub edx, ecx
    mov [edi + edx], al
    loop .hex_loop

    mov byte ptr [edi + 10], 0
    mov eax, edi
    mov edx, 10

    pop edi
    pop ecx
    pop ebx
    ret

# Helper: Convertir unsigned int en EAX a string decimal en int_conv_buf
# Retorna EAX = puntero al inicio del string, EDX = longitud
_uint_to_str:
    push ebx
    push esi
    push edi

    mov edi, offset int_conv_buf + 30
    mov byte ptr [edi + 1], 0
    xor ecx, ecx
    mov ebx, 10

.udiv_loop:
    xor edx, edx
    div ebx
    add dl, '0'
    mov [edi], dl
    inc ecx
    dec edi
    test eax, eax
    jnz .udiv_loop

    inc edi
    mov eax, edi
    mov edx, ecx

    pop edi
    pop esi
    pop ebx
    ret

# Helper: Convertir entero en EAX a string hexadecimal en int_conv_buf
# Retorna EAX = puntero al inicio del string, EDX = longitud
_int_to_hex:
    push ebx
    push esi
    push edi

    mov edi, offset int_conv_buf + 30
    mov byte ptr [edi + 1], 0
    xor ecx, ecx
    mov ebx, 16

.hex_div_loop:
    xor edx, edx
    div ebx
    cmp dl, 10
    jl .hex_num_digit
    add dl, 'a' - 10
    jmp .hex_num_store
.hex_num_digit:
    add dl, '0'
.hex_num_store:
    mov [edi], dl
    inc ecx
    dec edi
    test eax, eax
    jnz .hex_div_loop

    inc edi
    mov eax, edi
    mov edx, ecx

    pop edi
    pop esi
    pop ebx
    ret

# _fundamental_print_int(int val)
_fundamental_print_int:
fundamental_print_int:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 8]
    call _int_to_str
    call _write_raw

    pop ebp
    ret

# _fundamental_println_int(int val)
_fundamental_println_int:
fundamental_println_int:
    push ebp
    mov ebp, esp

    push dword ptr [ebp + 8]
    call _fundamental_print_int
    add esp, 4
    call _fundamental_println_empty

    pop ebp
    ret

# _fundamental_printf(char* format, ...)
# Implementacion 100% nativa en ensamblador x86 independiente de libc
_fundamental_printf:
fundamental_printf:
_printf:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov dword ptr [printf_buf_len], 0
    mov esi, [ebp + 8]          # format string
    test esi, esi
    jz .printf_done

    lea ebx, [ebp + 12]         # ebx = puntero al siguiente argumento en la pila

.printf_loop:
    mov al, [esi]
    test al, al
    jz .printf_done

    cmp al, '%'
    jne .print_literal_chunk

    # Encontramos '%'
    inc esi
    mov al, [esi]
    test al, al
    jz .printf_done

    cmp al, 'd'
    je .fmt_int
    cmp al, 'i'
    je .fmt_int
    cmp al, 'u'
    je .fmt_uint
    cmp al, 'x'
    je .fmt_hex
    cmp al, 'X'
    je .fmt_hex
    cmp al, 's'
    je .fmt_str
    cmp al, 'p'
    je .fmt_ptr
    cmp al, 'c'
    je .fmt_char
    cmp al, '%'
    je .fmt_percent

    # Otro caracter: imprimir '%' y el caracter
    mov byte ptr [one_char_buf], '%'
    mov eax, offset one_char_buf
    mov edx, 1
    call _printf_append
    mov al, [esi]
    mov [one_char_buf], al
    mov eax, offset one_char_buf
    mov edx, 1
    call _printf_append
    inc esi
    jmp .printf_loop

.fmt_int:
    mov eax, [ebx]
    add ebx, 4
    call _int_to_str
    call _printf_append
    inc esi
    jmp .printf_loop

.fmt_uint:
    mov eax, [ebx]
    add ebx, 4
    call _uint_to_str
    call _printf_append
    inc esi
    jmp .printf_loop

.fmt_hex:
    mov eax, [ebx]
    add ebx, 4
    call _int_to_hex
    call _printf_append
    inc esi
    jmp .printf_loop

.fmt_str:
    mov eax, [ebx]
    add ebx, 4
    test eax, eax
    jz .fmt_null_str
    push eax
    mov edi, eax
    xor edx, edx
.fmt_str_len:
    cmp byte ptr [edi + edx], 0
    je .fmt_str_got_len
    inc edx
    jmp .fmt_str_len
.fmt_str_got_len:
    pop eax
    call _printf_append
    jmp .fmt_str_end
.fmt_null_str:
    mov eax, offset null_str
    mov edx, 6
    call _printf_append
.fmt_str_end:
    inc esi
    jmp .printf_loop

.fmt_ptr:
    mov eax, [ebx]
    add ebx, 4
    call _ptr_to_hex
    call _printf_append
    inc esi
    jmp .printf_loop

.fmt_char:
    mov eax, [ebx]
    add ebx, 4
    mov [one_char_buf], al
    mov eax, offset one_char_buf
    mov edx, 1
    call _printf_append
    inc esi
    jmp .printf_loop

.fmt_percent:
    mov byte ptr [one_char_buf], '%'
    mov eax, offset one_char_buf
    mov edx, 1
    call _printf_append
    inc esi
    jmp .printf_loop

.print_literal_chunk:
    mov edi, esi
.find_chunk_end:
    mov al, [edi]
    test al, al
    jz .found_chunk
    cmp al, '%'
    je .found_chunk
    inc edi
    jmp .find_chunk_end

.found_chunk:
    mov edx, edi
    sub edx, esi
    mov eax, esi
    call _printf_append
    mov esi, edi
    jmp .printf_loop

.printf_done:
    call _printf_flush
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

# Helper: Añade EDX bytes desde EAX al buffer de printf y vacía si excede 1900 bytes
_printf_append:
    test edx, edx
    jle .pa_done

    push ebx
    push esi
    push edi

    mov esi, eax                    # fuente
    mov edi, offset printf_buf
    add edi, [printf_buf_len]       # destino

    # Si se llena más de 1900 bytes, vaciar
    mov ecx, [printf_buf_len]
    add ecx, edx
    cmp ecx, 1900
    jl .pa_copy

    call _printf_flush
    mov edi, offset printf_buf

.pa_copy:
    mov ecx, edx
    add [printf_buf_len], edx
    cld
    rep movsb

    pop edi
    pop esi
    pop ebx
.pa_done:
    ret

# Helper: Vacía el buffer de printf a STDOUT
_printf_flush:
    push ebx
    push esi
    push edi
    push ebp

    mov edx, [printf_buf_len]
    test edx, edx
    jle .pf_done

    mov dword ptr [printf_buf_len], 0
    mov eax, offset printf_buf
    call _write_raw

.pf_done:
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret

# _fundamental_read_str(char* dest)
# Usa EDI = buffer dest, ESI = contador/indice
_fundamental_read_str:
fundamental_read_str:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov edi, [ebp + 8]          # dest buffer
    test edi, edi
    jz .read_str_done

    xor esi, esi                # indice en dest

.read_str_loop:
    cmp esi, 255                # Limite maximo 255 caracteres
    jge .read_str_finish

    call _read_byte
    jc .read_str_finish         # EOF alcanzado

    cmp al, 13                  # '\r'
    je .read_str_loop           # ignorar retorno de carro en Windows
    cmp al, 10                  # '\n'
    je .read_str_finish         # fin de linea

    mov [edi + esi], al
    inc esi
    jmp .read_str_loop

.read_str_finish:
    mov byte ptr [edi + esi], 0 # Null-terminate

.read_str_done:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

# _fundamental_read_int(int* dest)
# Usa EDI = dest pointer, ESI = acumulador numerico, EBX = signo (1 si neg)
_fundamental_read_int:
fundamental_read_int:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov edi, [ebp + 8]          # int* dest
    test edi, edi
    jz .read_int_done

    xor esi, esi                # acumulador = 0
    xor ebx, ebx                # signo = 0

    # 1. Saltar espacios en blanco y saltos iniciales
.skip_ws_loop:
    call _read_byte
    jc .read_int_finish

    cmp al, ' '
    je .skip_ws_loop
    cmp al, 9                   # '\t'
    je .skip_ws_loop
    cmp al, 13                  # '\r'
    je .skip_ws_loop
    cmp al, 10                  # '\n'
    je .skip_ws_loop

    # 2. Verificar signo '-'
    cmp al, '-'
    jne .check_digit_first
    mov ebx, 1
    jmp .read_int_digits

.check_digit_first:
    cmp al, '0'
    jl .consume_rest_of_line
    cmp al, '9'
    jg .consume_rest_of_line
    sub al, '0'
    movzx esi, al

.read_int_digits:
    call _read_byte
    jc .read_int_finish

    cmp al, '0'
    jl .consume_rest_of_line
    cmp al, '9'
    jg .consume_rest_of_line

    sub al, '0'
    movzx edx, al
    imul esi, 10
    add esi, edx
    jmp .read_int_digits

.consume_rest_of_line:
    cmp al, 10
    je .read_int_finish
    cmp al, 13
    je .consume_lf

.consume_rest_loop:
    call _read_byte
    jc .read_int_finish
    cmp al, 10
    je .read_int_finish
    jmp .consume_rest_loop

.consume_lf:
    call _read_byte             # consumir posible '\n' tras '\r'

.read_int_finish:
    test ebx, ebx
    jz .store_int
    neg esi

.store_int:
    mov [edi], esi

.read_int_done:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

# ========================================================
# 1. MODULO DE MEMORIA DINAMICA Y BUFFERS
# ========================================================

# void* malloc(int size) (Linux sys_brk bump allocator)
.globl fundamental_malloc
.globl _fundamental_malloc
.globl fundamental_malloc
fundamental_malloc:
_fundamental_malloc:
fundamental_malloc:
    push ebp
    mov ebp, esp
    push ebx

    mov eax, [ebp + 8]      # size
    test eax, eax
    jg .malloc_valid_size
    mov eax, 1
.malloc_valid_size:
    add eax, 7
    and eax, -8             # align to 8 bytes
    push eax

    mov ebx, [current_brk]
    test ebx, ebx
    jnz .has_brk

    xor ebx, ebx
    mov eax, 45             # sys_brk = 45
    int 0x80
    mov [current_brk], eax

.has_brk:
    mov ebx, [current_brk]
    pop eax
    push ebx                # return pointer
    add ebx, eax
    mov eax, 45             # sys_brk
    int 0x80
    cmp eax, [esp]
    jbe .malloc_fail
    mov [current_brk], eax
    pop eax
    jmp .malloc_done

.malloc_fail:
    pop eax
    xor eax, eax
.malloc_done:
    pop ebx
    pop ebp
    ret

# void free(void* ptr)
.globl fundamental_free
.globl _fundamental_free
.globl fundamental_free
fundamental_free:
_fundamental_free:
fundamental_free:
    ret

# void* calloc(int num, int size)
.globl fundamental_calloc
.globl _fundamental_calloc
.globl fundamental_calloc
fundamental_calloc:
_fundamental_calloc:
fundamental_calloc:
    push ebp
    mov ebp, esp
    push edi

    mov eax, [ebp + 8]
    imul eax, [ebp + 12]    # bytes = num * size
    push eax
    call fundamental_malloc
    add esp, 4
    test eax, eax
    jz .calloc_done

    mov edi, eax
    mov ecx, [ebp + 8]
    imul ecx, [ebp + 12]
    push eax
    xor al, al
    cld
    rep stosb
    pop eax

.calloc_done:
    pop edi
    pop ebp
    ret

# void* realloc(void* ptr, int new_size)
.globl fundamental_realloc
.globl _fundamental_realloc
.globl fundamental_realloc
fundamental_realloc:
_fundamental_realloc:
fundamental_realloc:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov ebx, [ebp + 8]      # ptr
    mov esi, [ebp + 12]     # new_size

    test ebx, ebx
    jnz .realloc_has_ptr
    push esi
    call fundamental_malloc
    add esp, 4
    jmp .realloc_done

.realloc_has_ptr:
    test esi, esi
    jnz .realloc_valid_size
    xor eax, eax
    jmp .realloc_done

.realloc_valid_size:
    push esi
    call fundamental_malloc
    add esp, 4
    test eax, eax
    jz .realloc_done

    mov edi, eax
    mov ecx, esi
    push eax
    mov esi, ebx
    cld
    rep movsb
    pop eax

.realloc_done:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

# void* memset(void* dest, int value, int count)
_fundamental_memset:
fundamental_memset:
_memset:
    push ebp
    mov ebp, esp
    push edi

    mov edi, [ebp + 8]      # dest
    mov eax, [ebp + 12]     # value (byte en AL)
    mov ecx, [ebp + 16]     # count

    test ecx, ecx
    jle .memset_done

    cld
    rep stosb

.memset_done:
    mov eax, [ebp + 8]
    pop edi
    pop ebp
    ret

# void* memcpy(void* dest, const void* src, int count)
_fundamental_memcpy:
fundamental_memcpy:
_memcpy:
    push ebp
    mov ebp, esp
    push esi
    push edi

    mov edi, [ebp + 8]      # dest
    mov esi, [ebp + 12]     # src
    mov ecx, [ebp + 16]     # count

    test ecx, ecx
    jle .memcpy_done

    cld
    mov edx, ecx
    shr ecx, 2              # copiar dwords
    rep movsd
    mov ecx, edx
    and ecx, 3              # copiar bytes restantes
    rep movsb

.memcpy_done:
    mov eax, [ebp + 8]
    pop edi
    pop esi
    pop ebp
    ret

# void* memmove(void* dest, const void* src, int count)
_fundamental_memmove:
fundamental_memmove:
_memmove:
    push ebp
    mov ebp, esp
    push esi
    push edi

    mov edi, [ebp + 8]      # dest
    mov esi, [ebp + 12]     # src
    mov ecx, [ebp + 16]     # count

    test ecx, ecx
    jle .memmove_done

    cmp edi, esi
    jbe .memmove_fwd

    mov eax, esi
    add eax, ecx
    cmp edi, eax
    jae .memmove_fwd

    # Solapamiento: copiar de atras hacia adelante
    add edi, ecx
    dec edi
    add esi, ecx
    dec esi
    std
    rep movsb
    cld
    jmp .memmove_done

.memmove_fwd:
    cld
    rep movsb

.memmove_done:
    mov eax, [ebp + 8]
    pop edi
    pop esi
    pop ebp
    ret

# int memcmp(const void* s1, const void* s2, int count)
_fundamental_memcmp:
fundamental_memcmp:
_memcmp:
    push ebp
    mov ebp, esp
    push esi
    push edi

    mov esi, [ebp + 8]      # s1
    mov edi, [ebp + 12]     # s2
    mov ecx, [ebp + 16]     # count

    test ecx, ecx
    jle .memcmp_eq

.memcmp_loop:
    mov al, [esi]
    mov dl, [edi]
    cmp al, dl
    jne .memcmp_diff
    inc esi
    inc edi
    dec ecx
    jnz .memcmp_loop

.memcmp_eq:
    xor eax, eax
    jmp .memcmp_ret

.memcmp_diff:
    movzx eax, al
    movzx edx, dl
    sub eax, edx

.memcmp_ret:
    pop edi
    pop esi
    pop ebp
    ret

# ========================================================
# 2. MODULO DE CADENAS DE TEXTO (STRINGS)
# ========================================================

# int strlen(const char* str)
_fundamental_strlen:
fundamental_strlen:
_strlen:
    push ebp
    mov ebp, esp
    push edi

    mov edi, [ebp + 8]
    test edi, edi
    jz .strlen_zero

    xor ecx, ecx
.strlen_loop:
    cmp byte ptr [edi + ecx], 0
    je .strlen_found
    inc ecx
    jmp .strlen_loop

.strlen_found:
    mov eax, ecx
    jmp .strlen_done

.strlen_zero:
    xor eax, eax
.strlen_done:
    pop edi
    pop ebp
    ret

# char* strcpy(char* dest, const char* src)
_fundamental_strcpy:
fundamental_strcpy:
_strcpy:
    push ebp
    mov ebp, esp
    push esi
    push edi

    mov edi, [ebp + 8]
    mov esi, [ebp + 12]
    test edi, edi
    jz .strcpy_done
    test esi, esi
    jz .strcpy_done

.strcpy_loop:
    mov al, [esi]
    mov [edi], al
    inc esi
    inc edi
    test al, al
    jnz .strcpy_loop

.strcpy_done:
    mov eax, [ebp + 8]
    pop edi
    pop esi
    pop ebp
    ret

# char* strncpy(char* dest, const char* src, int n)
_fundamental_strncpy:
fundamental_strncpy:
_strncpy:
    push ebp
    mov ebp, esp
    push esi
    push edi

    mov edi, [ebp + 8]
    mov esi, [ebp + 12]
    mov ecx, [ebp + 16]

    test edi, edi
    jz .strncpy_done
    test ecx, ecx
    jle .strncpy_done

.strncpy_loop:
    test esi, esi
    jz .strncpy_pad
    mov al, [esi]
    mov [edi], al
    inc edi
    dec ecx
    jz .strncpy_done
    test al, al
    jz .strncpy_pad
    inc esi
    jmp .strncpy_loop

.strncpy_pad:
    test ecx, ecx
    jz .strncpy_done
    mov byte ptr [edi], 0
    inc edi
    dec ecx
    jmp .strncpy_pad

.strncpy_done:
    mov eax, [ebp + 8]
    pop edi
    pop esi
    pop ebp
    ret

# char* strcat(char* dest, const char* src)
_fundamental_strcat:
fundamental_strcat:
_strcat:
    push ebp
    mov ebp, esp
    push esi
    push edi

    mov edi, [ebp + 8]
    mov esi, [ebp + 12]
    test edi, edi
    jz .strcat_done
    test esi, esi
    jz .strcat_done

.strcat_find_null:
    cmp byte ptr [edi], 0
    je .strcat_copy
    inc edi
    jmp .strcat_find_null

.strcat_copy:
    mov al, [esi]
    mov [edi], al
    inc esi
    inc edi
    test al, al
    jnz .strcat_copy

.strcat_done:
    mov eax, [ebp + 8]
    pop edi
    pop esi
    pop ebp
    ret

# char* strncat(char* dest, const char* src, int n)
_fundamental_strncat:
fundamental_strncat:
_strncat:
    push ebp
    mov ebp, esp
    push esi
    push edi

    mov edi, [ebp + 8]
    mov esi, [ebp + 12]
    mov ecx, [ebp + 16]

    test edi, edi
    jz .strncat_done
    test esi, esi
    jz .strncat_done
    test ecx, ecx
    jle .strncat_done

.strncat_find_null:
    cmp byte ptr [edi], 0
    je .strncat_copy
    inc edi
    jmp .strncat_find_null

.strncat_copy:
    test ecx, ecx
    jz .strncat_null_term
    mov al, [esi]
    test al, al
    jz .strncat_null_term
    mov [edi], al
    inc esi
    inc edi
    dec ecx
    jmp .strncat_copy

.strncat_null_term:
    mov byte ptr [edi], 0

.strncat_done:
    mov eax, [ebp + 8]
    pop edi
    pop esi
    pop ebp
    ret

# int strcmp(const char* s1, const char* s2)
_fundamental_strcmp:
fundamental_strcmp:
_strcmp:
    push ebp
    mov ebp, esp
    push esi
    push edi

    mov esi, [ebp + 8]
    mov edi, [ebp + 12]

    test esi, esi
    jnz .strcmp_s1_ok
    test edi, edi
    jz .strcmp_both_null
    mov eax, -1
    jmp .strcmp_done
.strcmp_s1_ok:
    test edi, edi
    jnz .strcmp_loop
    mov eax, 1
    jmp .strcmp_done
.strcmp_both_null:
    xor eax, eax
    jmp .strcmp_done

.strcmp_loop:
    mov al, [esi]
    mov dl, [edi]
    cmp al, dl
    jne .strcmp_diff
    test al, al
    jz .strcmp_eq
    inc esi
    inc edi
    jmp .strcmp_loop

.strcmp_diff:
    movzx eax, al
    movzx edx, dl
    sub eax, edx
    jmp .strcmp_done

.strcmp_eq:
    xor eax, eax

.strcmp_done:
    pop edi
    pop esi
    pop ebp
    ret

# int strncmp(const char* s1, const char* s2, int n)
_fundamental_strncmp:
fundamental_strncmp:
_strncmp:
    push ebp
    mov ebp, esp
    push esi
    push edi

    mov esi, [ebp + 8]
    mov edi, [ebp + 12]
    mov ecx, [ebp + 16]

    test ecx, ecx
    jle .strncmp_eq

.strncmp_loop:
    mov al, [esi]
    mov dl, [edi]
    cmp al, dl
    jne .strncmp_diff
    test al, al
    jz .strncmp_eq
    inc esi
    inc edi
    dec ecx
    jnz .strncmp_loop

.strncmp_eq:
    xor eax, eax
    jmp .strncmp_done

.strncmp_diff:
    movzx eax, al
    movzx edx, dl
    sub eax, edx

.strncmp_done:
    pop edi
    pop esi
    pop ebp
    ret

# char* strchr(const char* str, int c)
_fundamental_strchr:
fundamental_strchr:
_strchr:
    push ebp
    mov ebp, esp
    push edi

    mov edi, [ebp + 8]
    mov dl, [ebp + 12]
    test edi, edi
    jz .strchr_null

.strchr_loop:
    mov al, [edi]
    cmp al, dl
    je .strchr_found
    test al, al
    jz .strchr_null
    inc edi
    jmp .strchr_loop

.strchr_found:
    mov eax, edi
    jmp .strchr_done

.strchr_null:
    xor eax, eax
.strchr_done:
    pop edi
    pop ebp
    ret

# char* strrchr(const char* str, int c)
_fundamental_strrchr:
fundamental_strrchr:
_strrchr:
    push ebp
    mov ebp, esp
    push edi

    mov edi, [ebp + 8]
    mov dl, [ebp + 12]
    xor eax, eax
    test edi, edi
    jz .strrchr_done

.strrchr_loop:
    mov cl, [edi]
    cmp cl, dl
    jne .strrchr_next
    mov eax, edi
.strrchr_next:
    test cl, cl
    jz .strrchr_done
    inc edi
    jmp .strrchr_loop

.strrchr_done:
    pop edi
    pop ebp
    ret

# char* strstr(const char* haystack, const char* needle)
_fundamental_strstr:
fundamental_strstr:
_strstr:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov esi, [ebp + 8]
    mov edi, [ebp + 12]

    test esi, esi
    jz .strstr_null
    test edi, edi
    jz .strstr_null

    cmp byte ptr [edi], 0
    je .strstr_found_hs

.strstr_outer:
    cmp byte ptr [esi], 0
    je .strstr_null

    mov ebx, 0
.strstr_inner:
    mov al, [edi + ebx]
    test al, al
    jz .strstr_found_match
    mov cl, [esi + ebx]
    cmp al, cl
    jne .strstr_inner_fail
    inc ebx
    jmp .strstr_inner

.strstr_inner_fail:
    inc esi
    jmp .strstr_outer

.strstr_found_match:
    mov eax, esi
    jmp .strstr_done

.strstr_found_hs:
    mov eax, esi
    jmp .strstr_done

.strstr_null:
    xor eax, eax
.strstr_done:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

# char* strdup(const char* src)
_fundamental_strdup:
fundamental_strdup:
_strdup:
    push ebp
    mov ebp, esp
    push esi
    push edi

    mov esi, [ebp + 8]
    test esi, esi
    jz .strdup_null

    push esi
    call _fundamental_strlen
    add esp, 4

    inc eax                 # len + 1
    push eax
    call _fundamental_malloc
    add esp, 4

    test eax, eax
    jz .strdup_null

    mov edi, eax
    push esi
    push edi
    call _fundamental_strcpy
    add esp, 8
    jmp .strdup_done

.strdup_null:
    xor eax, eax
.strdup_done:
    pop edi
    pop esi
    pop ebp
    ret

# ========================================================
# 3. MODULO DE CARACTERES (CTYPE)
# ========================================================

_fundamental_is_digit:
fundamental_is_digit:
_is_digit:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    cmp eax, '0'
    jl .is_digit_no
    cmp eax, '9'
    jg .is_digit_no
    mov eax, 1
    pop ebp
    ret
.is_digit_no:
    xor eax, eax
    pop ebp
    ret

_fundamental_is_alpha:
fundamental_is_alpha:
_is_alpha:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    cmp eax, 'a'
    jl .is_alpha_upper
    cmp eax, 'z'
    jle .is_alpha_yes
.is_alpha_upper:
    cmp eax, 'A'
    jl .is_alpha_no
    cmp eax, 'Z'
    jle .is_alpha_yes
.is_alpha_no:
    xor eax, eax
    pop ebp
    ret
.is_alpha_yes:
    mov eax, 1
    pop ebp
    ret

_fundamental_is_alnum:
fundamental_is_alnum:
_is_alnum:
    push ebp
    mov ebp, esp
    push dword ptr [ebp + 8]
    call _fundamental_is_alpha
    add esp, 4
    test eax, eax
    jnz .is_alnum_ret
    push dword ptr [ebp + 8]
    call _fundamental_is_digit
    add esp, 4
.is_alnum_ret:
    pop ebp
    ret

_fundamental_is_space:
fundamental_is_space:
_is_space:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    cmp eax, ' '
    je .is_space_yes
    cmp eax, 9
    je .is_space_yes
    cmp eax, 10
    je .is_space_yes
    cmp eax, 13
    je .is_space_yes
    cmp eax, 11
    je .is_space_yes
    cmp eax, 12
    je .is_space_yes
    xor eax, eax
    pop ebp
    ret
.is_space_yes:
    mov eax, 1
    pop ebp
    ret

_fundamental_is_upper:
fundamental_is_upper:
_is_upper:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    cmp eax, 'A'
    jl .is_upper_no
    cmp eax, 'Z'
    jg .is_upper_no
    mov eax, 1
    pop ebp
    ret
.is_upper_no:
    xor eax, eax
    pop ebp
    ret

_fundamental_is_lower:
fundamental_is_lower:
_is_lower:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    cmp eax, 'a'
    jl .is_lower_no
    cmp eax, 'z'
    jg .is_lower_no
    mov eax, 1
    pop ebp
    ret
.is_lower_no:
    xor eax, eax
    pop ebp
    ret

_fundamental_is_xdigit:
fundamental_is_xdigit:
_is_xdigit:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    cmp eax, '0'
    jl .is_xdigit_no
    cmp eax, '9'
    jle .is_xdigit_yes
    cmp eax, 'a'
    jl .is_xdigit_upper
    cmp eax, 'f'
    jle .is_xdigit_yes
.is_xdigit_upper:
    cmp eax, 'A'
    jl .is_xdigit_no
    cmp eax, 'F'
    jle .is_xdigit_yes
.is_xdigit_no:
    xor eax, eax
    pop ebp
    ret
.is_xdigit_yes:
    mov eax, 1
    pop ebp
    ret

_fundamental_to_upper:
fundamental_to_upper:
_to_upper:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    cmp eax, 'a'
    jl .to_upper_ret
    cmp eax, 'z'
    jg .to_upper_ret
    sub eax, 32
.to_upper_ret:
    pop ebp
    ret

_fundamental_to_lower:
fundamental_to_lower:
_to_lower:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    cmp eax, 'A'
    jl .to_lower_ret
    cmp eax, 'Z'
    jg .to_lower_ret
    add eax, 32
.to_lower_ret:
    pop ebp
    ret

# ========================================================
# 4. MODULO DE CONVERSIONES NUMERICAS
# ========================================================

# int atoi(const char* str)
_fundamental_atoi:
fundamental_atoi:
_atoi:
    push ebp
    mov ebp, esp
    push ebx
    push esi

    mov esi, [ebp + 8]
    xor eax, eax
    test esi, esi
    jz .atoi_done

.atoi_skip_ws:
    movzx edx, byte ptr [esi]
    push edx
    call _fundamental_is_space
    add esp, 4
    test eax, eax
    jz .atoi_check_sign
    inc esi
    jmp .atoi_skip_ws

.atoi_check_sign:
    xor ebx, ebx
    cmp byte ptr [esi], '-'
    jne .atoi_check_plus
    mov ebx, 1
    inc esi
    jmp .atoi_digits
.atoi_check_plus:
    cmp byte ptr [esi], '+'
    jne .atoi_digits
    inc esi

.atoi_digits:
    xor eax, eax
.atoi_digit_loop:
    movzx edx, byte ptr [esi]
    cmp edx, '0'
    jl .atoi_finish
    cmp edx, '9'
    jg .atoi_finish
    sub edx, '0'
    imul eax, 10
    add eax, edx
    inc esi
    jmp .atoi_digit_loop

.atoi_finish:
    test ebx, ebx
    jz .atoi_done
    neg eax

.atoi_done:
    pop esi
    pop ebx
    pop ebp
    ret

# char* itoa(int val, char* dest, int radix)
_fundamental_itoa:
fundamental_itoa:
_itoa:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov eax, [ebp + 8]      # val
    mov edi, [ebp + 12]     # dest
    mov ebx, [ebp + 16]     # radix

    cmp ebx, 2
    jl .itoa_fail
    cmp ebx, 36
    jg .itoa_fail
    test edi, edi
    jz .itoa_fail

    xor esi, esi
    cmp ebx, 10
    jne .itoa_unsigned
    test eax, eax
    jns .itoa_unsigned
    neg eax
    mov esi, 1

.itoa_unsigned:
    sub esp, 40
    lea edx, [esp + 36]
    mov byte ptr [edx], 0
    mov ecx, edx

.itoa_div_loop:
    dec ecx
    xor edx, edx
    div ebx
    cmp edx, 10
    jl .itoa_dec_digit
    add dl, 'a' - 10
    jmp .itoa_store_digit
.itoa_dec_digit:
    add dl, '0'
.itoa_store_digit:
    mov [ecx], dl
    test eax, eax
    jnz .itoa_div_loop

    test esi, esi
    jz .itoa_copy
    dec ecx
    mov byte ptr [ecx], '-'

.itoa_copy:
    mov edx, edi
.itoa_cp_loop:
    mov al, [ecx]
    mov [edi], al
    inc ecx
    inc edi
    test al, al
    jnz .itoa_cp_loop

    add esp, 40
    mov eax, edx
    jmp .itoa_done

.itoa_fail:
    xor eax, eax
.itoa_done:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

# int htoi(const char* str)
_fundamental_htoi:
fundamental_htoi:
_htoi:
    push ebp
    mov ebp, esp
    push esi

    mov esi, [ebp + 8]
    xor eax, eax
    test esi, esi
    jz .htoi_done

.htoi_skip_ws:
    cmp byte ptr [esi], ' '
    je .htoi_inc
    cmp byte ptr [esi], 9
    jne .htoi_check_prefix
.htoi_inc:
    inc esi
    jmp .htoi_skip_ws

.htoi_check_prefix:
    cmp byte ptr [esi], '0'
    jne .htoi_loop
    cmp byte ptr [esi + 1], 'x'
    je .htoi_skip_0x
    cmp byte ptr [esi + 1], 'X'
    jne .htoi_loop
.htoi_skip_0x:
    add esi, 2

.htoi_loop:
    movzx edx, byte ptr [esi]
    cmp edx, '0'
    jl .htoi_done
    cmp edx, '9'
    jle .htoi_digit
    cmp edx, 'a'
    jl .htoi_upper
    cmp edx, 'f'
    jle .htoi_lower
    jmp .htoi_done
.htoi_upper:
    cmp edx, 'A'
    jl .htoi_done
    cmp edx, 'F'
    jg .htoi_done
    sub edx, 'A' - 10
    jmp .htoi_accum
.htoi_lower:
    sub edx, 'a' - 10
    jmp .htoi_accum
.htoi_digit:
    sub edx, '0'

.htoi_accum:
    shl eax, 4
    add eax, edx
    inc esi
    jmp .htoi_loop

.htoi_done:
    pop esi
    pop ebp
    ret

# ========================================================
# 5. MODULO MATEMATICO Y NUMEROS PSEUDO-ALEATORIOS
# ========================================================

_fundamental_abs:
fundamental_abs:
_abs:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    cdq
    xor eax, edx
    sub eax, edx
    pop ebp
    ret

_fundamental_min:
fundamental_min:
_min:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    mov edx, [ebp + 12]
    cmp eax, edx
    jle .min_done
    mov eax, edx
.min_done:
    pop ebp
    ret

_fundamental_max:
fundamental_max:
_max:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    mov edx, [ebp + 12]
    cmp eax, edx
    jge .max_done
    mov eax, edx
.max_done:
    pop ebp
    ret

_fundamental_clamp:
fundamental_clamp:
_clamp:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    mov ecx, [ebp + 12]
    mov edx, [ebp + 16]
    cmp eax, ecx
    jge .clamp_check_max
    mov eax, ecx
    jmp .clamp_done
.clamp_check_max:
    cmp eax, edx
    jle .clamp_done
    mov eax, edx
.clamp_done:
    pop ebp
    ret

_fundamental_pow:
fundamental_pow:
_pow:
    push ebp
    mov ebp, esp
    push ebx

    mov ebx, [ebp + 8]      # base
    mov ecx, [ebp + 12]     # exp
    mov eax, 1

    test ecx, ecx
    jl .pow_zero
    jz .pow_done

.pow_loop:
    test ecx, 1
    jz .pow_square
    imul eax, ebx
.pow_square:
    shr ecx, 1
    jz .pow_done
    imul ebx, ebx
    jmp .pow_loop

.pow_zero:
    xor eax, eax
.pow_done:
    pop ebx
    pop ebp
    ret

_fundamental_sqrt:
fundamental_sqrt:
_sqrt:
    push ebp
    mov ebp, esp
    push ebx
    push esi

    mov esi, [ebp + 8]
    test esi, esi
    jle .sqrt_zero

    cmp esi, 3
    jle .sqrt_small

    mov ebx, esi
    shr ebx, 1

.sqrt_loop:
    mov eax, esi
    xor edx, edx
    div ebx
    add eax, ebx
    shr eax, 1

    cmp eax, ebx
    jge .sqrt_done
    mov ebx, eax
    jmp .sqrt_loop

.sqrt_small:
    mov eax, 1
    jmp .sqrt_ret

.sqrt_zero:
    xor eax, eax
    jmp .sqrt_ret

.sqrt_done:
    mov eax, ebx
.sqrt_ret:
    pop esi
    pop ebx
    pop ebp
    ret

_fundamental_gcd:
fundamental_gcd:
_gcd:
    push ebp
    mov ebp, esp
    push ebx

    mov eax, [ebp + 8]
    mov ebx, [ebp + 12]
    test eax, eax
    jns .gcd_pos_a
    neg eax
.gcd_pos_a:
    test ebx, ebx
    jns .gcd_pos_b
    neg ebx
.gcd_pos_b:

.gcd_loop:
    test ebx, ebx
    jz .gcd_done
    cdq
    idiv ebx
    mov eax, ebx
    mov ebx, edx
    jmp .gcd_loop

.gcd_done:
    pop ebx
    pop ebp
    ret

_fundamental_lcm:
fundamental_lcm:
_lcm:
    push ebp
    mov ebp, esp
    push ebx

    mov eax, [ebp + 8]
    mov ebx, [ebp + 12]
    test eax, eax
    jz .lcm_zero
    test ebx, ebx
    jz .lcm_zero

    push ebx
    push eax
    call _fundamental_gcd
    add esp, 8

    test eax, eax
    jz .lcm_zero

    mov ecx, eax
    mov eax, [ebp + 8]
    cdq
    idiv ecx
    imul eax, ebx
    test eax, eax
    jns .lcm_done
    neg eax

.lcm_done:
    pop ebx
    pop ebp
    ret

.lcm_zero:
    xor eax, eax
    pop ebx
    pop ebp
    ret

_fundamental_srand:
fundamental_srand:
_srand:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    mov [rand_seed], eax
    pop ebp
    ret

_fundamental_rand:
fundamental_rand:
_rand:
    push ebp
    mov ebp, esp
    mov eax, [rand_seed]
    imul eax, 1103515245
    add eax, 12345
    mov [rand_seed], eax
    shr eax, 16
    and eax, 0x7FFF
    pop ebp
    ret

# ========================================================
# 6. MODULO DE SISTEMA, PROCESO Y TIEMPO
# ========================================================

_fundamental_exit:
fundamental_exit:
fundamental_exit:
    push ebp
    mov ebp, esp
    mov ebx, [ebp + 8]   # exit code
    mov eax, 1          # sys_exit = 1
    int 0x80
    pop ebp
    ret

_fundamental_sleep:
fundamental_sleep:
fundamental_sleep:
    push ebp
    mov ebp, esp
    push ebx
    push ecx
    sub esp, 8          # struct timespec { long sec; long nsec; }
    mov eax, [ebp + 8]  # ms
    test eax, eax
    jle .sleep_done
    xor edx, edx
    mov ecx, 1000
    div ecx             # eax = sec, edx = rem_ms
    mov [esp], eax
    imul edx, 1000000
    mov [esp + 4], edx
    mov ebx, esp        # req
    xor ecx, ecx        # rem = NULL
    mov eax, 162        # sys_nanosleep = 162
    int 0x80
.sleep_done:
    add esp, 8
    pop ecx
    pop ebx
    pop ebp
    ret

_fundamental_get_tick_count:
fundamental_get_tick_count:
fundamental_get_tick_count:
_fundamental_time:
fundamental_time:
fundamental_time:
    push ebx
    xor ebx, ebx        # time(NULL)
    mov eax, 13         # sys_time = 13
    int 0x80
    pop ebx
    ret

# ========================================================
# 7. MODULO DE I/O DE CARACTERES
# ========================================================

_fundamental_putchar:
fundamental_putchar:
_putchar:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    mov [one_char_buf], al
    mov eax, offset one_char_buf
    mov edx, 1
    call _write_raw
    mov eax, [ebp + 8]
    pop ebp
    ret

_fundamental_getchar:
fundamental_getchar:
_getchar:
    push ebp
    mov ebp, esp
    call _read_byte
    jc .getchar_eof
    movzx eax, al
    pop ebp
    ret
.getchar_eof:
    mov eax, -1
    pop ebp
    ret

# ========================================================
# 8. MODULO DE ARCHIVOS NATIVO WIN32 (kernel32)
# ========================================================

# int file_exists(const char* path) (Linux sys_access syscall 33)
.globl fundamental_file_exists
.globl _fundamental_file_exists
.globl fundamental_file_exists
fundamental_file_exists:
_fundamental_file_exists:
fundamental_file_exists:
    push ebp
    mov ebp, esp
    push ebx
    mov ebx, [ebp + 8]
    test ebx, ebx
    jz .fe_false
    xor ecx, ecx        # F_OK = 0
    mov eax, 33         # sys_access
    int 0x80
    test eax, eax
    jnz .fe_false
    mov eax, 1
    pop ebx
    pop ebp
    ret
.fe_false:
    xor eax, eax
    pop ebx
    pop ebp
    ret

# int file_size(const char* path) (Linux sys_open/sys_lseek/sys_close)
.globl fundamental_file_size
.globl _fundamental_file_size
.globl fundamental_file_size
fundamental_file_size:
_fundamental_file_size:
fundamental_file_size:
    push ebp
    mov ebp, esp
    push ebx
    mov ebx, [ebp + 8]
    test ebx, ebx
    jz .fs_err
    xor ecx, ecx        # O_RDONLY = 0
    xor edx, edx
    mov eax, 5          # sys_open = 5
    int 0x80
    test eax, eax
    js .fs_err
    mov ebx, eax        # ebx = fd
    xor ecx, ecx        # offset = 0
    mov edx, 2          # SEEK_END = 2
    mov eax, 19         # sys_lseek = 19
    int 0x80
    push eax            # save size
    mov eax, 6          # sys_close = 6
    int 0x80
    pop eax             # restore size
    pop ebx
    pop ebp
    ret
.fs_err:
    mov eax, -1
    pop ebx
    pop ebp
    ret

# int file_read_all(const char* path, char* buffer, int max_len)
.globl fundamental_file_read_all
.globl _fundamental_file_read_all
.globl fundamental_file_read_all
fundamental_file_read_all:
_fundamental_file_read_all:
fundamental_file_read_all:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov ebx, [ebp + 8]      # path
    mov edi, [ebp + 12]     # buffer
    mov edx, [ebp + 16]     # max_len
    test ebx, ebx
    jz .fra_err
    test edi, edi
    jz .fra_err
    cmp edx, 1
    jle .fra_err
    dec edx
    push edx
    xor ecx, ecx            # O_RDONLY = 0
    xor edx, edx
    mov eax, 5              # sys_open
    int 0x80
    pop edx
    test eax, eax
    js .fra_err
    mov ebx, eax            # fd
    mov ecx, edi            # buffer
    mov eax, 3              # sys_read
    int 0x80
    push eax                # bytes read
    mov eax, 6              # sys_close
    int 0x80
    pop eax
    test eax, eax
    js .fra_err
    mov byte ptr [edi + eax], 0
    jmp .fra_done
.fra_err:
    mov eax, -1
.fra_done:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

# int file_write_all(const char* path, const char* content)
.globl fundamental_file_write_all
.globl _fundamental_file_write_all
.globl fundamental_file_write_all
fundamental_file_write_all:
_fundamental_file_write_all:
fundamental_file_write_all:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov ebx, [ebp + 8]      # path
    mov esi, [ebp + 12]     # content
    test ebx, ebx
    jz .fwa_err
    test esi, esi
    jz .fwa_err
    push esi
    call fundamental_strlen
    add esp, 4
    mov edi, eax            # length
    mov ecx, 577            # O_WRONLY | O_CREAT | O_TRUNC = 01101 = 577
    mov edx, 0666           # mode
    mov eax, 5              # sys_open
    int 0x80
    test eax, eax
    js .fwa_err
    mov ebx, eax            # fd
    mov ecx, esi            # content
    mov edx, edi            # length
    mov eax, 4              # sys_write
    int 0x80
    push eax
    mov eax, 6              # sys_close
    int 0x80
    pop eax
    jmp .fwa_done
.fwa_err:
    mov eax, -1
.fwa_done:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

# int file_append(const char* path, const char* content)
.globl fundamental_file_append
.globl _fundamental_file_append
.globl fundamental_file_append
fundamental_file_append:
_fundamental_file_append:
fundamental_file_append:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov ebx, [ebp + 8]      # path
    mov esi, [ebp + 12]     # content
    test ebx, ebx
    jz .fap_err
    test esi, esi
    jz .fap_err
    push esi
    call fundamental_strlen
    add esp, 4
    mov edi, eax            # length
    mov ecx, 1089           # O_WRONLY | O_CREAT | O_APPEND = 02101 = 1089
    mov edx, 0666           # mode
    mov eax, 5              # sys_open
    int 0x80
    test eax, eax
    js .fap_err
    mov ebx, eax            # fd
    mov ecx, esi            # content
    mov edx, edi            # length
    mov eax, 4              # sys_write
    int 0x80
    push eax
    mov eax, 6              # sys_close
    int 0x80
    pop eax
    jmp .fap_done
.fap_err:
    mov eax, -1
.fap_done:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

# int file_remove(const char* path)
.globl fundamental_file_remove
.globl _fundamental_file_remove
.globl fundamental_file_remove
fundamental_file_remove:
_fundamental_file_remove:
fundamental_file_remove:
    push ebp
    mov ebp, esp
    push ebx
    mov ebx, [ebp + 8]
    test ebx, ebx
    jz .frm_err
    mov eax, 10         # sys_unlink = 10
    int 0x80
    test eax, eax
    jnz .frm_err
    mov eax, 1
    pop ebx
    pop ebp
    ret
.frm_err:
    xor eax, eax
    pop ebx
    pop ebp
    ret

# int char_at(const char* str, int index)
.globl _fundamental_char_at
.globl fundamental_char_at
.globl _char_at
_fundamental_char_at:
fundamental_char_at:
_char_at:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]   # str
    mov edx, [ebp + 12]  # index
    test eax, eax
    jz .char_at_zero
    movzx eax, byte ptr [eax + edx]
    pop ebp
    ret
.char_at_zero:
    xor eax, eax
    pop ebp
    ret

# void set_char_at(char* str, int index, int ch)
.globl _fundamental_set_char_at
.globl fundamental_set_char_at
.globl _set_char_at
_fundamental_set_char_at:
fundamental_set_char_at:
_set_char_at:
    push ebp
    mov ebp, esp
    mov edx, [ebp + 8]   # str
    mov ecx, [ebp + 12]  # index
    mov eax, [ebp + 16]  # ch
    test edx, edx
    jz .set_char_at_done
    mov [edx + ecx], al
.set_char_at_done:
    pop ebp
    ret

# ========================================================
# Manejo de Saltos No Locales (setjmp / longjmp) para Itsuki x86
# ========================================================
.globl _itsuki_setjmp
.globl itsuki_setjmp
.globl _setjmp
.globl setjmp
_itsuki_setjmp:
itsuki_setjmp:
_setjmp:
setjmp:
    mov edx, [esp + 4]    # puntero a jmp_buf
    mov [edx + 0],  ebx   # guardar ebx
    mov [edx + 4],  esi   # guardar esi
    mov [edx + 8],  edi   # guardar edi
    mov [edx + 12], ebp   # guardar ebp
    lea ecx, [esp + 4]    # esp del llamador tras retorno
    mov [edx + 16], ecx   # guardar esp
    mov ecx, [esp]        # direccion de retorno (eip)
    mov [edx + 20], ecx   # guardar eip
    xor eax, eax          # retorno 0 en llamada inicial
    ret

.globl _itsuki_longjmp
.globl itsuki_longjmp
.globl _longjmp
.globl longjmp
_itsuki_longjmp:
itsuki_longjmp:
_longjmp:
longjmp:
    mov edx, [esp + 4]    # puntero a jmp_buf
    mov eax, [esp + 8]    # valor de retorno (val)
    test eax, eax
    jnz .itsuki_longjmp_ok
    mov eax, 1            # si val == 0, por estandar retorna 1
.itsuki_longjmp_ok:
    mov ebx, [edx + 0]    # restaurar ebx
    mov esi, [edx + 4]    # restaurar esi
    mov edi, [edx + 8]    # restaurar edi
    mov ebp, [edx + 12]   # restaurar ebp
    mov esp, [edx + 16]   # restaurar esp del llamador
    mov ecx, [edx + 20]   # recuperar eip
    jmp ecx               # saltar al llamador


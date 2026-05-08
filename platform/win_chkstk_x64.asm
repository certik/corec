; ============================================================================
; corec __chkstk implementation for Windows x64.
;
; The MSVC compiler emits a call to __chkstk at the start of any function
; whose stack frame exceeds one page (4 KB). __chkstk's job is to walk
; downward through every page the caller is about to allocate and touch
; one byte on each, so that the kernel commits the page (and moves the
; stack guard page along) before the caller actually does `sub rsp, rax`
; and starts using those slots. Without this probe, a function with a
; frame larger than a page can bump RSP past the guard page into
; uncommitted memory and the OS terminates the process with a silent
; STATUS_ACCESS_VIOLATION (0xC0000005) that cannot be caught by SEH/VEH
; because the stack itself is broken before the exception dispatcher runs.
;
; ABI on x64 (matches MSVC's runtime __chkstk):
;   - On entry, RAX contains the requested allocation size in bytes.
;   - The function must NOT modify RSP -- the caller emits the actual
;     `sub rsp, rax` after the call returns.
;   - The function may clobber R10 and R11; all other registers
;     (including RAX, RCX, RDX, R8, R9) and flags must be preserved.
;     We save R10/R11 ourselves so even those are preserved on return.
;   - The same routine is correct for both /MD (CRT) and /kernel mode
;     because we walk pages relative to RSP rather than consulting any
;     thread-environment-block field.
;
; We also export __chkstk_ms (the GCC/clang name) and _chkstk as
; aliases so this single object satisfies whatever name the toolchain
; happens to emit.
; ============================================================================

PUBLIC __chkstk
PUBLIC __chkstk_ms
PUBLIC _chkstk

_TEXT SEGMENT

ALIGN 16
__chkstk PROC
__chkstk_ms LABEL PROC
_chkstk LABEL PROC

    push    r10                     ; preserve R10
    push    r11                     ; preserve R11

    ; Compute pointer to the byte just below the caller's RSP. After
    ; the two pushes plus the return address pushed by `call`, the
    ; caller's RSP at the call site is rsp+24 (0x18).
    lea     r11, [rsp+18h]
    mov     r10, rax                ; r10 = remaining bytes to probe

    cmp     r10, 1000h
    jb      probe_tail              ; <= one page: jump straight to the
                                    ; final small probe

probe_loop:
    sub     r11, 1000h              ; step down one page
    mov     byte ptr [r11], 0       ; touch it (commits the guard page)
    sub     r10, 1000h
    cmp     r10, 1000h
    ja      probe_loop

probe_tail:
    ; Probe the last (possibly partial) page so the very bottom of the
    ; frame is committed too.
    sub     r11, r10
    mov     byte ptr [r11], 0

    pop     r11
    pop     r10
    ret

__chkstk ENDP

_TEXT ENDS
END

;
; ovl.s - copy the OVERLAY segment (menus, shop, main loop) to $0334-$07FF.
; Its load image follows ONCE and is overwritten by the BSS, so this runs as a
; constructor: crt0 calls the constructors before it clears the BSS.
; Nothing may write to $0334-$07FF after this (no conio output, no tape).
;
        .constructor copy_overlay
        .import __OVERLAY_LOAD__, __OVERLAY_RUN__, __OVERLAY_SIZE__
        .importzp ptr1, ptr2

        .segment "ONCE"

copy_overlay:
        lda     #<__OVERLAY_LOAD__
        sta     ptr1
        lda     #>__OVERLAY_LOAD__
        sta     ptr1+1
        lda     #<__OVERLAY_RUN__
        sta     ptr2
        lda     #>__OVERLAY_RUN__
        sta     ptr2+1
        ldx     #>__OVERLAY_SIZE__      ; whole pages
        ldy     #0
        cpx     #0
        beq     rest
page:   lda     (ptr1),y
        sta     (ptr2),y
        iny
        bne     page
        inc     ptr1+1
        inc     ptr2+1
        dex
        bne     page
rest:   cpy     #<__OVERLAY_SIZE__      ; y = 0 here
        beq     done
        lda     (ptr1),y
        sta     (ptr2),y
        iny
        bne     rest
done:   rts

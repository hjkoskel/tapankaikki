; map.s - draws the 40x22 character playfield from the 2x2-char metatile map.
;
; Inputs (set from C before calling draw_map):
;   _dm_map  pointer to the map byte of the top-left visible tile
;   _dm_w    map width in tiles
;   _dm_px   1 if the camera starts on the right half of a tile (char x odd)
;   _dm_py   1 if the camera starts on the bottom half of a tile (char y odd)
;   _dm_scr  destination: first screen byte of the view (hidden buffer)
;   _dm_rows number of character rows (22 full screen, 12 per split view)
; About 24 cycles per character, ~21,000 cycles for 22 rows.

        .export _draw_map
        .export _dm_map, _dm_w, _dm_px, _dm_py, _dm_scr, _dm_rows
        .import _mt_tl, _mt_tr, _mt_bl, _mt_br
        .importzp ptr1, ptr2, tmp1, tmp2, tmp3

        .bss
_dm_map: .res 2
_dm_w:   .res 1
_dm_px:  .res 1
_dm_py:  .res 1
_dm_scr: .res 2
_dm_rows: .res 1

        .code

; patch the table operands of the row loops: A/X = low/high of left table,
; tmp1/... are not touched here
.macro  SETTAB left, right
        lda     #<left
        sta     L0+1
        sta     L1+1
        sta     L1b+1
        lda     #>left
        sta     L0+2
        sta     L1+2
        sta     L1b+2
        lda     #<right
        sta     R0+1
        sta     R1+1
        sta     R1a+1
        lda     #>right
        sta     R0+2
        sta     R1+2
        sta     R1a+2
.endmacro

_draw_map:
        lda     _dm_map
        sta     ptr1
        lda     _dm_map+1
        sta     ptr1+1
        lda     _dm_scr
        sta     ptr2
        lda     _dm_scr+1
        sta     ptr2+1
        lda     _dm_py
        sta     tmp2            ; 0 = top half of tile row, 1 = bottom half
        lda     _dm_rows
        sta     tmp3

dm_row:   lda     tmp2
        bne     dm_bottom
        SETTAB  _mt_tl, _mt_tr
        jmp     dm_tabok
dm_bottom:
        SETTAB  _mt_bl, _mt_br
dm_tabok: lda     _dm_px
        bne     dm_odd

; ---- even: 20 tiles, chars 2t and 2t+1
        ldy     #0
dm_loop0: sty     tmp1
        lda     (ptr1),y
        tax
        tya
        asl     a
        tay
L0:     lda     $FFFF,x
        sta     (ptr2),y
        iny
R0:     lda     $FFFF,x
        sta     (ptr2),y
        ldy     tmp1
        iny
        cpy     #20
        bne     dm_loop0
        jmp     dm_next

; ---- odd: right half of tile 0, tiles 1..19, left half of tile 20
dm_odd:   ldy     #0
        lda     (ptr1),y
        tax
R1a:    lda     $FFFF,x
        sta     (ptr2),y
        ldy     #1
dm_loop1: sty     tmp1
        lda     (ptr1),y
        tax
        tya
        asl     a
        tay
        dey
L1:     lda     $FFFF,x
        sta     (ptr2),y
        iny
R1:     lda     $FFFF,x
        sta     (ptr2),y
        ldy     tmp1
        iny
        cpy     #20
        bne     dm_loop1
        lda     (ptr1),y        ; y = 20
        tax
        ldy     #39
L1b:    lda     $FFFF,x
        sta     (ptr2),y

; ---- next screen row
dm_next:  clc
        lda     ptr2
        adc     #40
        sta     ptr2
        bcc     :+
        inc     ptr2+1
:       lda     tmp2
        beq     dm_half
        clc                     ; bottom half done: next tile row
        lda     ptr1
        adc     _dm_w
        sta     ptr1
        bcc     :+
        inc     ptr1+1
:
dm_half:  lda     tmp2
        eor     #1
        sta     tmp2
        dec     tmp3
        beq     dm_done
        jmp     dm_row
dm_done:  rts

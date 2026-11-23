; irq.s - raster interrupt: screen flip and hardware sprites.
;
; The game writes sprite shadow registers; the interrupt copies them to the
; VIC. Set 0 is copied at raster line 251 (lower border, also the frame
; counter and the $D018 screen flip). In split screen mode a second interrupt
; at line _irq_split (the HUD row between the views) copies set 1, so each
; view has all 8 hardware sprites. The game keeps view 0 sprites above the
; split line and view 1 sprites a few lines below it, so the registers can be
; rewritten while no sprite is being displayed.
;
; CIA1 interrupts are switched off: the game reads keyboard and joysticks
; itself, and the long KERNAL keyboard scan would delay the split interrupt.

        .export _irq_init, _irq_frame, _irq_d018, _irq_split
        .export _spr_x, _spr_y, _spr_ptr, _spr_col, _spr_ena, _spr_msb, _spr_mc

TOPLINE = 251
SPRPTR_A = $C3F8
SPRPTR_B = $C7F8

        .bss
_spr_x:     .res 16
_spr_y:     .res 16
_spr_ptr:   .res 16
_spr_col:   .res 16
_spr_ena:   .res 2
_spr_msb:   .res 2
_spr_mc:    .res 2
_irq_frame: .res 1
_irq_d018:  .res 1
_irq_split: .res 1              ; 0 = one view, else raster line of the split
phase:      .res 1

        .code

; copy sprite set S to the VIC: Y positions first (they must be in place
; before the sprite's first line), then the rest. About 320 cycles.
.macro  COPYSET S
  .repeat 8, I
        lda     _spr_y+S*8+I
        sta     $D001+I*2
  .endrepeat
  .repeat 8, I
        lda     _spr_x+S*8+I
        sta     $D000+I*2
        lda     _spr_col+S*8+I
        sta     $D027+I
        lda     _spr_ptr+S*8+I
        sta     SPRPTR_A+I
        sta     SPRPTR_B+I
  .endrepeat
        lda     _spr_msb+S
        sta     $D010
        lda     _spr_mc+S
        sta     $D01C
        lda     _spr_ena+S
        sta     $D015
.endmacro

_irq_init:
        sei
        lda     #$7F
        sta     $DC0D           ; CIA1 interrupts off
        lda     $DC0D
        lda     $D018
        sta     _irq_d018
        lda     #0
        sta     phase
        sta     _irq_split
        sta     _spr_ena
        sta     _spr_ena+1
        lda     #<irq
        sta     $0314
        lda     #>irq
        sta     $0315
        lda     #TOPLINE
        sta     $D012
        lda     $D011
        and     #$7F
        sta     $D011
        lda     #1
        sta     $D01A           ; raster interrupt on
        lda     #$FF
        sta     $D019
        cli
        rts

irq:    lda     $D019
        and     #1
        bne     :+
        jmp     $EA31           ; not ours: KERNAL interrupt
:       sta     $D019           ; acknowledge
        lda     phase
        beq     top
        jmp     bottom
top:

        lda     _irq_d018       ; lower border: flip screens, sprite set 0
        sta     $D018
        COPYSET 0
        inc     _irq_frame
        lda     _irq_split
        beq     done
        sta     $D012
        inc     phase
done:   jmp     $EA81           ; KERNAL: restore registers, RTI

bottom: COPYSET 1
        lda     #TOPLINE
        sta     $D012
        lda     #0
        sta     phase
        jmp     $EA81

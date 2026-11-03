; NES Harbor's music: two voices, a square wave's melody and the
; triangle's bass, from music_data.inc (tools/gen_music.py).
;
; The scope keeps its NMI off, so nothing plays the music on a clock:
; music_tick goes once for each vblank waited for (wait_vblank, and
; restore_scroll, which ends each vblank's writes). A tune that loops goes
; back to its start; one that does not stops. music_on is the player's: B on
; the picker's list.

SIXTEENTH = 7                   ; frames
SQ1_VOL  = $4000
SQ1_SWEEP = $4001
SQ1_LO   = $4002
SQ1_HI   = $4003
TRI_LIN  = $4008
TRI_LO   = $400A
TRI_HI   = $400B
SQ_TONE  = %10110110            ; duty 50%, held, constant volume 6
SQ_QUIET = %10110000
TRI_ON   = %11111111
TRI_OFF  = %10000000
TRI_UP   = 12                   ; the triangle sounds an octave under the table
TUNE_LOOP = $FF
TUNE_END  = $FE
MELODY = 0                      ; a voice, as an offset into the two-byte tables
BASS   = 2

.segment "ZEROPAGE"
voice_ptr:      .res 4          ; each voice's next note
.segment "BSS"
voice_start:    .res 4          ; and its start
voice_wait:     .res 4          ; sixteenths left of the note sounding
music_frame:    .res 1          ; frames left of the sixteenth
music_playing:  .res 1
music_tune:     .res 1
music_on:       .res 1          ; 0: the player has it off

.segment "RODATA"
tune_melody_lo: .byte <tune_sailor_melody, <tune_fanfare_melody
tune_melody_hi: .byte >tune_sailor_melody, >tune_fanfare_melody
tune_bass_lo:   .byte <tune_sailor_bass, <tune_fanfare_bass
tune_bass_hi:   .byte >tune_sailor_bass, >tune_fanfare_bass
TUNE_SAILOR  = 0
TUNE_FANFARE = 1
.include "music_data.inc"

.segment "CODE"

; A = TUNE_*. The one already playing goes on as it is.
.proc music_play
    tax
    lda music_playing
    beq :+
    cpx music_tune
    beq @done
:
    stx music_tune
    lda tune_melody_lo,x
    sta voice_start + MELODY
    lda tune_melody_hi,x
    sta voice_start + MELODY + 1
    lda tune_bass_lo,x
    sta voice_start + BASS
    lda tune_bass_hi,x
    sta voice_start + BASS + 1
    ldx #3
:
    lda voice_start,x
    sta voice_ptr,x
    dex
    bpl :-
    lda #1
    sta voice_wait + MELODY
    sta voice_wait + BASS
    sta music_frame
    sta music_playing
    lda #%00000101              ; the first square and the triangle
    sta APUSTATUS
    lda #$08                    ; no sweep
    sta SQ1_SWEEP
@done:
    rts
.endproc

.proc music_silence
    lda #SQ_QUIET
    sta SQ1_VOL
    lda #TRI_OFF
    sta TRI_LIN
    rts
.endproc

; B on the picker: the music off, or on again.
.proc music_toggle
    lda music_on
    eor #1
    sta music_on
    bne :+
    jmp music_silence
:
    lda #1                      ; each voice's next note at once
    sta voice_wait + MELODY
    sta voice_wait + BASS
    sta music_frame
    rts
.endproc

; Once a frame. Keeps X and Y.
.proc music_tick
    lda music_playing
    beq @done
    lda music_on
    beq @done
    txa
    pha
    tya
    pha
    dec music_frame
    bne @out
    lda #SIXTEENTH
    sta music_frame
    ldx #MELODY
    jsr voice_step
    ldx #BASS
    jsr voice_step
@out:
    pla
    tay
    pla
    tax
@done:
    rts
.endproc

; A sixteenth of voice X: the next note when this one is done.
.proc voice_step
    dec voice_wait,x
    beq @next
    lda voice_wait,x            ; the melody's last sixteenth: a breath
    cmp #1
    bne @done
    cpx #MELODY
    bne @done
    lda #SQ_QUIET
    sta SQ1_VOL
@done:
    rts

@next:
    lda (voice_ptr,x)
    cmp #TUNE_END
    bne :+
    lda #0
    sta music_playing
    jmp music_silence
:
    cmp #TUNE_LOOP
    bne @note
    lda voice_start,x           ; from the start again
    sta voice_ptr,x
    lda voice_start + 1,x
    sta voice_ptr + 1,x
    lda (voice_ptr,x)
@note:
    tay                         ; the note: 0 a rest
    jsr voice_advance
    lda (voice_ptr,x)           ; and its length
    sta voice_wait,x
    jsr voice_advance
    cpx #MELODY
    bne @bass
    cpy #0
    beq @sq_rest
    lda #SQ_TONE
    sta SQ1_VOL
    lda note_period_lo,y
    sta SQ1_LO
    lda note_period_hi,y
    sta SQ1_HI
    rts
@sq_rest:
    lda #SQ_QUIET
    sta SQ1_VOL
    rts
@bass:
    cpy #0
    beq @tri_rest
    tya
    clc
    adc #TRI_UP
    tay
    lda #TRI_ON
    sta TRI_LIN
    lda note_period_lo,y
    sta TRI_LO
    lda note_period_hi,y
    sta TRI_HI
    rts
@tri_rest:
    lda #TRI_OFF
    sta TRI_LIN
    rts
.endproc

.proc voice_advance
    inc voice_ptr,x
    bne :+
    inc voice_ptr + 1,x
:
    rts
.endproc

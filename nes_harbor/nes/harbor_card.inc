; NES Harbor's ship card: what AIS says of the ship chosen on the map.
;
; A tapped on the map (pressed and let go with no direction, which would be
; a pan) asks for the card. The screen is the startup's font on black: a
; title, the nesp32's sixteen lines of the ship (HARBOR's DETAILS), one an
; exchange, and how to leave. B or A goes back to the map, which is drawn
; and asked for again.

.ifndef NESP_HARBOR_DETAILS
NESP_HARBOR_DETAILS = $06
.endif
.ifndef NESP_HARBOR_DETAIL_LINES
NESP_HARBOR_DETAIL_LINES = 16
.endif
.ifndef NESP_HARBOR_DETAIL_WIDTH
NESP_HARBOR_DETAIL_WIDTH = 24
.endif

CARD_LINES = NESP_HARBOR_DETAIL_LINES
CARD_WIDTH = NESP_HARBOR_DETAIL_WIDTH
CARD_FIRST_ROW = 6
CARD_COLUMN = 4
CARD_TITLE_ADDR = $2088         ; row 4, column 8
CARD_FOOT_ADDR = $234B          ; row 26, column 11
CARD_HEAD = 4                   ; STATE, ID, NAMES, LINE
CARD_TRIES = 60                 ; frames a line is asked for before it is passed over

.segment "BSS"
card_armed:     .res 1          ; A is down, and has not panned
card_a_was:     .res 1          ; A was down at the last look
card_wanted:    .res 1          ; the map asks for the card
card_line:      .res 1
card_tries:     .res 1
card_tiles:     .res CARD_WIDTH

.segment "RODATA"
card_row_lo:
    .repeat CARD_LINES, i
        .byte <($2000 + (CARD_FIRST_ROW + i) * 32 + CARD_COLUMN)
    .endrepeat
card_row_hi:
    .repeat CARD_LINES, i
        .byte >($2000 + (CARD_FIRST_ROW + i) * 32 + CARD_COLUMN)
    .endrepeat
card_title:
    tiles FONT_DIM_BASE, "  SHIP DETAILS  "
card_foot:
    tiles FONT_DIM_BASE, "B  BACK"
card_no_details:
    tiles FONT_DIM_BASE, "UPDATE NESP32 FOR CARDS "
.assert * - card_no_details = CARD_WIDTH, error, "the message is a line"

.segment "CODE"

; From update_selection: A tapped asks for the card. A pan (harbor_pan)
; while A is down takes the tap away.
.proc card_watch
    lda controller_current
    and #BUTTON_A
    tax
    beq @up
    lda card_a_was
    bne @was                    ; held from before
    lda #1                      ; just pressed
    sta card_armed
@was:
    stx card_a_was
    rts
@up:
    lda card_a_was
    beq @done
    lda card_armed              ; let go: a tap, unless it panned
    beq @done
    lda active_slots
    beq @done
    lda #1
    sta card_wanted
@done:
    lda #0
    sta card_a_was
    sta card_armed
    rts
.endproc

; The card, until B or A. Returns with the screen still the card's: the
; caller draws the map again.
.proc ship_card
    lda #0
    sta card_wanted
    sta card_armed
    jsr card_video
    lda #0
    sta card_line
@line:
    lda #CARD_TRIES
    sta card_tries
@ask:
    jsr wait_vblank
    lda radar_id
    sta nesp_request
    lda selected_slot
    sta nesp_request + 1
    lda card_line
    sta nesp_request + 2
    lda #NESP_SVC_HARBOR
    ldx #NESP_HARBOR_DETAILS
    ldy #3
    jsr nesp_call
    bcs @again                  ; no answer this time
    cmp #NESP_ST_OK
    bne @none                   ; a firmware with no DETAILS
    lda nesp_reply_len
    cmp #CARD_HEAD + CARD_WIDTH
    bne @again                  ; no ship in the slot yet, or another place's
    lda nesp_reply + 1
    cmp radar_id
    bne @again
    lda nesp_reply + 3
    cmp card_line
    bne @again
    ldx #0
@char:
    lda nesp_reply + CARD_HEAD,x
    jsr card_tile
    sta card_tiles,x
    inx
    cpx #CARD_WIDTH
    bne @char
    jsr card_write_line
    jmp @next
@again:
    dec card_tries
    bne @ask
@next:
    inc card_line
    lda card_line
    cmp #CARD_LINES
    bne @line
    jmp @wait

@none:
    ldx #CARD_WIDTH - 1
:
    lda card_no_details,x
    sta card_tiles,x
    dex
    bpl :-
    lda #0
    sta card_line
    jsr card_write_line

@wait:
    jsr wait_vblank
    jsr read_controller
    lda controller_previous
    eor #$FF
    and controller_current
    tax
    lda controller_current
    sta controller_previous
    txa
    and #(BUTTON_B | BUTTON_A)
    beq @wait
@release:                       ; the map is drawn once A and B are let go,
    jsr wait_vblank             ; which it would take as presses of its own
    jsr read_controller
    lda controller_current
    and #(BUTTON_B | BUTTON_A)
    bne @release
    lda #0
    sta card_a_was
    sta card_armed
    rts
.endproc

; card_tiles as row card_line of the card, in a vblank.
.proc card_write_line
    lda #<card_tiles
    sta field_src
    lda #>card_tiles
    sta field_src + 1
    lda #0
    sta field_font
    jsr begin_vram_update
    ldx card_line
    lda card_row_hi,x
    pha
    lda card_row_lo,x
    tax                         ; write_field: A the address's high byte, X its low
    pla
    ldy #CARD_WIDTH
    jsr write_field
    jmp restore_scroll
.endproc

; A in ASCII, as the card's tile: the font's characters, bright; anything
; else a space.
.proc card_tile
    cmp #'a'
    bcc :+
    cmp #'z' + 1
    bcs :+
    and #<~$20
:
    ldy #0
@find:
    cmp card_charset,y
    beq @found
    iny
    cpy #CARD_CHARSET_LEN
    bne @find
    ldy #0                      ; a space
@found:
    tya
    clc
    adc #FONT_DIM_BASE
    rts
.endproc

.segment "RODATA"
card_charset:                   ; the font's order: glyph n is card_charset[n]
    .byte " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./:"
CARD_CHARSET_LEN = * - card_charset
.assert CARD_CHARSET_LEN = FONT_GLYPHS, error, "the card's characters are the font's"
.segment "CODE"

; The card's screen, with rendering off: black, the title and the way back.
.proc card_video
    lda #0
    sta PPUMASK
    lda #CHR_BANK_RADAR
    jsr select_chr_bank
    jsr load_ldv_palette
    bit PPUSTATUS
    lda #$20
    sta PPUADDR
    lda #$00
    sta PPUADDR
    lda #CHAR_SPACE
    ldx #0
    ldy #4
:
    sta PPUDATA
    inx
    bne :-
    dey
    bne :-
    lda #0
    sta field_font
    lda #<card_title
    sta field_src
    lda #>card_title
    sta field_src + 1
    ldy #16
    lda #>CARD_TITLE_ADDR
    ldx #<CARD_TITLE_ADDR
    jsr write_field
    lda #<card_foot
    sta field_src
    lda #>card_foot
    sta field_src + 1
    ldy #7
    lda #>CARD_FOOT_ADDR
    ldx #<CARD_FOOT_ADDR
    jsr write_field
    ldx #0
    lda #$FF
:
    sta oam_shadow,x
    inx
    bne :-
    lda #0
    sta OAMADDR
    lda #>oam_shadow
    sta OAMDMA
    lda #0
    sta PPUSCROLL
    sta PPUSCROLL
    lda #PPUCTRL_SPR_8X16
    sta PPUCTRL
    lda #SHOW_STARTUP
    sta PPUMASK
    rts
.endproc

; NES Harbor's ship card: what AIS says of the ship chosen on the map.
;
; A tapped on the map (pressed and let go with no direction, which would be
; a pan) asks for the card. The screen is tools/make_overworld.py's: a
; yellow frame, a picture of a ship of the selected one's group, and the
; nesp32's sixteen lines of the ship (HARBOR's DETAILS), one an exchange,
; each put where card_route says: the name, MMSI and type by the picture,
; the rest as an orange label (the line's first word) and its white value.
; B or A goes back to the map, which is drawn again from what is kept.

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
CARD_HEAD = 4                   ; STATE, ID, NAMES, LINE
CARD_TRIES = 60                 ; frames a line is asked for before it is passed over
CARD_ROW_WIDTH = 28             ; columns 2 to 29, inside the frame
CARD_RIGHT_WIDTH = 30 - SHIP_RIGHT_COL
CARD_LABEL_WIDTH = SHIP_VALUE_COL - SHIP_LABEL_COL - 1
CARD_VALUE_WIDTH = 30 - SHIP_VALUE_COL
ROUTE_WHOLE = 0                 ; the line as it is, by the picture
ROUTE_VALUE = 1                 ; what is after its first word, by the picture
ROUTE_LABEL = 2                 ; its first word, and the rest, below

.segment "BSS"
card_armed:     .res 1          ; A is down, and has not panned
card_a_was:     .res 1          ; A was down at the last look
card_wanted:    .res 1          ; the map asks for the card
card_line:      .res 1
card_tries:     .res 1
card_tiles:     .res CARD_WIDTH ; the line, as glyphs
card_out:       .res CARD_ROW_WIDTH

.segment "RODATA"
; Where each of DETAILS' lines goes: its row, and how.
;   0 name, 1 MMSI, 2 IMO, 3 CALL, 4 FLAG, 5 TYPE, 6 STATUS, 7 DEST, 8 ETA,
;   9 SIZE, 10 DRAFT, 11 SOG and COG, 12 HEADING, 13 LAT, 14 LON, 15 HEARD
card_route_mode:
    .byte ROUTE_WHOLE, ROUTE_WHOLE, ROUTE_LABEL, ROUTE_LABEL, ROUTE_LABEL, ROUTE_VALUE
    .byte ROUTE_LABEL, ROUTE_LABEL, ROUTE_LABEL, ROUTE_LABEL, ROUTE_LABEL, ROUTE_LABEL
    .byte ROUTE_LABEL, ROUTE_LABEL, ROUTE_LABEL, ROUTE_LABEL
.assert * - card_route_mode = CARD_LINES, error, "a route for each line"
card_no_details:
    tiles 0, "UPDATE NESP32 FOR CARDS "
.assert * - card_no_details = CARD_WIDTH, error, "the message is a line"
CARD_NO_DETAILS_LINE = 11       ; shown where SOG is: UPDATE, and the rest

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
    jsr card_glyph
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
    lda #CARD_NO_DETAILS_LINE
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

; card_tiles as line card_line of the card, where card_route puts it, in a
; vblank.
.proc card_write_line
    ldx #CARD_ROW_WIDTH - 1
    lda #CHAR_SPACE
:
    sta card_out,x
    dex
    bpl :-
    ldx card_line
    lda card_route_mode,x
    cmp #ROUTE_LABEL
    beq @label
    ldy #0                      ; by the picture: from the line's start, or
    cmp #ROUTE_WHOLE            ; after its first word
    beq :+
    jsr card_after_word
:
    ldx #0
@right:
    cpy #CARD_WIDTH
    bcs @right_done
    lda card_tiles,y
    clc
    adc #FONT_DIM_BASE
    sta card_out,x
    iny
    inx
    cpx #CARD_RIGHT_WIDTH
    bne @right
@right_done:
    lda #SHIP_RIGHT_COL
    ldy #CARD_RIGHT_WIDTH
    jmp @write

@label:
    ldy #0                      ; the first word, in FONT_BASE (orange)
    ldx #SHIP_LABEL_COL - 2
@word:
    lda card_tiles,y
    beq @value
    cpx #SHIP_LABEL_COL - 2 + CARD_LABEL_WIDTH
    bcs :+
    sta card_out,x
    inx
:
    iny
    cpy #CARD_WIDTH
    bne @word
@value:
    ldy #0
    jsr card_after_word
    ldx #SHIP_VALUE_COL - 2     ; the rest, in FONT_DIM_BASE (white)
:
    cpy #CARD_WIDTH
    bcs :+
    lda card_tiles,y
    clc
    adc #FONT_DIM_BASE
    sta card_out,x
    iny
    inx
    cpx #CARD_ROW_WIDTH
    bne :-
:
    lda #2
    ldy #CARD_ROW_WIDTH

@write:                         ; A the column, Y the width
    sty field_width
    ldx card_line
    clc
    adc card_row_lo,x           ; the route's row's address
    sta field_addr_lo
    lda card_row_hi,x
    adc #0
    sta field_addr_hi
    lda #<card_out
    sta field_src
    lda #>card_out
    sta field_src + 1
    lda #0
    sta field_font
    jsr begin_vram_update
    lda field_addr_hi
    ldx field_addr_lo
    ldy field_width
    jsr write_field
    jmp restore_scroll
.endproc

; Y = where the line's value begins: after its first word and the spaces
; after that.
.proc card_after_word
    ldy #0
:
    lda card_tiles,y
    beq :+
    iny
    cpy #CARD_WIDTH
    bne :-
    rts
:
    iny
    cpy #CARD_WIDTH
    bcs :+
    lda card_tiles,y
    beq :-
:
    rts
.endproc

.segment "RODATA"
card_row_lo:                    ; a line's row's start, by its number
    .byte <($2000 + 2 * 32),<($2000 + 3 * 32),<($2000 + 16 * 32),<($2000 + 15 * 32),<($2000 + 12 * 32),<($2000 + 4 * 32),<($2000 + 9 * 32),<($2000 + 10 * 32),<($2000 + 11 * 32),<($2000 + 13 * 32),<($2000 + 14 * 32),<($2000 + 7 * 32),<($2000 + 8 * 32),<($2000 + 17 * 32),<($2000 + 18 * 32),<($2000 + 19 * 32)
card_row_hi:
    .byte >($2000 + 2 * 32),>($2000 + 3 * 32),>($2000 + 16 * 32),>($2000 + 15 * 32),>($2000 + 12 * 32),>($2000 + 4 * 32),>($2000 + 9 * 32),>($2000 + 10 * 32),>($2000 + 11 * 32),>($2000 + 13 * 32),>($2000 + 14 * 32),>($2000 + 7 * 32),>($2000 + 8 * 32),>($2000 + 17 * 32),>($2000 + 18 * 32),>($2000 + 19 * 32)
.assert * - card_row_hi = CARD_LINES, error, "a row for each line"
.segment "CODE"

; A in ASCII as the font's glyph: capitals, digits, space and + - . / :,
; small letters as capitals, anything else a space.
.proc card_glyph
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
    rts
.endproc

.segment "RODATA"
card_charset:                   ; the font's order: glyph n is card_charset[n]
    .byte " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./:"
CARD_CHARSET_LEN = * - card_charset
.assert CARD_CHARSET_LEN = FONT_GLYPHS, error, "the card's characters are the font's"
.segment "CODE"

; The card's screen, with rendering off: the frame, the picture of the
; selected ship's group with its hull's colour, and the way back.
.proc card_video
    lda #0
    sta PPUMASK
    sta ppu_bg
    lda #CHR_BANK_RADAR
    jsr select_chr_bank
    ldx selected_slot
    ldy ship_group,x
    sty view_tmp2
    lda group_card_hull,y
    jsr load_card_palette
    bit PPUSTATUS
    lda #$20
    sta PPUADDR
    lda #$00
    sta PPUADDR
    lda #<ship_card_nam
    sta view_ptr_a
    lda #>ship_card_nam
    sta view_ptr_a + 1
    jsr copy_nametable
    lda view_tmp2               ; the group's picture: group * 40 tiles
    asl a
    asl a
    asl a
    sta view_tmp
    asl a
    asl a
    clc
    adc view_tmp
    tay
    ldx #0
@row:
    bit PPUSTATUS
    lda ship_pic_hi,x
    sta PPUADDR
    lda ship_pic_lo,x
    sta PPUADDR
    lda #SHIP_PIC_W
    sta view_tmp
:
    lda ship_pictures,y
    sta PPUDATA
    iny
    dec view_tmp
    bne :-
    inx
    cpx #SHIP_PIC_H
    bne @row
    jmp screen_on
.endproc

.segment "RODATA"
ship_pic_lo:
    .repeat SHIP_PIC_H, i
        .byte <(SHIP_PIC_ADDR + i * 32)
    .endrepeat
ship_pic_hi:
    .repeat SHIP_PIC_H, i
        .byte >(SHIP_PIC_ADDR + i * 32)
    .endrepeat
.segment "CODE"

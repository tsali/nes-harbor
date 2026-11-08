; NES Harbor's overworld: the port card, the map, its ships and its status bar.
;
; The art is tools/make_overworld.py's (assets/overworld): pattern table
; $0000 has the picker's font and the cards' pictures, $1000 the boats, the
; map's tiles and the status bar's font on black. The map screen shows its
; background from $1000 (ppu_bg); every other screen from $0000.
;
; THE MAP is HARBOR's land and water mask, 36 by 36 cells of 4 scope points
; (harbor_map.inc keeps it in map_mask), one 8x8 tile to a cell: the
; middle 32 columns and 24 rows of it, so a scope point is two pixels. A cell
; is drawn by its neighbours: land with a rocky edge on each side that has
; water (OW_LAND + those sides), water by land with surf on each side that
; has land (OW_SURF + those sides), and open water plain or with a wave.
; A 16x16 block with land in it takes palette 0, any other palette 1, whose
; two wave colours the frame cycles (wave_tick). A row is made in RAM and
; written in a vblank with the OAM (render_map_row, vq_flush).
;
; THE SHIPS are 16x16 boats, one of eight headings from the record's track,
; coloured by their AIS type's group (sprite palettes 0-3), each with a
; wake astern when it is moving. The selected one has a yellow box and is
; first in the OAM; the others and the wakes take turns to be first
; (oam_rotation), so that where more than eight sprites share a line, as
; the NES draws only eight, each ship flickers rather than vanishing.
;
; THE STATUS BAR, under the map: the port, the ship count, the range, the
; link's state and the AIS SIGNAL meter. The meter is how fresh the AIS
; feed is: ten bars while the NESP32 answers and AISHub's data is current,
; one bar fewer for each second (64 frames) without a good reply, at most
; four while the data is stale (WAITING) and none when AIS is down, the
; NESP32 does not answer, or its data is bad.
;
; THE NAME TAG, at the top right of the map: the selected ship's name,
; the AIS ship type and its speed. The name is the record's eight letters
; until the full one comes from line 0 of its card (tag_turn).

.include "overworld.inc"
.segment "CODE"

STAT_DIRTY_PORT  = $01
STAT_DIRTY_LINE  = $02
STAT_DIRTY_METER = $04
STAT_DIRTY_ALL   = $07
MAP_CELLS        = 36
CELL_STRIDE      = 5            ; bytes to a row of the mask, a bit to a cell
CELL_BUFFER      = 40
SIGNAL_BARS      = 10
SIGNAL_LIVE      = 10
SIGNAL_STALE     = 4
VQ_SIZE          = 80
PORT_CARD_MIN    = 120          ; frames the port card shows at least
PORT_CARD_MAX    = 420          ; and at most, if the data is slow
WAVE_FRAMES      = 20           ; between steps of the waves' colours (Rai 11/8: a tad slower)
TAG_TRIES        = 20
SHIP_X_MIN       = 12           ; scope points that put a boat whole on the map
SHIP_X_MAX       = 131
SHIP_Y_MIN       = 29
SHIP_Y_MAX       = 119
SPRITE_PAL_CURSOR = 2           ; SP2's colour 2 is yellow
SPRITE_PAL_WAKE  = 0            ; SP0's colour 3 is white

.segment "ZEROPAGE"
ppu_bg:         .res 1          ; $10 when the background is from $1000
view_ptr_a:     .res 2
view_ptr_b:     .res 2
view_tmp:       .res 1
view_tmp2:      .res 1
view_row:       .res 1          ; the map's screen row being made
cell_row:       .res 1          ; and its row of the mask
fleck_base:     .res 1
vq_at:          .res 1
oam_at:         .res 1
att_index:      .res 1
quad_base:      .res 1
row_end:        .res 1

.segment "BSS"
map_mask:       .res MAP_CELLS * CELL_STRIDE
cells_n:        .res CELL_BUFFER    ; the row above, this row and the one below,
cells_c:        .res CELL_BUFFER    ; a byte to a cell: 1 land
cells_s:        .res CELL_BUFFER
block_land:     .res 16
attr_shadow:    .res 48         ; the map's attribute rows, 0 to 5
render_row:     .res 1          ; the next screen row to draw; MAP_SCREEN_ROWS when none
render_water:   .res 1          ; draw open water, not the mask
vq_len:         .res 1
vq_buf:         .res VQ_SIZE    ; address high, low, count, tiles; again
line_buf:       .res 2 * TAG_TEXT
place_name:     .res PORT_NAME_LEN  ; the place, as the font's glyphs
status_dirty:   .res 1
tag_dirty:      .res 1
tag_fetch:      .res 1          ; the slot whose full name to ask for; $FF none
tag_name_slot:  .res 1          ; the slot whose full name is in tag_name; $FF none
tag_tries:      .res 1
tag_name:       .res TAG_TEXT
meter_shown:    .res 1
signal_cap:     .res 1
wave_phase:     .res 1
wave_timer:     .res 1
port_timer:     .res 2
port_count_shown: .res 1
ship_left:      .res MAX_TARGETS    ; the boat's left edge on the screen
ship_top:       .res MAX_TARGETS    ; its OAM Y; $FF when it is off the map
ship_head:      .res MAX_TARGETS    ; heading 0 (N) to 7 (NW); bit 7: no wake
ship_speed:     .res MAX_TARGETS    ; knots; $FF unknown
ship_group:     .res MAX_TARGETS    ; 0 cargo/tanker, 1 passenger, 2 service, 3 other

.segment "RODATA"
quad_keep:
    .byte $FC, $F3, $CF, $3F
quad_one:
    .byte $01, $04, $10, $40
fleck_pick:
    .byte OW_FLECK_A, OW_SEA, OW_SEA, OW_SEA, OW_SEA, OW_SEA, OW_SEA, OW_SEA
    .byte OW_SEA, OW_SEA, OW_SEA, OW_FLECK_B, OW_SEA, OW_SEA, OW_SEA, OW_SEA
wave_colours_1:                 ; BG1's colours 1 and 2, a step at a time
    .byte $22, $12, SEA_COLOUR, $12
wave_colours_2:
    .byte SEA_COLOUR, $12, $22, $12
zip_label:
    tiles 0, "ZIP "
kt_label:
    tiles 0, "KT"
nm_label:
    tiles 0, "NM"
no_ships_label:
    tiles 0, "NO SHIPS        "
.assert * - no_ships_label = TAG_TEXT, error, "a line of the tag"
SEA_COLOUR = $02

.segment "CODE"

; ---------------------------------------------------------------------------
; The place's name, as the port card and the status bar show it: the port's
; from the list, or ZIP and the zip code.
.proc build_place_name
    lda picker_port
    cmp #PORT_COUNT
    bcs @zip
    jsr port_name_pointer
    ldy #PORT_NAME_LEN - 1
:
    lda (place_ptr),y
    sta place_name,y
    dey
    bpl :-
    rts
@zip:
    ldx #PORT_NAME_LEN - 1
    lda #CHAR_SPACE
:
    sta place_name,x
    dex
    bpl :-
    ldx #3
:
    lda zip_label,x
    sta place_name,x
    dex
    bpl :-
    ldx #ZIP_LEN - 1
:
    lda zip_code,x
    sec
    sbc #'0' - CHAR_0
    sta place_name + 4,x
    dex
    bpl :-
    rts
.endproc

; ---------------------------------------------------------------------------
; The VRAM queue: entries made outside vblank, written in one.

; A:X = the address, Y = the width, field_src the glyphs and field_font
; what is added to each. Into the queue.
.proc vq_field
    sta field_addr_hi
    stx field_addr_lo
    sty field_width
    ldx vq_len
    lda field_addr_hi
    sta vq_buf,x
    inx
    lda field_addr_lo
    sta vq_buf,x
    inx
    lda field_width
    sta vq_buf,x
    inx
    ldy #0
:
    lda (field_src),y
    clc
    adc field_font
    sta vq_buf,x
    inx
    iny
    cpy field_width
    bne :-
    stx vq_len
    rts
.endproc

; In a vblank, or with rendering off: the queue to the PPU, and empty it.
; About fifteen cycles a byte.
.proc vq_flush
    ldx #0
@entry:
    cpx vq_len
    beq @done
    bit PPUSTATUS
    lda vq_buf,x
    sta PPUADDR
    inx
    lda vq_buf,x
    sta PPUADDR
    inx
    ldy vq_buf,x
    inx
@byte:
    lda vq_buf,x
    sta PPUDATA
    inx
    dey
    bne @byte
    beq @entry
@done:
    lda #0
    sta vq_len
    rts
.endproc

; ---------------------------------------------------------------------------
; The map's rows.

; A = a row of the mask; view_ptr_a = where its cells go, a byte each.
.proc unpack_cells
    sta view_tmp
    asl a
    asl a
    clc
    adc view_tmp                ; row * 5
    tax
    ldy #0
@byte:
    lda map_mask,x
    sta view_tmp2
    lda #8
    sta view_tmp
@bit:
    lda #0
    asl view_tmp2
    rol a
    sta (view_ptr_a),y
    iny
    dec view_tmp
    bne @bit
    inx
    cpy #CELL_BUFFER
    bne @byte
    rts
.endproc

.macro point_at ptr, buffer
    lda #<(buffer)
    sta ptr
    lda #>(buffer)
    sta ptr + 1
.endmacro

; Screen row A of the map into the queue: its tiles (but where the name tag
; is) and its attribute row. About 6000 cycles, outside vblank.
.proc render_map_row
    sta view_row
    clc
    adc #MAP_CELL_ROW0
    sta cell_row
    lda render_water
    bne @water
    point_at view_ptr_a, cells_n
    lda cell_row
    sec
    sbc #1
    jsr unpack_cells
    point_at view_ptr_a, cells_c
    lda cell_row
    jsr unpack_cells
    point_at view_ptr_a, cells_s
    lda cell_row
    clc
    adc #1
    jsr unpack_cells
    jmp @cells
@water:
    lda #0
    ldx #CELL_BUFFER - 1
:
    sta cells_n,x
    sta cells_c,x
    sta cells_s,x
    dex
    bpl :-
@cells:
    ; Which blocks have land: the block's rows are this one and the next,
    ; or the last and this one.
    lda view_row
    and #1
    bne @odd
    point_at view_ptr_a, cells_c
    point_at view_ptr_b, cells_s
    jmp @blocks
@odd:
    point_at view_ptr_a, cells_n
    point_at view_ptr_b, cells_c
@blocks:
    ldx #0
    ldy #MAP_CELL_COL0
@block:
    lda (view_ptr_a),y
    ora (view_ptr_b),y
    iny
    ora (view_ptr_a),y
    ora (view_ptr_b),y
    iny
    sta block_land,x
    inx
    cpx #16
    bne @block

    lda cell_row                ; the waves' place in fleck_pick
    asl a
    asl a
    adc cell_row                ; row * 5 (carry clear: row < 36)
    sta fleck_base

    ; The queue's entry: the row's address, and 32 tiles, or 18 where the
    ; name tag is.
    ldx vq_len
    lda view_row
    lsr a
    lsr a
    lsr a
    ora #$20
    sta vq_buf,x
    inx
    lda view_row
    asl a
    asl a
    asl a
    asl a
    asl a
    sta vq_buf,x
    inx
    lda #MAP_SCREEN_COLS
    ldy view_row
    cpy #TAG_ROW
    bcc :+
    cpy #TAG_ROW + 4
    bcs :+
    lda #TAG_COL
:
    sta vq_buf,x
    inx
    stx vq_at
    clc
    adc #MAP_CELL_COL0
    sta row_end

    ldx #MAP_CELL_COL0
@tile:
    lda cells_c - 1,x           ; land at W S E N, as bits 3 2 1 0
    asl a
    ora cells_s,x
    asl a
    ora cells_c + 1,x
    asl a
    ora cells_n,x
    ldy cells_c,x
    beq @sea
    eor #$0F                    ; land: the sides with water
    ora #OW_LAND
    bne @put
@sea:
    cmp #0
    beq @open
    ora #OW_SURF                ; water by land: the sides with land
    bne @put
@open:
    lda cells_n - 1,x
    ora cells_n + 1,x
    ora cells_s - 1,x
    ora cells_s + 1,x
    beq @deep
    lda #OW_SURF                ; land at a corner only
    bne @put
@deep:
    txa
    sec
    sbc #MAP_CELL_COL0
    lsr a
    tay
    lda block_land,y
    bne @plain                  ; land's palette: no waves
    txa
    clc
    adc fleck_base
    and #15
    tay
    lda fleck_pick,y
    bne @put
@plain:
    lda #OW_SEA
@put:
    ldy vq_at
    sta vq_buf,y
    iny
    sty vq_at
    inx
    cpx row_end
    bne @tile
    sty vq_len

    ; The attribute row: each block's quarter of its byte, palette 0 for
    ; land, 1 for open water.
    lda view_row
    lsr a
    and #1
    asl a
    sta quad_base               ; 0 the top two quarters, 2 the bottom
    lda view_row
    lsr a
    lsr a
    asl a
    asl a
    asl a
    sta view_tmp2               ; the attribute row * 8
    ldx #0
@quarter:
    txa
    lsr a
    clc
    adc view_tmp2
    sta att_index
    txa
    and #1
    ora quad_base
    tay
    stx view_tmp
    ldx att_index
    lda attr_shadow,x
    and quad_keep,y
    ldx view_tmp
    pha
    lda block_land,x
    bne :+
    pla
    ora quad_one,y
    pha
:
    pla
    ldx att_index
    sta attr_shadow,x
    ldx view_tmp
    inx
    cpx #16
    bne @quarter
    ; The name tag's, rows 2 to 5 from column 14: white (2) in rows 2-3,
    ; yellow (3) in rows 4-5.
    .assert TAG_ROW = 2 .and TAG_COL = 14, error, "the tag's attributes are for rows 2-5, columns 14-31"
    lda attr_shadow + 3
    and #$3F
    ora #$80
    sta attr_shadow + 3
    lda attr_shadow + 11
    and #$F3
    ora #$0C
    sta attr_shadow + 11
    ldx #3
:
    lda attr_shadow + 4,x
    and #$0F
    ora #$A0
    sta attr_shadow + 4,x
    lda attr_shadow + 12,x
    and #$F0
    ora #$0F
    sta attr_shadow + 12,x
    dex
    bpl :-
    ldx vq_len
    lda #$23
    sta vq_buf,x
    inx
    lda view_tmp2
    ora #$C0
    sta vq_buf,x
    inx
    lda #8
    sta vq_buf,x
    inx
    ldy view_tmp2
:
    lda attr_shadow,y
    sta vq_buf,x
    inx
    iny
    tya
    and #7
    bne :-
    stx vq_len
    rts
.endproc

; A new map, or water over the old one: from the top, a row a frame.
.proc map_render_begin
    sta render_water
    lda #0
    sta render_row
    rts
.endproc

; ---------------------------------------------------------------------------
; The ships.

; slot * 24, the slot's names in identity_tiles.
.proc times24
    asl a
    asl a
    asl a
    sta view_tmp
    asl a
    clc
    adc view_tmp
    rts
.endproc

; X = a slot: A = its group, from the AIS type's two digits in its names.
; 7x 8x cargo and tankers (0); 6x passenger and 4x high speed (1); 5x
; pilots, search and rescue, tugs, tenders, law, medical, 31-33 and 35
; towing, dredging and military (2); the rest, sail, pleasure and
; fishing among them (3).
.proc group_of_slot
    txa
    jsr times24
    tay
    lda identity_tiles + 22,y
    sec
    sbc #CHAR_0
    cmp #10
    bcs @other
    cmp #9
    beq @other
    cmp #7
    bcs @cargo
    cmp #6
    beq @passenger
    cmp #4
    beq @passenger
    cmp #5
    beq @service
    cmp #3
    bne @other
    lda identity_tiles + 23,y
    sec
    sbc #CHAR_0
    beq @other                  ; 30 fishing
    cmp #4
    bcc @service                ; 31 32 towing, 33 dredging
    cmp #5
    beq @service                ; 35 military
@other:
    lda #3
    rts
@cargo:
    lda #0
    rts
@passenger:
    lda #1
    rts
@service:
    lda #2
    rts
.endproc

; The scene's records as the boats on the map: where, which way, how fast,
; and which group. Slots that are not in the scene are off the map.
.proc ships_from_scene
    ldx #MAX_TARGETS - 1
    lda #$FF
:
    sta ship_top,x
    sta ship_speed,x
    dex
    bpl :-
    lda packet_count
    sta records_left
    ldy #0
@record:
    lda records_left
    bne :+
    rts
:
    sty rec_offset
    lda scene_payload,y
    sta record_flags
    and #7
    tax
    jsr group_of_slot
    sta ship_group,x
    ldy rec_offset
    lda record_flags            ; the heading, if the record has a track
    and #TRACK_INVALID
    beq :+
    lda #$80                    ; none: drawn as north, with no wake
    bne @headed
:
    lda scene_payload + 3,y
    clc
    adc #16                     ; the nearest of eight
    lsr a
    lsr a
    lsr a
    lsr a
    lsr a
@headed:
    sta ship_head,x
    lda record_flags
    and #SPEED_INVALID
    bne @still
    lda scene_payload + 6,y
    sta ship_speed,x
    bne @placed
@still:
    lda ship_head,x             ; unknown or no speed: no wake
    ora #$80
    sta ship_head,x
@placed:
    lda scene_payload + 1,y     ; X, scope points: on the map's columns?
    cmp #SHIP_X_MIN
    bcc @next
    cmp #SHIP_X_MAX + 1
    bcs @next
    sec
    sbc #SHIP_X_MIN
    asl a                       ; 2X - 16 is the middle, less 8 the left
    sta ship_left,x
    lda scene_payload + 2,y
    cmp #SHIP_Y_MIN
    bcc @next
    cmp #SHIP_Y_MAX + 1
    bcs @next
    asl a
    sec
    sbc #57                     ; 2Y - 48 the middle, less 8 the top, less 1
    sta ship_top,x
@next:
    tya
    clc
    adc #RECORD_SIZE
    tay
    dec records_left
    jmp @record
.endproc

; X = a slot on the map: its boat, two 8x16 sprites, at oam_at.
.proc emit_boat
    ldy oam_at
    lda ship_top,x
    sta oam_shadow + 0,y
    sta oam_shadow + 4,y
    lda ship_head,x
    and #7
    asl a
    asl a                       ; heading * 4
    ora #SPR_BOAT | 1           ; odd: the pattern table at $1000
    sta oam_shadow + 1,y
    clc
    adc #2
    sta oam_shadow + 5,y
    lda ship_group,x
    sta oam_shadow + 2,y
    sta oam_shadow + 6,y
    lda ship_left,x
    sta oam_shadow + 3,y
    clc
    adc #8
    sta oam_shadow + 7,y
    tya
    clc
    adc #8
    sta oam_at
    rts
.endproc

; X = a slot on the map: its wake, if it has one and it is on the screen.
.proc emit_wake
    lda ship_head,x
    bmi @none
    tay
    lda ship_left,x
    clc
    adc #8                      ; the boat's middle
    sta view_tmp
    lda wake_dx,y
    bmi @left
    clc
    adc view_tmp
    bcs @none
    bcc @x
@left:
    clc
    adc view_tmp
    bcc @none                   ; under 0
@x:
    sta view_tmp
    lda ship_top,x
    clc
    adc #8                      ; the middle, less 1 as OAM's Y is
    sta view_tmp2
    lda wake_dy,y
    clc
    adc view_tmp2
    cmp #200
    bcs @none
    sta view_tmp2
    tya
    asl a
    ora #SPR_WAKE | 1
    ldy oam_at
    sta oam_shadow + 1,y
    lda view_tmp2
    sta oam_shadow + 0,y
    lda #SPRITE_PAL_WAKE
    sta oam_shadow + 2,y
    lda view_tmp
    sta oam_shadow + 3,y
    tya
    clc
    adc #4
    sta oam_at
@none:
    rts
.endproc

; X = a slot: carry clear if it is in the scene and on the map.
.proc slot_shown
    lda slot_masks,x
    and active_slots
    beq @no
    lda ship_top,x
    cmp #$FF
    beq @no
    clc
    rts
@no:
    sec
    rts
.endproc

; The OAM for this frame: the cursor and the selected boat first, then the
; others from oam_rotation round, then the wakes the same way.
.proc build_ship_oam
    lda #$FF
    ldx #0
:
    sta oam_shadow,x
    inx
    inx
    inx
    inx
    bne :-
    lda #0
    sta oam_at
    lda active_slots
    bne :+
    rts
:
    ldx selected_slot
    jsr slot_shown
    bcs @others
    ldy oam_at
    lda ship_top,x
    sta oam_shadow + 0,y
    sta oam_shadow + 4,y
    lda #SPR_BOX | 1
    sta oam_shadow + 1,y
    sta oam_shadow + 5,y
    lda #SPRITE_PAL_CURSOR
    sta oam_shadow + 2,y
    lda #SPRITE_PAL_CURSOR | $40    ; the right half: the left flipped
    sta oam_shadow + 6,y
    lda ship_left,x
    sta oam_shadow + 3,y
    clc
    adc #8
    sta oam_shadow + 7,y
    tya
    clc
    adc #8
    sta oam_at
    jsr emit_boat
@others:
    lda #0
    sta temp
@boat:
    lda oam_rotation
    clc
    adc temp
    and #7
    tax
    cpx selected_slot
    beq :+
    jsr slot_shown
    bcs :+
    jsr emit_boat
:
    inc temp
    lda temp
    cmp #MAX_TARGETS
    bne @boat
    ldx selected_slot
    jsr slot_shown
    bcs :+
    jsr emit_wake
:
    lda #0
    sta temp
@wake:
    lda oam_rotation
    clc
    adc temp
    and #7
    tax
    cpx selected_slot
    beq :+
    jsr slot_shown
    bcs :+
    jsr emit_wake
:
    inc temp
    lda temp
    cmp #MAX_TARGETS
    bne @wake
    rts
.endproc

; ---------------------------------------------------------------------------
; The status bar and the name tag, into the queue.

.proc queue_port_line
    point_at field_src, place_name
    lda #PANEL_FONT
    sta field_font
    ldy #PORT_NAME_LEN
    lda #>STAT_PORT
    ldx #<STAT_PORT
    jmp vq_field
.endproc

; A = a number under 100: its two digits as glyphs in line_buf, X onward.
.proc two_digits
    ldy #CHAR_0
:
    cmp #10
    bcc :+
    sbc #10
    iny
    bne :-
:
    clc
    adc #CHAR_0
    sta line_buf + 1,x
    tya
    sta line_buf,x
    rts
.endproc

; The ship count, the range and the link's state.
.proc queue_stat_line
    point_at field_src, line_buf
    lda #PANEL_FONT
    sta field_font
    lda displayed_count
    ldx #0
    jsr two_digits
    point_at field_src, line_buf + 1    ; one digit: there are eight at most
    ldy #1
    lda #>STAT_COUNT
    ldx #<STAT_COUNT
    jsr vq_field
    jsr range_digits
    lda picker_tiles + ZIP_LEN
    sta line_buf
    lda picker_tiles + ZIP_LEN + 1
    sta line_buf + 1
    lda nm_label
    sta line_buf + 2
    lda nm_label + 1
    sta line_buf + 3
    point_at field_src, line_buf
    ldy #4
    lda #>STAT_RANGE
    ldx #<STAT_RANGE
    jsr vq_field
    ldx #LINK_STATE_WIDTH - 1   ; the link: LIVE in green
:
    lda status_fields + STATUS_LINK,x
    clc
    adc #PANEL_FONT
    sta line_buf,x
    dex
    bpl :-
    lda link_live
    beq :++
    ldx #3
:
    txa
    clc
    adc #PANEL_LIVE
    sta line_buf,x
    dex
    bpl :-
:
    lda #0
    sta field_font
    ldy #LINK_STATE_WIDTH
    lda #>STAT_LINK
    ldx #<STAT_LINK
    jmp vq_field
.endproc

; A = the meter's bars, as it should be now.
.proc signal_now
    lda link_age_hi             ; seconds of 64 frames since a good reply
    asl a
    asl a
    sta view_tmp
    lda link_age_lo
    rol a
    rol a
    rol a
    and #3
    ora view_tmp
    sta view_tmp
    lda #SIGNAL_BARS
    sec
    sbc view_tmp
    bcs :+
    lda #0
:
    cmp signal_cap
    bcc :+
    lda signal_cap
:
    rts
.endproc

.proc queue_meter
    jsr signal_now
    sta meter_shown
    ldx #0
:
    lda #PANEL_BAR_ON
    cpx meter_shown
    bcc :+
    lda #PANEL_BAR_OFF
:
    sta line_buf,x
    inx
    cpx #SIGNAL_BARS
    bne :--
    point_at field_src, line_buf
    lda #0
    sta field_font
    ldy #SIGNAL_BARS
    lda #>STAT_METER
    ldx #<STAT_METER
    jmp vq_field
.endproc

; The name tag's two lines: the name; the type and the speed.
.proc queue_tag
    ldx #TAG_TEXT - 1
    lda #CHAR_SPACE
:
    sta line_buf,x
    sta line_buf + TAG_TEXT,x
    dex
    bpl :-
    lda active_slots
    bne @ship
    ldx #TAG_TEXT - 1
:
    lda no_ships_label,x
    sta line_buf,x
    dex
    bpl :-
    jmp @queue
@ship:
    lda selected_slot
    cmp tag_name_slot
    bne @short
    ldx #TAG_TEXT - 1
:
    lda tag_name,x
    sta line_buf,x
    dex
    bpl :-
    jmp @kind
@short:
    jsr times24
    tay
    ldx #0
:
    lda identity_tiles,y
    sta line_buf,x
    iny
    inx
    cpx #8
    bne :-
@kind:
    lda selected_slot
    jsr times24
    tay
    ldx #0
:
    lda identity_tiles + 8,y    ; the AIS type, as a word of four
    sta line_buf + TAG_TEXT,x
    iny
    inx
    cpx #4
    bne :-
    ldx selected_slot
    lda ship_speed,x
    cmp #$FF
    beq @queue
    sta value_lo
    lda #0
    sta value_hi
    jsr convert_u16_3digits
    ldx #TAG_TEXT + TAG_TEXT - 6    ; the speed, then KT, at the right
    lda digit_hundreds
    beq :+
    clc
    adc #CHAR_0
    sta line_buf,x
:
    inx
    lda digit_hundreds
    ora digit_tens
    beq :+
    lda digit_tens
    clc
    adc #CHAR_0
    sta line_buf,x
:
    inx
    lda digit_ones
    clc
    adc #CHAR_0
    sta line_buf,x
    lda kt_label
    sta line_buf + TAG_TEXT + TAG_TEXT - 2
    lda kt_label + 1
    sta line_buf + TAG_TEXT + TAG_TEXT - 1
@queue:
    point_at field_src, line_buf
    lda #PANEL_FONT
    sta field_font
    ldy #TAG_TEXT
    lda #>TAG_NAME_ADDR
    ldx #<TAG_NAME_ADDR
    jsr vq_field
    point_at field_src, line_buf + TAG_TEXT
    ldy #TAG_TEXT
    lda #>TAG_INFO_ADDR
    ldx #<TAG_INFO_ADDR
    jmp vq_field
.endproc

; The selected ship has changed, or its names: the tag again, and its full
; name asked for.
.proc tag_changed
    lda #1
    sta tag_dirty
    lda #$FF
    sta tag_name_slot
    lda selected_slot
    sta tag_fetch
    lda #TAG_TRIES
    sta tag_tries
    rts
.endproc

; A frame between SCENEs (harbor_turn): the selected ship's full name, line
; 0 of its card, if it is wanted. Carry set if the frame's exchange is used.
.proc tag_turn
    lda tag_fetch
    bpl :+
    clc
    rts
:
    lda radar_id
    sta nesp_request
    lda tag_fetch
    sta nesp_request + 1
    lda #0
    sta nesp_request + 2
    lda #NESP_SVC_HARBOR
    ldx #NESP_HARBOR_DETAILS
    ldy #3
    jsr nesp_call
    bcs @again
    cmp #NESP_ST_OK
    bne @stop                   ; a firmware with no cards: the short name stays
    lda nesp_reply_len
    cmp #CARD_HEAD + CARD_WIDTH
    bne @again
    lda nesp_reply + 1
    cmp radar_id
    bne @again
    lda nesp_reply + 3
    bne @again
    ldx #0
:
    lda nesp_reply + CARD_HEAD,x
    jsr card_glyph
    sta tag_name,x
    inx
    cpx #TAG_TEXT
    bne :-
    lda tag_fetch
    sta tag_name_slot
    lda #1
    sta tag_dirty
@stop:
    lda #$FF
    sta tag_fetch
    sec
    rts
@again:
    dec tag_tries
    beq @stop
    sec
    rts
.endproc

; The waves' two colours, a step every WAVE_FRAMES, in a vblank.
.proc wave_tick
    dec wave_timer
    bne @done
    lda #WAVE_FRAMES
    sta wave_timer
    lda wave_phase
    clc
    adc #1
    and #3
    sta wave_phase
    tax
    bit PPUSTATUS
    lda #$3F
    sta PPUADDR
    lda #$05
    sta PPUADDR
    lda wave_colours_1,x
    sta PPUDATA
    lda wave_colours_2,x
    sta PPUDATA
@done:
    rts
.endproc

; ---------------------------------------------------------------------------
; The map screen.

; With rendering off: the map from map_mask if there is one whole (or
; water), the name tag, the status bar, and the ships.
.proc map_video
    lda #0
    sta PPUMASK
    sta vq_len
    lda #$10
    sta ppu_bg
    lda #CHR_BANK_RADAR
    jsr select_chr_bank
    bit PPUSTATUS
    lda #$3F
    sta PPUADDR
    lda #$00
    sta PPUADDR
    ldx #0
:
    lda map_palette,x
    sta PPUDATA
    inx
    cpx #32
    bne :-
    ldx #47
:
    lda map_attributes,x
    sta attr_shadow,x
    dex
    bpl :-

    ldx #1                      ; a whole map, or water
    lda map_have
    beq :+
    ldx #0
:
    stx render_water
    lda #0
    sta render_row
@row:
    lda render_row
    jsr render_map_row
    jsr vq_flush
    inc render_row
    lda render_row
    cmp #MAP_SCREEN_ROWS
    bne @row

    ldx #0                      ; the name tag's frame
@tag_row:
    bit PPUSTATUS
    lda #$20
    sta PPUADDR
    lda tag_row_lo,x
    sta PPUADDR
    lda tag_row_start,x
    tay
    lda #TAG_WIDTH
    sta view_tmp
:
    lda tag_frame,y
    sta PPUDATA
    iny
    dec view_tmp
    bne :-
    inx
    cpx #4
    bne @tag_row

    bit PPUSTATUS               ; the status bar, rows 24 to 29
    lda #$23
    sta PPUADDR
    lda #$00
    sta PPUADDR
    ldx #0
:
    lda status_rows,x
    sta PPUDATA
    inx
    cpx #192
    bne :-
    bit PPUSTATUS               ; and the attributes, the map's and the bar's
    lda #$23
    sta PPUADDR
    lda #$C0
    sta PPUADDR
    ldx #0
:
    lda attr_shadow,x
    sta PPUDATA
    inx
    cpx #48
    bne :-
:
    lda map_attributes,x
    sta PPUDATA
    inx
    cpx #64
    bne :-

    jsr queue_port_line
    jsr queue_stat_line
    jsr vq_flush
    jsr queue_meter
    jsr queue_tag
    jsr vq_flush
    lda #0
    sta status_dirty
    sta tag_dirty
    lda #MAP_SCREEN_ROWS        ; all drawn
    sta render_row
    lda #WAVE_FRAMES
    sta wave_timer
    lda #0
    sta wave_phase

    jsr build_ship_oam
    lda #0
    sta OAMADDR
    lda #>oam_shadow
    sta OAMDMA
    lda #0
    sta PPUSCROLL
    sta PPUSCROLL
    lda #PPUCTRL_SPR_8X16
    ora ppu_bg
    sta PPUCTRL
    lda #SHOW_SCOPE
    sta PPUMASK
    rts
.endproc

.segment "RODATA"
tag_row_lo:
    .repeat 4, i
        .byte <((TAG_ROW + i) * 32 + TAG_COL)
    .endrepeat
tag_row_start:
    .repeat 4, i
        .byte i * TAG_WIDTH
    .endrepeat
.segment "CODE"

; The map's frame: the OAM, the waves, and one thing more of what has
; changed: the tag, the status bar, or a row of the map.
.proc commit_map_frame
    lda #0
    sta vq_len
    jsr signal_now
    cmp meter_shown
    beq :+
    lda #STAT_DIRTY_METER
    ora status_dirty
    sta status_dirty
:
    lda tag_dirty
    beq @status
    lda #0
    sta tag_dirty
    jsr queue_tag
    jmp @write
@status:
    lda status_dirty
    beq @map
    lsr a
    bcc :+
    lda #<~STAT_DIRTY_PORT
    and status_dirty
    sta status_dirty
    jsr queue_port_line
    jmp @write
:
    lsr a
    bcc :+
    lda #<~STAT_DIRTY_LINE
    and status_dirty
    sta status_dirty
    jsr queue_stat_line
    jmp @write
:
    lda #<~STAT_DIRTY_METER
    and status_dirty
    sta status_dirty
    jsr queue_meter
    jmp @write
@map:
    lda render_row
    cmp #MAP_SCREEN_ROWS
    bcs @write
    jsr render_map_row
    inc render_row
@write:
    jsr begin_vram_update
    lda #0
    sta OAMADDR
    lda #>oam_shadow
    sta OAMDMA
    jsr wave_tick
    jsr vq_flush
    jmp restore_scroll
.endproc

; ---------------------------------------------------------------------------
; The port card: NOW ENTERING, a picture of a harbor, the place, its ships
; and its range, while the first of its data comes.

.proc port_card_video
    lda #0
    sta PPUMASK
    sta ppu_bg
    sta vq_len
    lda #CHR_BANK_RADAR
    jsr select_chr_bank
    lda #$16                    ; a red hull
    jsr load_card_palette
    bit PPUSTATUS
    lda #$20
    sta PPUADDR
    lda #$00
    sta PPUADDR
    point_at view_ptr_a, port_card_nam
    jsr copy_nametable
    ; the place's name, in the middle of its row
    ldx #PORT_NAME_LEN
:
    lda place_name - 1,x
    bne :+
    dex
    bne :-
:
    stx view_tmp                ; its length, less spaces after it
    lda #32
    sec
    sbc view_tmp
    lsr a
    clc
    adc #<(PORT_NAME_ROW * 32)
    tax
    point_at field_src, place_name
    lda #FONT_DIM_BASE
    sta field_font
    ldy view_tmp
    beq :+
    lda #>($2000 + PORT_NAME_ROW * 32)
    jsr write_field
:
    jsr range_digits
    lda picker_tiles + ZIP_LEN
    sta line_buf
    lda picker_tiles + ZIP_LEN + 1
    sta line_buf + 1
    lda nm_label
    sta line_buf + 2
    lda nm_label + 1
    sta line_buf + 3
    point_at field_src, line_buf
    ldy #4
    lda #>PORT_RANGE_ADDR
    ldx #<PORT_RANGE_ADDR
    jsr write_field
    lda #$FF
    sta port_count_shown
    jsr queue_port_count
    jsr vq_flush
    jmp screen_on
.endproc

; The card palette, with BG1's hull colour A's.
.proc load_card_palette
    pha
    bit PPUSTATUS
    lda #$3F
    sta PPUADDR
    lda #$00
    sta PPUADDR
    ldx #0
:
    lda card_palette,x
    sta PPUDATA
    inx
    cpx #32
    bne :-
    bit PPUSTATUS
    lda #$3F
    sta PPUADDR
    lda #$05
    sta PPUADDR
    pla
    sta PPUDATA
    rts
.endproc

; view_ptr_a: 960 tiles and 64 attributes, to the PPU's address as it is.
.proc copy_nametable
    ldx #4
    ldy #0
:
    lda (view_ptr_a),y
    sta PPUDATA
    iny
    bne :-
    inc view_ptr_a + 1
    dex
    bne :-
    rts
.endproc

; The port card's SHIPS, if it has changed: the count, or - before a scene.
.proc queue_port_count
    lda radar_have
    beq @none
    lda displayed_count
    cmp port_count_shown
    beq @done
    sta port_count_shown
    ldx #0
    jsr two_digits
    lda line_buf
    cmp #CHAR_0
    bne :+
    lda line_buf + 1            ; no leading zero
    sta line_buf
    lda #CHAR_SPACE
    sta line_buf + 1
:
    jmp @queue
@none:
    lda port_count_shown
    cmp #$FE
    beq @done
    lda #$FE
    sta port_count_shown
    lda #CHAR_HYPHEN
    sta line_buf
    lda #CHAR_SPACE
    sta line_buf + 1
@queue:
    point_at field_src, line_buf
    lda #FONT_DIM_BASE
    sta field_font
    ldy #2
    lda #>PORT_SHIPS_ADDR
    ldx #<PORT_SHIPS_ADDR
    jmp vq_field
@done:
    rts
.endproc

; No sprites, no scroll, and the picture on: a card's screen.
.proc screen_on
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
    ora ppu_bg
    sta PPUCTRL
    lda #SHOW_SCOPE
    sta PPUMASK
    rts
.endproc

; The port card, until its data has come (a scene, and the map or none),
; PORT_CARD_MAX frames, or A or Start. Carry set: B, or no place after all,
; and back to the picker.
.proc port_card
    jsr build_place_name
    jsr port_card_video
    lda #0
    sta port_timer
    sta port_timer + 1
@frame:
    jsr latch_controller
    lda controller_pending
    sta controller_pressed
    lda #0
    sta controller_pending
    lda controller_pressed
    and #BUTTON_B
    bne @back
    lda controller_pressed
    and #(BUTTON_A | BUTTON_START)
    bne @go
    jsr age_link
    lda oam_rotation
    cmp #KEYS_TURN
    bne :+
    jsr radar_keys
    jmp :++
:
    jsr radar_poll
:
    lda startup_error
    bne @back
    inc oam_rotation
    lda oam_rotation
    and #7
    sta oam_rotation
    inc port_timer
    bne :+
    inc port_timer + 1
:
    lda port_timer + 1          ; long enough: the map, come what may
    cmp #>PORT_CARD_MAX
    bcc :+
    lda port_timer
    cmp #<PORT_CARD_MAX
    bcs @go
:
    lda radar_have              ; ships and a map, and shown a while
    beq @show
    lda map_have
    ora map_absent
    beq @show
    lda port_timer + 1
    bne @go
    lda port_timer
    cmp #PORT_CARD_MIN
    bcs @go
@show:
    lda #0
    sta vq_len
    jsr queue_port_count
    jsr begin_vram_update
    jsr vq_flush
    jsr restore_scroll
    jmp @frame
@go:
    clc
    rts
@back:
    sec
    rts
.endproc

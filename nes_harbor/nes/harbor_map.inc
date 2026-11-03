; NES Harbor's map: the land and water round the place, under the ships.
;
; The nesp32 makes it (HARBOR's MAP): the scope's 144 points across as 18 by
; 18 tiles, each tile four cells of land or water, a row of tiles to a
; reply. This asks for a row on the frames between SCENEs, and draws a row
; in the vblank that has the OAM in it, so a whole map takes 18 of each.
; A map is known by its serial: MAP for row 0 every MAP_PROBE_FRAMES asks
; whether there is a new one. A firmware with no MAP leaves the water.
;
; A held with the pad moves the place by a quarter of the range (PAN): the
; nesp32 draws its ships again round the new middle at once, and makes a
; new map. The old one is painted over with water first.
;
; radar_link.inc calls harbor_turn on the frames between SCENEs, harbor_pan
; from the pad, harbor_map_begin with radar_begin, and the scope commits
; through commit_oam_map.

.ifndef NESP_HARBOR_MAP
NESP_HARBOR_MAP = $04
.endif
.ifndef NESP_HARBOR_PAN
NESP_HARBOR_PAN = $05
.endif

MAP_ROWS = 18
MAP_COLS = 18
MAP_BYTES = MAP_COLS / 2        ; a tile to a nibble
MAP_ADDR = $2067                ; row 3, column 7: the scope's corner
MAP_TILE_BASE = $70             ; the sixteen tiles: bit 1 NW land, 2 NE, 4 SW, 8 SE
MAP_PROBE_FRAMES = 4            ; SCENE turns between asks for a new map
MAP_HEAD = 3                    ; STATE, ID, MAP_SERIAL
PAD_DIRECTIONS = BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT

.segment "BSS"
map_row:        .res 1          ; the next row to ask for; MAP_ROWS when none
map_row_ready:  .res 1          ; a row in map_row_tiles to draw, plus one
map_want:       .res 1          ; the serial of the map being fetched
map_have:       .res 1          ; and of the one fetched whole
map_clear:      .res 1          ; the next row to paint water; MAP_ROWS when none
map_probe_wait: .res 1
map_absent:     .res 1          ; the nesp32 has no MAP
pan_pending:    .res 1
pan_dx:         .res 1
pan_dy:         .res 1
map_row_tiles:  .res MAP_COLS

.segment "RODATA"
map_row_lo:
    .repeat MAP_ROWS, i
        .byte <(MAP_ADDR + i * 32)
    .endrepeat
map_row_hi:
    .repeat MAP_ROWS, i
        .byte >(MAP_ADDR + i * 32)
    .endrepeat

.segment "CODE"

; With radar_begin: what is on the scope is to be asked for again.
.proc harbor_map_begin
    lda #0
    sta map_row
    sta map_have
    sta map_want
    sta map_row_ready
    lda #MAP_PROBE_FRAMES
    sta map_probe_wait
    rts
.endproc

; The place has moved, or its range: water over the old map, then the new.
.proc map_repaint
    lda #0
    sta map_clear
    jmp harbor_map_begin
.endproc

; From the pad, with A held: controller_pressed's directions as a PAN.
.proc harbor_pan
    ldx #0
    lda controller_pressed
    and #BUTTON_RIGHT
    beq :+
    inx
:
    lda controller_pressed
    and #BUTTON_LEFT
    beq :+
    dex
:
    stx pan_dx
    ldx #0
    lda controller_pressed
    and #BUTTON_UP
    beq :+
    inx
:
    lda controller_pressed
    and #BUTTON_DOWN
    beq :+
    dex
:
    stx pan_dy
    lda #1
    sta pan_pending
    rts
.endproc

; A frame between SCENEs: a PAN asked for, or a row of the map.
.proc harbor_turn
    lda pan_pending
    beq @map
    lda #0
    sta pan_pending
    inc radar_id
    lda radar_id
    sta nesp_request
    lda pan_dx
    sta nesp_request + 1
    lda pan_dy
    sta nesp_request + 2
    lda #NESP_SVC_HARBOR
    ldx #NESP_HARBOR_PAN
    ldy #3
    jsr nesp_call
    bcs @done                   ; no answer: SCENE's exchanges say so
    cmp #NESP_ST_OK
    bne @done                   ; a firmware with no PAN
    jsr radar_begin             ; the ships round the new middle
    jmp map_repaint

@map:
    lda map_absent
    bne @done
    lda map_clear               ; the water first
    cmp #MAP_ROWS
    bcc @done
    lda map_row_ready           ; and the last row drawn
    bne @done
    lda map_row
    cmp #MAP_ROWS
    bcc @ask
    rts                         ; harbor_probe starts the next ask
@ask:
    lda radar_id
    sta nesp_request
    lda map_row
    sta nesp_request + 1
    lda #NESP_SVC_HARBOR
    ldx #NESP_HARBOR_MAP
    ldy #2
    jsr nesp_call
    bcs @done
    cmp #NESP_ST_OK
    beq @heard
    lda #1                      ; no MAP here: the water stays
    sta map_absent
@done:
    rts

@heard:
    lda REPLY_ID
    cmp radar_id
    bne @done                   ; about a place before this one
    lda nesp_reply + 2          ; MAP_SERIAL
    bne :+
    lda #MAP_ROWS               ; no map yet: ask again later
    sta map_row
    rts
:
    cmp map_want
    beq @same
    sta map_want                ; a new map: from its first row
    lda map_row
    beq @same
    lda #0
    sta map_row
    rts
@same:
    lda map_want
    cmp map_have
    bne @take
    lda #MAP_ROWS               ; the one already drawn
    sta map_row
    rts
@take:
    lda nesp_reply_len
    cmp #MAP_HEAD + MAP_BYTES
    bne @done
    ldx #0
    ldy #0
@code:
    lda nesp_reply + MAP_HEAD,y
    lsr a
    lsr a
    lsr a
    lsr a
    clc
    adc #MAP_TILE_BASE
    sta map_row_tiles,x
    inx
    lda nesp_reply + MAP_HEAD,y
    and #$0F
    clc
    adc #MAP_TILE_BASE
    sta map_row_tiles,x
    inx
    iny
    cpy #MAP_BYTES
    bne @code
    ldx map_row
    inx
    stx map_row_ready
    stx map_row
    cpx #MAP_ROWS
    bne @done
    lda map_want
    sta map_have
    rts
.endproc

; With each SCENE: now and then, whether there is a new map.
.proc harbor_probe
    lda map_row
    cmp #MAP_ROWS
    bcc @done                   ; one is being fetched
    dec map_probe_wait
    bne @done
    lda #MAP_PROBE_FRAMES
    sta map_probe_wait
    lda #0
    sta map_row
@done:
    rts
.endproc

; The scope's vblank with only the OAM in it, and a row of the map, or of
; water, if one is waiting.
.proc commit_oam_map
    lda map_clear
    cmp #MAP_ROWS
    bcc @clear
    lda map_row_ready
    bne @row
    jmp commit_oam
@clear:
    ldx #MAP_COLS - 1
    lda #MAP_TILE_BASE
:
    sta map_row_tiles,x
    dex
    bpl :-
    ldy map_clear
    inc map_clear
    jmp @write
@row:
    tay
    dey
    lda #0
    sta map_row_ready
@write:
    jsr begin_vram_update
    lda #0
    sta OAMADDR
    lda #>oam_shadow
    sta OAMDMA
    bit PPUSTATUS
    lda map_row_hi,y
    sta PPUADDR
    lda map_row_lo,y
    sta PPUADDR
    ldx #0
:
    lda map_row_tiles,x
    sta PPUDATA
    inx
    cpx #MAP_COLS
    bne :-
    jmp restore_scroll
.endproc

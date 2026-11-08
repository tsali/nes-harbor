; NES Harbor, from NES Radar Scope v2
;
; Starts with a controller-1 port picker, then asks a nesp32 on controller
; port 2 for the place and its ships (nes/radar_link.inc), and shows them
; on the overworld map (nes/harbor_view.inc).

.setcpu "6502"

PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
OAMADDR   = $2003
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
OAMDMA    = $4014
JOY1      = $4016
JOY2      = $4017
DMC_FREQ  = $4010
APUSTATUS = $4015

MAX_TARGETS       = 8
RECORD_SIZE       = 9
IDENTITY_RECORD_SIZE = 25
MAX_PAYLOAD       = MAX_TARGETS * RECORD_SIZE

SCENE_STALE       = $01
SCENE_SEVERE      = $1C
TRACK_INVALID     = $08
ALT_INVALID       = $10
SPEED_INVALID     = $20

PPUCTRL_SPR_8X16  = %00100000
PPUCTRL_UPDATE_NMI = %10100000
SHOW_SCOPE        = %00011110
SHOW_BACKGROUND   = %00001010
SHOW_STARTUP      = %00011110

CHAR_SPACE        = 0
CHAR_0            = 1
CHAR_A            = 11
CHAR_PLUS         = 37
CHAR_HYPHEN       = 38
CHAR_DOT          = 39
CHAR_SLASH        = 40
CHAR_COLON        = 41

STARTUP_CODE_ADDR   = $218E      ; row 12, column 14
STARTUP_CURSOR_ADDR = $21AE      ; row 13, column 14
STARTUP_MESSAGE_ADDR = $21E8     ; row 15, column 8
STARTUP_SPRITE_Y    = 12 * 8 - 1
STARTUP_SPRITE_X    = 14 * 8
STARTUP_SPRITE_PALETTE = 2

STATUS_COUNT = 0
STATUS_APT   = 2
STATUS_LINK  = 6
LINK_STATE_WIDTH = 9
STATUS_SIZE       = STATUS_LINK + LINK_STATE_WIDTH

BUTTON_A          = $80
BUTTON_B          = $40
BUTTON_SELECT     = $20
BUTTON_START      = $10
BUTTON_UP         = $08
BUTTON_DOWN       = $04
BUTTON_LEFT       = $02
BUTTON_RIGHT      = $01


LINK_STALE_FRAMES     = 600       ; 10 seconds without a valid packet
LINK_DOT_FRAMES       = 8         ; between one dot after LIVE and the next
LINK_STREAM_STEPS     = 4         ; one, two, three dots and LIVE again: about
                                  ; half a second for each new piece of data
SPLASH_FRAMES         = 120       ; about two seconds at NTSC field rate
CHR_BANK_SPLASH       = 0
CHR_BANK_RADAR        = 1

; Splash name and version stamp.  The splash bank is a bitmap of the LDV logo
; and has no font, so the glyphs are copied out of the LDV font into the tiles it
; leaves unused at the top of the bank (see the CHR0 segment).  Digits are
; contiguous, so VER_0 + n is the tile for digit n.  Tile $00 is blank in this
; bank, which makes it the space.
VER_GLYPH_BASE        = $E6
VER_GLYPH_COUNT       = 21        ; ten digits, '.', 'V', then N E S R A D H B O
VER_0                 = VER_GLYPH_BASE
VER_DOT               = VER_GLYPH_BASE + 10
VER_V                 = VER_GLYPH_BASE + 11
VER_N                 = VER_GLYPH_BASE + 12
VER_E                 = VER_GLYPH_BASE + 13
VER_S                 = VER_GLYPH_BASE + 14
VER_R                 = VER_GLYPH_BASE + 15
VER_A                 = VER_GLYPH_BASE + 16
VER_D                 = VER_GLYPH_BASE + 17
VER_H                 = VER_GLYPH_BASE + 18
VER_B                 = VER_GLYPH_BASE + 19
VER_O                 = VER_GLYPH_BASE + 20
VER_SPACE             = $00
SPLASH_TITLE_ADDR     = $232B     ; row 25, column 11
VERSION_STAMP_ADDR    = $236D     ; row 27, column 13 -- one blank row below

.include "ldv_screen.inc"
.include "harbor_text.inc"

.segment "ZEROPAGE"
packet_flags:   .res 1
packet_count:   .res 1
temp:           .res 1
record_flags:   .res 1
slot_number:    .res 1
records_left:   .res 1
seen_slots:     .res 1
panel_offset:   .res 1
value_lo:       .res 1
value_hi:       .res 1
digit_hundreds: .res 1
digit_tens:     .res 1
digit_ones:     .res 1
rec_offset:     .res 1
vblank_ready:   .res 1
controller_current: .res 1
controller_previous: .res 1
controller_pressed: .res 1
controller_pending: .res 1
editor_index:   .res 1
picker_port:    .res 1        ; the port in the list; PORT_COUNT for a zip code
picker_mode:    .res 1        ; 0 the list, 1 the zip code's digits
range_index:    .res 1        ; of harbor_ranges
place_ptr:      .res 2
startup_error:  .res 1
field_src:      .res 2
field_width:    .res 1
field_font:     .res 1
field_addr_hi:  .res 1
field_addr_lo:  .res 1
active_slots:   .res 1
selected_slot:  .res 1
previous_selected_slot: .res 1
oam_rotation:   .res 1
track_value:    .res 1
displayed_count: .res 1
link_age_lo:     .res 1
link_age_hi:     .res 1
link_timed_out:  .res 1
link_live:       .res 1           ; the LINK field says LIVE
link_stream:     .res 1           ; dot steps left: new scenes are coming
link_dot_timer:  .res 1
link_dots:       .res 1
link_dirty:      .res 1           ; the dots have changed and are not on screen
navigation_requested: .res 1

.segment "BSS"
.align 256
oam_shadow:     .res 256
scene_payload:  .res MAX_PAYLOAD
identity_tiles: .res 192
icao_code:      .res 4
status_fields:   .res STATUS_SIZE
startup_cursor:  .res ZIP_LEN
zip_code:        .res ZIP_LEN  ; ASCII digits
picker_tiles:    .res ZIP_LEN + 2
splash_frames:   .res 1

.segment "HEADER"
    .byte "NES", $1A
    .byte 2                       ; 32 KiB fixed PRG
.ifdef CART
    .byte 2                       ; the splash's tiles and the scope's, once each
    .byte $10, $00                ; a part of nesp32's cartridge, which is an MMC1
.else
    .byte 4                       ; four 8 KiB CHR-ROM banks
    .byte $30, $00                ; mapper 3 CN-ROM, horizontal mirroring
.endif
    .res 8, $00

.segment "CODE"

.proc reset
    sei
    cld
    ldx #$40
    stx JOY2
    ldx #$FF
    txs
    inx
    stx PPUCTRL
    stx PPUMASK
    stx DMC_FREQ
    stx APUSTATUS
    txa                           ; all of RAM from nothing: power-on RAM is
:                                 ; anything, and the keys typed are counted in it
    sta $0000,x                   ; (nothing has been pushed yet, so the stack
    sta $0100,x                   ; page goes too)
    sta $0200,x
    sta $0300,x
    sta $0400,x
    sta $0500,x
    sta $0600,x
    sta $0700,x
    inx
    bne :-

    stx radar_id                  ; no airport chosen, and no hello said
    stx radar_greeted
    stx radar_chosen
    lda #OUT0_MARK                ; the line to the nesp32 leaves break
    sta JOY1

    bit PPUSTATUS
@wait_vblank_1:
    bit PPUSTATUS
    bpl @wait_vblank_1
@wait_vblank_2:
    bit PPUSTATUS
    bpl @wait_vblank_2

    lda #0                        ; the first port, at HARBOR's own range
    sta picker_port
    lda #RANGE_DEFAULT
    sta range_index
    ldx #ZIP_LEN - 1
:
    lda zip_default,x
    sta zip_code,x
    dex
    bpl :-
    lda #0
    sta startup_error
    sta active_slots
    sta selected_slot
    sta previous_selected_slot
    sta oam_rotation
    sta controller_pending
    sta displayed_count
    sta link_age_lo
    sta link_age_hi
    sta link_timed_out
    sta navigation_requested
    lda #1
    jsr initialize_display_buffers
    jsr initialize_splash_video
    lda #1                        ; the music on, and a fanfare for the title
    sta music_on
    lda #TUNE_FANFARE
    jsr music_play
    jsr wait_splash
    jsr radar_recall              ; the picker begins at the place the nesp32 kept

radar_editor:                     ; nes/radar_link.inc comes back here
@show_editor:
    jsr initialize_startup_video
    lda #TUNE_SAILOR
    jsr music_play
    lda #0
    sta editor_index
    sta picker_mode
    sta navigation_requested
    jsr read_controller           ; a button still held from the scope, or from
    lda controller_current        ; Start, is not a press
    sta controller_previous
    jsr render_editor
    lda startup_error
    beq @editor_loop
    jsr radar_show_error          ; which of the reasons there was no scope

@editor_loop:
    inc radar_key_turn          ; every other frame, what is typed: before the
    lda radar_key_turn          ; vblank, which the editor's writes are for
    lsr a
    bcc :+
    jsr radar_editor_keys
:
    jsr wait_vblank
    jsr read_controller
    lda controller_previous
    eor #$FF
    and controller_current
    sta controller_pressed
    lda controller_current
    sta controller_previous
    jsr radar_editor_key        ; a key typed on the Bluetooth keyboard: a
    bcs :+                      ; letter or a digit
    jsr picker_typed
    jmp @editor_changed
:
    ora controller_pressed      ; or the buttons it stands for
    sta controller_pressed
.ifdef CART
    and #BUTTON_SELECT
    beq :+
    jmp to_cart_menu
:
.endif
    lda picker_mode
    bne @zip

    ; The list: UP and DOWN go through the ports, and the zip code after
    ; them; LEFT and RIGHT the range. Start or A: that port, or the zip
    ; code's digits.
    lda controller_pressed
    and #(BUTTON_START | BUTTON_A)
    beq @list_moves
    lda picker_port
    cmp #PORT_COUNT
    beq :+
    jmp @submit
:
    lda #1
    sta picker_mode
    lda #0
    sta editor_index
    jmp @editor_changed
@list_moves:
    lda controller_pressed
    and #BUTTON_B                 ; B: the music on or off
    beq :+
    jsr music_toggle
    jmp @editor_loop
:
    lda controller_pressed
    and #BUTTON_UP
    bne @port_up
    lda controller_pressed
    and #BUTTON_DOWN
    bne @port_down
    lda controller_pressed
    and #BUTTON_LEFT
    bne @range_down
    lda controller_pressed
    and #BUTTON_RIGHT
    bne @range_up
    jmp @editor_loop

@port_up:
    lda picker_port
    bne :+
    lda #PORT_COUNT + 1
:
    sec
    sbc #1
    sta picker_port
    jmp @editor_changed
@port_down:
    lda picker_port
    cmp #PORT_COUNT
    bcc :+
    lda #$FF
:
    clc
    adc #1
    sta picker_port
    jmp @editor_changed
@range_down:
    lda range_index
    beq :+
    dec range_index
:
    jmp @editor_changed
@range_up:
    lda range_index
    cmp #RANGE_COUNT - 1
    bcs :+
    inc range_index
:
    jmp @editor_changed

    ; The zip code: UP and DOWN the digit, LEFT and RIGHT which, Start or
    ; A to look it up, and B back to the list.
@zip:
    lda controller_pressed
    and #(BUTTON_START | BUTTON_A)
    beq :+
    jmp @submit
:
    lda controller_pressed
    and #BUTTON_B
    beq :+
    lda #0
    sta picker_mode
    jmp @editor_changed
:
    lda controller_pressed
    and #BUTTON_UP
    bne @digit_up
    lda controller_pressed
    and #BUTTON_DOWN
    bne @digit_down
    lda controller_pressed
    and #BUTTON_RIGHT
    bne @move_right
    lda controller_pressed
    and #BUTTON_LEFT
    bne @move_left
    jmp @editor_loop

@digit_up:
    ldx editor_index
    lda zip_code,x
    cmp #'9'
    bne :+
    lda #'0' - 1
:
    clc
    adc #1
    sta zip_code,x
    jmp @editor_changed

@digit_down:
    ldx editor_index
    lda zip_code,x
    cmp #'0'
    bne :+
    lda #'9' + 1
:
    sec
    sbc #1
    sta zip_code,x
    jmp @editor_changed

@move_right:
    jsr zip_cursor_right
    jmp @editor_changed

@move_left:
    lda editor_index
    bne :+
    lda #ZIP_LEN
    sta editor_index
:
    dec editor_index

@editor_changed:
    lda #0
    sta startup_error
    jsr render_editor
    jmp @editor_loop

@submit:
    jsr picker_tag                ; the scope's corner says where
    jmp radar_submit              ; the place and the scope, over the nesp32's link
.endproc

.proc zip_cursor_right
    inc editor_index
    lda editor_index
    cmp #ZIP_LEN
    bcc :+
    lda #0
    sta editor_index
:
    rts
.endproc

; A key typed in the editor, A its ASCII: a capital or a digit. A digit
; goes into the zip code where the cursor is, and from the list begins
; one. A letter in the list goes to the next port with that initial.
.proc picker_typed
    cmp #'A'
    bcs @letter
    ldx picker_mode
    bne @digit
    ldx #PORT_COUNT               ; from the list: a new zip code
    stx picker_port
    ldx #1
    stx picker_mode
    ldx #0
    stx editor_index
@digit:
    ldx editor_index
    sta zip_code,x
    jmp zip_cursor_right

@letter:
    ldx picker_mode
    bne @done
    sec
    sbc #'A' - CHAR_A             ; the letter's glyph, as the names hold it
    sta temp
    lda #PORT_COUNT
    sta value_hi                  ; ports left to look at
@next:
    inc picker_port
    lda picker_port
    cmp #PORT_COUNT
    bcc :+
    lda #0
    sta picker_port
:
    dec value_hi
    bmi @done                     ; round them all: none
    jsr port_name_pointer
    ldy #0
    lda (place_ptr),y
    cmp temp
    bne @next
@done:
    rts
.endproc

; place_ptr = the name of picker_port in the list, or the zip code's line.
.proc port_name_pointer
    lda picker_port
    cmp #PORT_COUNT
    bcc :+
    lda #<zip_entry_name
    sta place_ptr
    lda #>zip_entry_name
    sta place_ptr + 1
    rts
:
    tax
    lda #<port_names
    sta place_ptr
    lda #>port_names
    sta place_ptr + 1
    cpx #0
    beq @done
@add:
    lda place_ptr
    clc
    adc #PORT_NAME_LEN
    sta place_ptr
    bcc :+
    inc place_ptr + 1
:
    dex
    bne @add
@done:
    rts
.endproc

; place_ptr = picker_port's middle in port_places: eight bytes.
.proc port_place_pointer
    lda picker_port
    asl a
    asl a
    asl a
    clc
    adc #<port_places
    sta place_ptr
    lda #>port_places
    adc #0
    sta place_ptr + 1
    rts
.endproc
.assert PORT_COUNT * 8 <= 256, error, "a port's place is found by its index times eight"

; icao_code = what the scope's corner says: the port's tag, or ZIP.
.proc picker_tag
    lda picker_port
    cmp #PORT_COUNT
    bcc :+
    ldx #3
@zip:
    lda zip_tag,x
    sta icao_code,x
    dex
    bpl @zip
    rts
:
    asl a
    asl a
    tay
    ldx #0
@tag:
    lda port_tags,y
    sta icao_code,x
    iny
    inx
    cpx #4
    bne @tag
    rts
.endproc


; Check reserved target flags, coordinate bounds, and unique stable slots.
; Carry set reports malformed data without changing the displayed scene.
.proc validate_records
    lda #0
    sta seen_slots
    lda packet_count
    sta records_left
    ldy #0
@record:
    lda records_left
    beq @valid
    lda scene_payload,y
    bmi @invalid
    and #$07
    tax
    lda slot_masks,x
    and seen_slots
    bne @invalid
    lda slot_masks,x
    ora seen_slots
    sta seen_slots
    lda scene_payload + 1,y
    cmp #160
    bcs @invalid
    lda scene_payload + 2,y
    cmp #160
    bcs @invalid
    lda scene_payload + 7,y
    cmp #$80                        ; unavailable is valid
    beq @rate_valid
    cmp #100                        ; +0 through +99
    bcc @rate_valid
    cmp #$9D                        ; $9D through $FF = -99 through -1
    bcc @invalid
@rate_valid:
    lda scene_payload + 8,y
    cmp #$FF                        ; unavailable is valid
    beq @distance_valid
    cmp #100                        ; 0.0 through 9.9 n.m.
    bcs @invalid
@distance_valid:
    tya
    clc
    adc #RECORD_SIZE
    tay
    dec records_left
    bne @record
@valid:
    clc
    rts
@invalid:
    sec
    rts
.endproc

.proc validate_identity_char
    cmp #' '
    beq @valid
    cmp #'-'
    beq @valid
    cmp #'0'
    bcc @invalid
    cmp #':'
    bcc @valid
    cmp #'A'
    bcc @invalid
    cmp #'['
    bcs @invalid
@valid:
    clc
    rts
@invalid:
    sec
    rts
.endproc

.proc identity_char_tile
    cmp #' '
    bne :+
    lda #CHAR_SPACE
    rts
:
    cmp #'-'
    bne :+
    lda #CHAR_HYPHEN
    rts
:
    cmp #'A'
    bcs @letter
    sec
    sbc #'0'
    clc
    adc #CHAR_0
    rts
@letter:
    sec
    sbc #'A'
    clc
    adc #CHAR_A
    rts
.endproc

; Clear the ships' names and the status bar's fields.
.proc initialize_display_buffers
    ldx #0
    lda #CHAR_SPACE
@identity:
    sta identity_tiles,x
    inx
    cpx #192
    bne @identity
    ldx #0
@status:
    sta status_fields,x
    inx
    cpx #STATUS_SIZE
    bne @status
    jsr set_link_waiting
    lda #CHAR_SPACE
    sta status_fields + STATUS_COUNT
    lda #CHAR_0
    sta status_fields + STATUS_COUNT + 1
    rts
.endproc

; Poll once after an accepted scene. Down/Right selects the next occupied slot;
; Up/Left selects the previous; B is back a step, to the editor; and on
; nesp32's cartridge Select is its menu, as it is from the editor. B never
; leaves the program, so that B pressed again and again cannot.
.proc update_selection
    jsr latch_controller
    jsr card_watch              ; A tapped: the ship's card
    lda controller_pending
    sta controller_pressed
    lda #0
    sta controller_pending
    lda controller_pressed      ; SELECT: back a step, to the port editor
    and #BUTTON_SELECT          ; (on the cart, Select there is its menu)
    beq :+
    lda #1
    sta navigation_requested
    rts
:
    lda controller_current      ; B held: the pad moves the place (Rai 11/8)
    and #BUTTON_B
    beq @not_pan
    lda controller_pressed
    and #PAD_DIRECTIONS
    beq @not_pan
    jsr harbor_pan
    lda #0                      ; a pan is no tap
    sta card_armed
    jmp @ranged
@not_pan:
    lda controller_pressed      ; LEFT and RIGHT: the range, at once; UP and
    and #BUTTON_LEFT            ; DOWN alone choose the ship
    beq @right
    lda range_index
    beq @ranged
    dec range_index
    jsr scope_rerange
    jmp @ranged
@right:
    lda controller_pressed
    and #BUTTON_RIGHT
    beq @ranged
    lda range_index
    cmp #RANGE_COUNT - 1
    bcs @ranged
    inc range_index
    jsr scope_rerange
@ranged:
    lda #<~(BUTTON_LEFT | BUTTON_RIGHT)
    pha
    lda controller_current      ; UP and DOWN choose the ship, unless B is held
    and #BUTTON_B
    beq :+
    pla
    lda #<~PAD_DIRECTIONS
    pha
:
    pla
    and controller_pressed
    sta controller_pressed
    jmp move_selection
.endproc

; A new range on the map: the status bar's now, and the place again with
; it, which the nesp32 takes as the same place drawn again to the new range
; (a new id, no fetch).
.proc scope_rerange
    inc radar_id
    lda #0
    sta radar_chosen            ; radar_poll says the place again
    lda #STAT_DIRTY_LINE
    ora status_dirty
    sta status_dirty
    jmp map_repaint             ; and a map to the new range
.endproc

; The slot walk on its own, without the B and Select handling above.
.proc move_selection
    lda selected_slot
    sta previous_selected_slot
    lda active_slots
    beq @none
    ldx selected_slot
    lda slot_masks,x
    and active_slots
    bne @poll
    ldx #$FF
@first:
    inx
    lda slot_masks,x
    and active_slots
    beq @first
    stx selected_slot
@poll:
    lda controller_pressed
    and #(BUTTON_DOWN | BUTTON_RIGHT)
    bne @next
    lda controller_pressed
    and #(BUTTON_UP | BUTTON_LEFT)
    bne @previous
    rts
@next:
    ldx selected_slot
    ldy #8
@next_slot:
    inx
    txa
    and #7
    tax
    lda slot_masks,x
    and active_slots
    bne @choose
    dey
    bne @next_slot
    rts
@previous:
    ldx selected_slot
    ldy #8
@previous_slot:
    dex
    txa
    and #7
    tax
    lda slot_masks,x
    and active_slots
    bne @choose
    dey
    bne @previous_slot
    rts
@choose:
    stx selected_slot
    rts
@none:
    lda #0
    sta selected_slot
    rts
.endproc


.proc build_status_fields
    lda #CHAR_SPACE
    sta status_fields + STATUS_COUNT
    lda packet_count
    clc
    adc #CHAR_0
    sta status_fields + STATUS_COUNT + 1
    ; A scene was just taken from the nesp32: the link is LIVE.
    jmp set_link_live
.endproc

; Remove every ship while keeping their names, for a scene that cannot be
; shown.
.proc clear_target_state
    lda #0
    sta active_slots
    sta displayed_count
    sta packet_count
    sta selected_slot
    sta previous_selected_slot
    sta oam_rotation
    jsr ships_from_scene
    lda #CHAR_SPACE
    sta status_fields + STATUS_COUNT
    lda #CHAR_0
    sta status_fields + STATUS_COUNT + 1
    lda #1
    sta tag_dirty
    rts
.endproc

; A READY reply from the nesp32 proves the link is working. Whether its
; scene is fresh is the scene's flags' to say.
.proc note_valid_packet
    lda #0
    sta link_age_lo
    sta link_age_hi
    sta link_timed_out
    rts
.endproc

; Advance the no-valid-packet age by one video field. Carry is set exactly once
; when the ten-second threshold is crossed and the state becomes WAITING.
.proc age_link
    lda link_timed_out
    bne @unchanged
    inc link_age_lo
    bne :+
    inc link_age_hi
:
    lda link_age_hi
    cmp #>LINK_STALE_FRAMES
    bcc @unchanged
    bne @timeout
    lda link_age_lo
    cmp #<LINK_STALE_FRAMES
    bcc @unchanged
@timeout:
    lda #1
    sta link_timed_out
    jsr set_link_waiting
    sec
    rts
@unchanged:
    clc
    rts
.endproc

.proc set_link_waiting
    lda #<link_waiting_chars
    ldx #>link_waiting_chars
    jsr copy_link_state
    lda #SIGNAL_STALE
    sta signal_cap
    rts
.endproc

; A scene from the nesp32 is on the scope and it is good. The dots after it
; (link_stream) say that data is coming in.
.proc set_link_live
    lda #<link_live_chars
    ldx #>link_live_chars
    jsr copy_link_state
    lda #1
    sta link_live
    lda #SIGNAL_LIVE
    sta signal_cap
    rts
.endproc

.proc set_link_adsb_down
    lda #<link_adsb_down_chars
    ldx #>link_adsb_down_chars
    jmp copy_link_state
.endproc

; A:X points to a fixed-width LINK state in ROM. Every state but LIVE stops
; the dots.
.proc copy_link_state
    sta field_src
    stx field_src + 1
    ldy #0
    sty link_live
    sty link_stream
    sty signal_cap              ; no signal, unless the caller says
@copy:
    lda (field_src),y
    sta status_fields + STATUS_LINK,y
    iny
    cpy #LINK_STATE_WIDTH
    bne @copy
    rts
.endproc

; Convert value_hi:value_lo to three decimal digits, clipping at 999.
.proc convert_u16_3digits
    lda value_hi
    cmp #3
    bcc @convert
    bne @clip
    lda value_lo
    cmp #$E8                       ; $03E8 = 1000
    bcs @clip
@convert:
    lda #0
    sta digit_hundreds
    sta digit_tens
@hundreds:
    lda value_hi
    bne @subtract_100
    lda value_lo
    cmp #100
    bcc @tens
@subtract_100:
    lda value_lo
    sec
    sbc #100
    sta value_lo
    lda value_hi
    sbc #0
    sta value_hi
    inc digit_hundreds
    jmp @hundreds
@tens:
    lda value_lo
    cmp #10
    bcc @ones
    sec
    sbc #10
    sta value_lo
    inc digit_tens
    jmp @tens
@ones:
    lda value_lo
    sta digit_ones
    rts
@clip:
    lda #9
    sta digit_hundreds
    sta digit_tens
    sta digit_ones
    rts
.endproc

; Read a standard controller from port 1. The final bit layout is convenient
; for edge handling: A,B,Select,Start,Up,Down,Left,Right become bits 7..0.
.proc read_controller
    lda #0                        ; OUT0 rests at mark, which holds the pad's
    sta JOY1                      ; latch open: dropping it takes the buttons
    sta controller_current
    ldx #8
@button:
    lda JOY1
    and #1
    lsr a
    rol controller_current
    dex
    bne @button
    lda #OUT0_MARK
    sta JOY1
    rts
.endproc

.proc latch_controller
    jsr read_controller
    jsr nesp_key_next           ; a key typed that stands for buttons
    ora controller_pending
    sta controller_pending
    lda controller_previous
    eor #$FF
    and controller_current
    ora controller_pending
    sta controller_pending
    lda controller_current
    sta controller_previous
    rts
.endproc

; The picker, in three vblanks: the port's name, the range and the sprites
; (none); the zip code, its cursor and the message line; the help for the
; list or for the zip code.
.proc render_editor
    jsr port_name_pointer
    lda place_ptr
    sta field_src
    lda place_ptr + 1
    sta field_src + 1

    jsr range_digits

    ldx #ZIP_LEN - 1              ; the zip code's digits, or nothing
@zip:
    lda zip_code,x
    sec
    sbc #'0' - CHAR_0
    ldy picker_port
    cpy #PORT_COUNT
    beq :+
    lda #CHAR_SPACE
:
    sta picker_tiles,x
    lda #CHAR_SPACE
    sta startup_cursor,x
    dex
    bpl @zip
    lda picker_mode
    beq :+
    ldx editor_index
    lda #CHAR_HYPHEN
    sta startup_cursor,x
:

    jsr begin_vram_update
    lda #FONT_DIM_BASE            ; the startup palette's bright green
    sta field_font
    ldy #PORT_NAME_LEN
    lda #>PICKER_NAME_ADDR
    ldx #<PICKER_NAME_ADDR
    jsr write_field
    lda #<(picker_tiles + ZIP_LEN)
    sta field_src
    lda #>(picker_tiles + ZIP_LEN)
    sta field_src + 1
    ldy #2
    lda #>PICKER_RANGE_ADDR
    ldx #<PICKER_RANGE_ADDR
    jsr write_field
    lda #0
    sta OAMADDR
    lda #>oam_shadow
    sta OAMDMA
    jsr restore_scroll

    jsr begin_vram_update
    lda #<picker_tiles
    sta field_src
    lda #>picker_tiles
    sta field_src + 1
    lda #FONT_DIM_BASE            ; the digits bright while they are being set
    ldx picker_mode
    bne :+
    lda #FONT_BASE
:
    sta field_font
    ldy #ZIP_LEN
    lda #>STARTUP_CODE_ADDR
    ldx #<STARTUP_CODE_ADDR
    jsr write_field
    lda #<startup_cursor
    sta field_src
    lda #>startup_cursor
    sta field_src + 1
    lda #FONT_DIM_BASE
    sta field_font
    ldy #ZIP_LEN
    lda #>STARTUP_CURSOR_ADDR
    ldx #<STARTUP_CURSOR_ADDR
    jsr write_field
    lda #<startup_blank_16
    sta field_src
    lda #>startup_blank_16
    sta field_src + 1
    ldy #16
    lda #>STARTUP_MESSAGE_ADDR
    ldx #<STARTUP_MESSAGE_ADDR
    jsr write_field
    jsr restore_scroll

    lda #<help_list               ; the help: the list's, or the zip code's
    ldx #>help_list
    ldy picker_mode
    beq :+
    lda #<help_zip
    ldx #>help_zip
:
    sta field_src
    stx field_src + 1
    jsr begin_vram_update
    lda #FONT_DIM_BASE
    sta field_font
    ldy #16
    lda #>PICKER_HELP_ADDR
    ldx #<PICKER_HELP_ADDR
    jsr write_field
    jsr help_next
    ldy #16
    lda #>(PICKER_HELP_ADDR + 32)
    ldx #<(PICKER_HELP_ADDR + 32)
    jsr write_field
    jsr help_next
    ldy #16
    lda #>(PICKER_HELP_ADDR + 64)
    ldx #<(PICKER_HELP_ADDR + 64)
    jsr write_field
    jmp restore_scroll
.endproc

; picker_tiles + ZIP_LEN: the range's two digits, with no leading zero.
.proc range_digits
    ldx range_index               ; the range's two digits
    lda harbor_ranges,x
    ldx #CHAR_0
:
    cmp #10
    bcc :+
    sbc #10                       ; carry is set
    inx
    bne :-
:
    clc
    adc #CHAR_0
    sta picker_tiles + ZIP_LEN + 1
    cpx #CHAR_0
    bne :+
    ldx #CHAR_SPACE               ; no leading zero
:
    stx picker_tiles + ZIP_LEN
    rts
.endproc

.proc help_next
    lda field_src
    clc
    adc #16
    sta field_src
    bcc :+
    inc field_src + 1
:
    rts
.endproc

; A=0 shows REQUESTING; A!=0 shows INVALID AIRPORT.
.proc show_startup_message
    sta temp
    jsr begin_vram_update
    lda temp
    bne @invalid
    lda #<startup_requesting_chars
    ldx #>startup_requesting_chars
    bne @source_ready
@invalid:
    lda #<startup_invalid_chars
    ldx #>startup_invalid_chars
@source_ready:
    sta field_src
    stx field_src + 1
    lda #FONT_DIM_BASE
    sta field_font
    ldy #16
    lda #>STARTUP_MESSAGE_ADDR
    ldx #<STARTUP_MESSAGE_ADDR
    jsr write_field
    jsr restore_scroll
    rts
.endproc



.proc initialize_startup_video
    lda #0
    sta PPUMASK
    sta ppu_bg
    lda #CHR_BANK_RADAR
    jsr select_chr_bank
    jsr load_ldv_palette

    bit PPUSTATUS
    lda #$20
    sta PPUADDR
    lda #$00
    sta PPUADDR
    ldx #0
    lda #CHAR_SPACE
@clear_0:
    sta PPUDATA
    inx
    bne @clear_0
    ldx #0
@clear_1:
    sta PPUDATA
    inx
    bne @clear_1
    ldx #0
@clear_2:
    sta PPUDATA
    inx
    bne @clear_2
    ldx #0
@clear_3:
    sta PPUDATA
    inx
    bne @clear_3
    jsr write_startup_static

    ldx #0
    lda #$FF
@clear_oam:
    sta oam_shadow,x
    inx
    bne @clear_oam
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

.proc load_ldv_palette
    bit PPUSTATUS
    lda #$3F
    sta PPUADDR
    lda #$00
    sta PPUADDR
    ldx #0
@palette:
    lda ldv_palette,x
    sta PPUDATA
    inx
    cpx #32
    bne @palette
    rts
.endproc

.proc write_startup_static
    lda #0                        ; the words are tiles already
    sta field_font
    lda #<picker_title
    ldx #>picker_title
    ldy #10
    jsr static_at
    .addr PICKER_TITLE_ADDR
    lda #<picker_select
    ldx #>picker_select
    ldy #11
    jsr static_at
    .addr PICKER_SELECT_ADDR
    lda #<picker_range_label
    ldx #>picker_range_label
    ldy #5
    jsr static_at
    .addr PICKER_RANGE_LABEL
    lda #<picker_range_unit
    ldx #>picker_range_unit
    ldy #2
    jsr static_at
    .addr PICKER_RANGE_UNIT
.ifdef CART
    lda #<picker_menu
    ldx #>picker_menu
    ldy #11
    jsr static_at
    .addr PICKER_MENU_ADDR
.endif
    lda #<picker_source
    ldx #>picker_source
    ldy #21
    jsr static_at
    .addr PICKER_SOURCE_ADDR
    lda #<picker_credits
    ldx #>picker_credits
    ldy #24
    jsr static_at
    .addr PICKER_CREDIT_ADDR
    lda #<(picker_credits + 24)
    ldx #>(picker_credits + 24)
    ldy #24
    jsr static_at
    .addr PICKER_CREDIT_ADDR + 32
    lda #<(picker_credits + 48)
    ldx #>(picker_credits + 48)
    ldy #24
    jsr static_at
    .addr PICKER_CREDIT_ADDR + 64
    lda #<(picker_credits + 72)
    ldx #>(picker_credits + 72)
    ldy #24
    jsr static_at
    .addr PICKER_CREDIT_ADDR + 96
    rts
.endproc

; write_field with the source in A:X, the width in Y, and the nametable
; address in the two bytes after the jsr, which it returns past.
.proc static_at
    sta field_src
    stx field_src + 1
    sty field_width
    tsx
    lda $0101,x                   ; the jsr's return address, less one
    sta place_ptr
    lda $0102,x
    sta place_ptr + 1
    lda $0101,x                   ; and it returns past the address
    clc
    adc #2
    sta $0101,x
    bcc :+
    inc $0102,x
:
    ldy #1
    lda (place_ptr),y             ; the address's low byte
    tax
    iny
    lda (place_ptr),y
    ldy field_width
    jmp write_field
.endproc

; The title's sea, every eight frames: its wave colours trade places
; (palette 1's second and third), just after a vblank has begun.
.proc shimmer_splash_sea
    ; Gentle 4-step colour cycle, one step every 32 frames (~0.53 s):
    ; dark/mid/light/mid blue so the sea rolls instead of strobing.
    lda splash_frames
    and #31
    bne @done
    lda splash_frames
    lsr
    lsr
    lsr
    lsr
    lsr
    and #3
    tax
    ldy shimmer_b,x
    lda shimmer_a,x
    tax
    bit PPUSTATUS
    lda #$3F
    sta PPUADDR
    lda #$06
    sta PPUADDR
    stx PPUDATA
    sty PPUDATA
    lda #0
    sta PPUADDR
    sta PPUADDR
    sta PPUSCROLL
    sta PPUSCROLL
@done:
    rts
.endproc
shimmer_a: .byte $01, $11, $21, $11
shimmer_b: .byte $21, $11, $01, $11


; A:X = PPU address high:low, field_src = source character indices,
; Y = fixed width, field_font = font base. Every call writes exactly Y tiles.
.proc write_field
    sta field_addr_hi
    stx field_addr_lo
    sty field_width
    bit PPUSTATUS
    lda field_addr_hi
    sta PPUADDR
    lda field_addr_lo
    sta PPUADDR
    ldy #0
@tile:
    lda (field_src),y
    clc
    adc field_font
    sta PPUDATA
    iny
    cpy field_width
    bne @tile
    rts
.endproc

; A link state, A:X as copy_link_state's, on the status bar.
.proc show_link
    jsr copy_link_state
    lda #STAT_DIRTY_LINE | STAT_DIRTY_METER
    ora status_dirty
    sta status_dirty
    rts
.endproc

; One frame of the dots after LIVE: one, two, three, a step every
; LINK_DOT_FRAMES, for as long as link_stream has steps left. When it runs
; out, LIVE stands alone. Carry set if the field has changed.
.proc tick_link_dots
    lda link_stream
    beq @done
    dec link_dot_timer
    bne @done
    lda #LINK_DOT_FRAMES
    sta link_dot_timer
    dec link_stream
    beq @plain
    inc link_dots
    lda link_dots
    cmp #4
    bcc :+
    lda #1
    sta link_dots
:
    ldx #0
@dot:
    ldy #CHAR_SPACE
    cpx link_dots
    bcs :+
    ldy #CHAR_DOT
:
    tya
    sta status_fields + STATUS_LINK + 4,x
    inx
    cpx #3
    bne @dot
    sec
    rts
@plain:
    lda #CHAR_SPACE
    sta status_fields + STATUS_LINK + 4
    sta status_fields + STATUS_LINK + 5
    sta status_fields + STATUS_LINK + 6
    sec
    rts
@done:
    clc
    rts
.endproc

; Data is coming from the nesp32, a scene or its names: the dots, if LIVE,
; from one again.
.proc note_link_stream
    lda link_live
    beq :+
    lda #0
    sta link_dots
    lda #LINK_STREAM_STEPS
    sta link_stream
    lda #1                      ; and a step at once
    sta link_dot_timer
:
    rts
.endproc

; Arm NMI only long enough to acquire an exact vblank boundary. Rendering is
; deliberately left enabled: forced blank skips the pre-render scroll reload
; and was the diagnosed cause of the earlier one-tile vertical displacement.
.proc begin_vram_update
    ; The link runs with NMI disabled. Between exchanges, arm NMI only long
    ; enough to get an exact vblank boundary.
    lda #0
    sta vblank_ready
    bit PPUSTATUS
    lda #PPUCTRL_UPDATE_NMI
    ora ppu_bg                    ; the frame's background table, throughout
    sta PPUCTRL
@wait:
    lda vblank_ready
    beq @wait
    lda #PPUCTRL_SPR_8X16
    ora ppu_bg
    sta PPUCTRL
    bit PPUSTATUS
    rts
.endproc

.proc restore_scroll
    ; PPUADDR and PPUSCROLL share the write toggle. Reset it explicitly before
    ; restoring scroll so a marginal/late address write cannot pair with the
    ; first scroll byte and leave the nametable displaced.
    bit PPUSTATUS
    lda #0
    sta PPUSCROLL
    sta PPUSCROLL
    lda #PPUCTRL_SPR_8X16
    ora ppu_bg
    sta PPUCTRL
    jmp music_tick              ; a vblank's writes are done: a frame of music
.endproc

.proc wait_vblank
@wait_end:
    bit PPUSTATUS
    bmi @wait_end
@wait_start:
    bit PPUSTATUS
    bpl @wait_start
    jmp music_tick
.endproc

.ifdef CART
; Select, in the editor or on the scope, on nesp32's cartridge: back to its
; menu, the way
; reset goes there. The cartridge points this program's reset vector at
; the menu's stub, which resets the mapper and starts the menu. With the
; picture and the NMI off first, as a reset would leave them.
.proc to_cart_menu
    sei
    lda #0
    sta PPUCTRL
    sta PPUMASK
    jmp ($FFFC)
.endproc
.endif

.proc nmi
    pha
    lda #1
    sta vblank_ready
    pla
    rti
.endproc

.proc irq
    rti
.endproc

; CN-ROM's discrete latch can have PRG bus conflicts.  BANKDATA begins at
; $8000 with bytes 0,1,2,3, so A and the ROM byte always agree on the write.
.proc select_chr_bank
.ifdef CART
    jmp radar_cart_chr            ; the cartridge's mapper is asked another way
.else
    tax
    sta chr_bank_values,x
    rts
.endif
.endproc

.proc initialize_splash_video
    lda #0
    sta PPUMASK
    sta PPUCTRL
    lda #CHR_BANK_SPLASH
    jsr select_chr_bank

    bit PPUSTATUS
    lda #$3F
    sta PPUADDR
    lda #$00
    sta PPUADDR
    ldx #0
@palette:
    lda splash_bg_palette,x
    sta PPUDATA
    inx
    cpx #16
    bne @palette

    bit PPUSTATUS
    lda #$20
    sta PPUADDR
    lda #$00
    sta PPUADDR
    ldx #0
@nametable_0:
    lda splash_bg_nam,x
    sta PPUDATA
    inx
    bne @nametable_0
    ldx #0
@nametable_1:
    lda splash_bg_nam + 256,x
    sta PPUDATA
    inx
    bne @nametable_1
    ldx #0
@nametable_2:
    lda splash_bg_nam + 512,x
    sta PPUDATA
    inx
    bne @nametable_2
    ldx #0
@nametable_3:
    lda splash_bg_nam + 768,x
    sta PPUDATA
    inx
    bne @nametable_3

    jsr draw_version_stamp

    ldx #0
    lda #$FF
@clear_oam:
    sta oam_shadow,x
    inx
    bne @clear_oam
    lda #0
    sta OAMADDR
    lda #>oam_shadow
    sta OAMDMA
    lda #0
    sta PPUSCROLL
    sta PPUSCROLL
    sta PPUCTRL
    lda #SHOW_BACKGROUND
    sta PPUMASK
    rts
.endproc

; Writes the product name and build version into the blank strip below the logo,
; so a ROM that turns up in the wild says what it is and which build it is.  The
; splash nametable itself is left alone; the text is painted over it after the
; upload.  Rendering is still off at this point, so this does not have to fit in
; vblank.
.proc draw_version_stamp
    bit PPUSTATUS
    lda #>SPLASH_TITLE_ADDR
    sta PPUADDR
    lda #<SPLASH_TITLE_ADDR
    sta PPUADDR
    ldx #0
@title:
    lda splash_title,x
    sta PPUDATA
    inx
    cpx #SPLASH_TITLE_LENGTH
    bne @title

    bit PPUSTATUS
    lda #>VERSION_STAMP_ADDR
    sta PPUADDR
    lda #<VERSION_STAMP_ADDR
    sta PPUADDR
    ldx #0
@glyph:
    lda version_stamp,x
    sta PPUDATA
    inx
    cpx #VERSION_STAMP_LENGTH
    bne @glyph
    rts
.endproc

; The splash for SPLASH_FRAMES, or until Start. On nesp32's cartridge it is
; not skipped: the Start that chose NES Radar from the menu is still held.
.proc wait_splash
    ; Title stays up until START is pressed (no timeout), sea animates slowly.
    lda #0
    sta splash_frames
@frame:
    jsr wait_vblank
    inc splash_frames
    jsr shimmer_splash_sea
    jsr read_controller
    lda controller_current
    and #BUTTON_START
    beq @frame
@release:
    jsr wait_vblank
    inc splash_frames
    jsr shimmer_splash_sea
    jsr read_controller
    lda controller_current
    and #BUTTON_START
    bne @release
    rts
.endproc

.include "harbor_data.inc"
.include "radar_link.inc"
.include "harbor_view.inc"
.include "harbor_map.inc"
.include "harbor_card.inc"
.include "music.inc"

.segment "BANKDATA"
chr_bank_values:
    .byte $00, $01, $02, $03

.segment "SPLASHDATA"
splash_bg_palette:
    .incbin "assets/splash/splash_bg.palette.bin"
splash_bg_nam:
    .incbin "assets/splash/splash_bg.nam"

; "NES HARBOR"
splash_title:
    .byte VER_N, VER_E, VER_S, VER_SPACE, VER_H, VER_A, VER_R, VER_B, VER_O, VER_R
SPLASH_TITLE_LENGTH = * - splash_title

; "V0.1.0" -- NES Harbor's release version. Bump this and README.md together. The
; splash stamp is the only way to tell what is on a cartridge or in a .nes
; file once it is out of context.
version_stamp:
    .byte VER_V, VER_0 + 0, VER_DOT, VER_0 + 1, VER_DOT, VER_0 + 0
VERSION_STAMP_LENGTH = * - version_stamp

.segment "RODATA"
slot_masks:
    .byte $01, $02, $04, $08, $10, $20, $40, $80

link_waiting_chars:
    .byte CHAR_A + ('W' - 'A'), CHAR_A + ('A' - 'A'), CHAR_A + ('I' - 'A'), CHAR_A + ('T' - 'A'), CHAR_A + ('I' - 'A'), CHAR_A + ('N' - 'A'), CHAR_A + ('G' - 'A'), CHAR_SPACE, CHAR_SPACE
link_live_chars:
    .byte CHAR_A + ('L' - 'A'), CHAR_A + ('I' - 'A'), CHAR_A + ('V' - 'A'), CHAR_A + ('E' - 'A'), CHAR_SPACE, CHAR_SPACE, CHAR_SPACE, CHAR_SPACE, CHAR_SPACE
link_no_answer_chars:
    .byte CHAR_A + ('N' - 'A'), CHAR_A + ('O' - 'A'), CHAR_SPACE, CHAR_A + ('A' - 'A'), CHAR_A + ('N' - 'A'), CHAR_A + ('S' - 'A'), CHAR_A + ('W' - 'A'), CHAR_A + ('E' - 'A'), CHAR_A + ('R' - 'A')
link_bad_data_chars:
    .byte CHAR_A + ('B' - 'A'), CHAR_A + ('A' - 'A'), CHAR_A + ('D' - 'A'), CHAR_SPACE, CHAR_A + ('D' - 'A'), CHAR_A + ('A' - 'A'), CHAR_A + ('T' - 'A'), CHAR_A + ('A' - 'A'), CHAR_SPACE
link_adsb_down_chars:
    tiles 0, " AIS DOWN"

blank_identity_row:
    .repeat 12
        .byte CHAR_SPACE
    .endrepeat

startup_requesting_chars:
    .byte CHAR_SPACE,CHAR_SPACE,CHAR_SPACE
    .byte CHAR_A+('R'-'A'),CHAR_A+('E'-'A'),CHAR_A+('Q'-'A'),CHAR_A+('U'-'A'),CHAR_A+('E'-'A')
    .byte CHAR_A+('S'-'A'),CHAR_A+('T'-'A'),CHAR_A+('I'-'A'),CHAR_A+('N'-'A'),CHAR_A+('G'-'A')
    .byte CHAR_SPACE,CHAR_SPACE,CHAR_SPACE
startup_invalid_chars:
    tiles 0, "PLACE NOT FOUND "
startup_blank_16:
    .res 16, CHAR_SPACE

.segment "VECTORS"
    .addr nmi, reset, irq

.segment "CHR0"
    ; splash_bg.chr leaves tiles $E6-$FF blank; the stamp glyphs go there
    ; instead, lifted from the LDV font so the stamp matches the text the rest of
    ; the ROM draws.  $E6-$EF are digits 0-9, $F0 '.', $F1 'V', $F2-$F7 N E S R A D.
    .incbin "assets/splash/splash_bg.chr", 0, VER_GLYPH_BASE * 16
    .incbin "assets/ldv/ldv_bg.chr", CHAR_0 * 16, 10 * 16
    .incbin "assets/ldv/ldv_bg.chr", CHAR_DOT * 16, 16
    .incbin "assets/ldv/ldv_bg.chr", (CHAR_A + 'V' - 'A') * 16, 16
    .incbin "assets/ldv/ldv_bg.chr", (CHAR_A + 'N' - 'A') * 16, 16
    .incbin "assets/ldv/ldv_bg.chr", (CHAR_A + 'E' - 'A') * 16, 16
    .incbin "assets/ldv/ldv_bg.chr", (CHAR_A + 'S' - 'A') * 16, 16
    .incbin "assets/ldv/ldv_bg.chr", (CHAR_A + 'R' - 'A') * 16, 16
    .incbin "assets/ldv/ldv_bg.chr", (CHAR_A + 'A' - 'A') * 16, 16
    .incbin "assets/ldv/ldv_bg.chr", (CHAR_A + 'D' - 'A') * 16, 16
    .incbin "assets/ldv/ldv_bg.chr", (CHAR_A + 'H' - 'A') * 16, 16
    .incbin "assets/ldv/ldv_bg.chr", (CHAR_A + 'B' - 'A') * 16, 16
    .incbin "assets/ldv/ldv_bg.chr", (CHAR_A + 'O' - 'A') * 16, 16
    .res $1000 - (VER_GLYPH_BASE + VER_GLYPH_COUNT) * 16, $00
    .res $1000, $00

.segment "CHR1"                  ; tools/make_overworld.py's
    .incbin "assets/overworld/harbor_a.chr"
    .incbin "assets/overworld/harbor_b.chr"

.ifndef CART                      ; the cartridge has these tiles once
.segment "CHR2"
    .incbin "assets/overworld/harbor_a.chr"
    .incbin "assets/overworld/harbor_b.chr"

.segment "CHR3"
    .incbin "assets/overworld/harbor_a.chr"
    .incbin "assets/overworld/harbor_b.chr"
.endif

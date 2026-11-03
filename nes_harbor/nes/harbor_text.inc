; NES Harbor's words for the picker, as tiles.
;
; tiles base, "TEXT": capitals, digits, space and + - . / : from the scope's
; font at base (FONT_BASE or FONT_DIM_BASE), and @ (STARTUP_CHAR_AT) as it
; is. nes-radar's small letters are gone: three of their tiles are the
; scope's title letters H, B and O (tools/relabel_scope.py). A character
; there is no tile for stops the build.

.macro tiles base, text
  .repeat .strlen(text), i
    .if .strat(text, i) = ' '
      .byte base + CHAR_SPACE
    .elseif .strat(text, i) >= '0' .and .strat(text, i) <= '9'
      .byte base + CHAR_0 + .strat(text, i) - '0'
    .elseif .strat(text, i) >= 'A' .and .strat(text, i) <= 'Z'
      .byte base + CHAR_A + .strat(text, i) - 'A'
    .elseif .strat(text, i) = '+'
      .byte base + CHAR_PLUS
    .elseif .strat(text, i) = '-'
      .byte base + CHAR_HYPHEN
    .elseif .strat(text, i) = '.'
      .byte base + CHAR_DOT
    .elseif .strat(text, i) = '/'
      .byte base + CHAR_SLASH
    .elseif .strat(text, i) = ':'
      .byte base + CHAR_COLON
    .elseif .strat(text, i) = '@'
      .byte STARTUP_CHAR_AT
    .else
      .error "a character the startup screen has no tile for"
    .endif
  .endrepeat
.endmacro

; Where the picker draws. The zip code's digits and cursor are at
; STARTUP_CODE_ADDR and STARTUP_CURSOR_ADDR, rows 12 and 13, and the message
; at STARTUP_MESSAGE_ADDR, row 15, as nes-radar's ICAO code and its were.
PICKER_TITLE_ADDR  = $208B      ; row 4, column 11: NES HARBOR
PICKER_SELECT_ADDR = $20EA      ; row 7, column 10: SELECT PORT
PICKER_NAME_ADDR   = $2126      ; row 9, column 6: the port, twenty wide
PICKER_RANGE_LABEL = $216B      ; row 11, column 11: RANGE
PICKER_RANGE_ADDR  = $2171      ; row 11, column 17: its two digits
PICKER_RANGE_UNIT  = $2174      ; row 11, column 20: NM
PICKER_HELP_ADDR   = $2228      ; rows 17 to 19, column 8: sixteen wide
PICKER_MENU_ADDR   = $228A      ; row 20, column 10: SELECT MENU
PICKER_SOURCE_ADDR = $22C5      ; row 22, column 5
PICKER_CREDIT_ADDR = $2304      ; rows 24 to 27, column 4: 24 wide
SCOPE_RANGE_ADDR   = $2365      ; the scope's row 27, column 5: nes-radar's 09 of 09NM

ZIP_LEN     = 5
RANGE_COUNT = 6
RANGE_DEFAULT = 3               ; 20 nm, HARBOR's own


; NES Harbor's picker: its words, ranges and ports (nes/ports.inc).

.segment "RODATA"
harbor_ranges:
    .byte 3, 5, 10, 20, 40, 60
.assert * - harbor_ranges = RANGE_COUNT, error, "a range for each place in the list"

zip_entry_name:
    tiles 0, "ENTER A ZIP CODE    "
zip_tag:
    .byte " ZIP"
zip_default:
    .byte "32502"               ; downtown Pensacola

help_list:
    tiles 0, "UP DOWN   PORT  "
    tiles 0, "LEFT RIGHT RANGE"
    tiles 0, "START   CONFIRM "
help_zip:
    tiles 0, "UP DOWN   DIGIT "
    tiles 0, "LEFT RIGHT  MOVE"
    tiles 0, "START  OK B BACK"

picker_title:
    tiles FONT_DIM_BASE, "NES HARBOR"
picker_select:
    tiles FONT_DIM_BASE, "SELECT PORT"
picker_range_label:
    tiles FONT_DIM_BASE, "RANGE"
picker_range_unit:
    tiles FONT_DIM_BASE, "NM"
.ifdef CART
picker_menu:
    tiles FONT_DIM_BASE, "SELECT MENU"
.endif
picker_source:
    tiles FONT_DIM_BASE, "SHIP DATA: AISHUB.NET"
picker_credits:
    tiles FONT_DIM_BASE, "  BSKY @CULTOFJAMES.ORG "
    tiles FONT_DIM_BASE, "    X @PANDAVIERANASH   "
    tiles FONT_DIM_BASE, "      GITHUB TSALI      "
    tiles FONT_DIM_BASE, " SCOPE BY LEVIMAAIA.COM "
.assert * - picker_credits = 4 * 24, error, "four lines of credits, 24 wide"

.include "ports.inc"
.segment "CODE"

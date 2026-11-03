# NES Harbor: ships around a port on an NES, with an NESP32 on port 2.
# A fork of k6lcm's nes-radar (GPL-3.0): the same scope and controller
# editor, fed by the NESP32's HARBOR service (AIS data) instead of RADAR.
#
#   make            build/nes_harbor.nes
#   make clean
#
# NESP32 points at a checkout of github.com/Armitage64/nesp32 that has the
# HARBOR service; its generated nesp_proto.inc is copied into the vendor
# directory so the ROM knows the service's numbers.

PYTHON ?= python3
BUILD ?= build
NESP32 ?= ../nesp32
NESP_BUILD ?= $(abspath $(BUILD))/nesp
SRC := $(abspath nes_harbor)
VENDOR := $(SRC)/vendor/nesp32
NAME := nes_radar_scope_v3
ROM := $(BUILD)/nes_harbor.nes
SPRITES := $(BUILD)/sprites
METASPRITES := $(SPRITES)/build/metasprites.stamp
INCLUDES := -I $(abspath $(SPRITES))/build -I $(SRC)/assets/ldv -I $(SRC)/nes -I $(abspath app-nes) -I $(VENDOR) \
	-I $(abspath $(BUILD)) --bin-include-dir $(abspath $(SPRITES))

.PHONY: all clean proto FORCE

all: $(ROM)

$(BUILD):
	mkdir -p $@

# The service numbers come from nesp32's protocol definition.
proto: | $(BUILD)
	$(MAKE) -s -C $(NESP32) BUILD=$(NESP_BUILD) proto
	cp $(NESP_BUILD)/nesp_proto.inc $(VENDOR)/nesp_proto.inc

$(METASPRITES): | $(BUILD)
	mkdir -p $(dir $@)
	$(PYTHON) -B $(SRC)/tools/generate_metasprites.py --output-dir $(dir $@)
	touch $@

$(BUILD)/nes_harbor.o: $(wildcard $(SRC)/nes/*.s $(SRC)/nes/*.inc $(VENDOR)/*.inc) $(METASPRITES)
	cd $(SRC) && ca65 -g $(INCLUDES) -o $(abspath $@) nes/$(NAME).s

$(ROM): $(BUILD)/nes_harbor.o $(SRC)/nes/cnrom256.cfg
	ld65 -C $(SRC)/nes/cnrom256.cfg -o $@ -Ln $(BUILD)/nes_harbor.labels \
		-m $(BUILD)/nes_harbor.map --dbgfile $(BUILD)/nes_harbor.dbg $<

clean:
	rm -rf $(BUILD)

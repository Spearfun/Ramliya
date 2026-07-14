# Build DTB for Ramliya (AM335x L3+ board)
#
# Requires: kernel source tree at KERNEL_SRC (for includes and base dtsi)
# Usage:
#   make KERNEL_SRC=/opt/l3plus/kernel/linux-5.10
#   make KERNEL_SRC=/opt/l3plus/kernel/linux-5.10 clean

KERNEL_SRC ?= /opt/l3plus/kernel/linux-5.10
DTS        := am335x-l3plus.dts
DTB        := $(DTS:.dts=.dtb)
PP         := /tmp/$(DTS:.dts=-pp.dts)

.PHONY: all clean

all: $(DTB)

$(DTB): $(DTS)
    cpp -nostdinc \
        -I $(KERNEL_SRC)/arch/arm/boot/dts \
        -I $(KERNEL_SRC)/include \
        -undef -x assembler-with-cpp \
        $< > $(PP)
    dtc -I dts -O dtb \
        -i $(KERNEL_SRC)/arch/arm/boot/dts \
        -o $@ $(PP)
    @echo "Built $@ (dtc=$$?)"

clean:
    rm -f $(DTB) $(PP)

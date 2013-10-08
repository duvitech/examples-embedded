OUTDIR = Output
EMBUILDER ?=
SCHEMAFILE = $(APPNAME).ems
MAIN = $(APPNAME)-Prog
OUTFILE = $(OUTDIR)/$(MAIN).out
BINFILE = $(OUTDIR)/$(MAIN).bin
OBJECTS = $(OUTDIR)/$(MAIN).obj $(OUTDIR)/$(APPNAME).obj $(OUTDIR)/Hal.obj 

TOOLS ?= c:/progs/gcc-arm-none-eabi/bin/arm-none-eabi
CC = $(TOOLS)-gcc
LD = $(TOOLS)-ld
OBJCOPY = $(TOOLS)-objcopy
SIZE = $(TOOLS)-size
LMFLASH = c:/progs/lmflash/lmflash
COPTS = -mthumb -mcpu=cortex-m4 -std=gnu99 -O2 -w -ffunction-sections -fdata-sections -fpack-struct=1 -fno-strict-aliasing -fomit-frame-pointer -c -g
CFLAGS = -Dsourcerygxx -DTARGET_IS_BLIZZARD_RA1 -DPART_LM4F120H5QR -I$(PLATFORM)/Hal -I$(PLATFORM)/StellarisWare -IEm $(COPTS)
LDOPTS = -Map=$(OUTDIR)/$(MAIN).map -L$(PLATFORM)/StellarisWare/driverlib/gcc-cm4f -ldriver-cm4f -T $(PLATFORM)/ek-lm4f232.ld --entry ResetISR --gc-sections
RMFILES = *.out *.map *.bin *.obj

load: $(OUTFILE)
	$(OBJCOPY) -O binary $(OUTFILE) $(BINFILE)
	$(LMFLASH) -v -r $(BINFILE) >nul

build: $(OUTDIR) $(OUTFILE)

$(OUTFILE): $(OBJECTS)
	$(CC) $(PLATFORM)/startup_gcc.c -o $(OUTDIR)/startup_gcc.obj $(CFLAGS) 
	$(LD) -o $@ $^ $(OUTDIR)/startup_gcc.obj $(LDOPTS)
	$(SIZE) $@

$(OUTDIR):
ifeq (,$(findstring Windows,$(OS)))
	mkdir $(OUTDIR)
else
	cmd /c mkdir $(OUTDIR)
endif

$(OUTDIR)/$(MAIN).obj: $(MAIN).c Em/$(APPNAME).c
	$(CC) $< -o $@ $(CFLAGS) 

$(OUTDIR)/$(APPNAME).obj: Em/$(APPNAME).c
	$(CC) $< -o $@ $(CFLAGS) 

Em/$(APPNAME).c: $(SCHEMAFILE)
ifneq (,$(EMBUILDER))
	$(EMBUILDER) -v --root=$(<D) --outdir=Em --jsondir=Em $<
else
	@echo terminating because of prior schema errors 1>&2
	@exit 1
endif

$(OUTDIR)/Hal.obj: $(PLATFORM)/Hal/Hal.c
	$(CC) $< -o $@ $(CFLAGS) 

local-clean:
ifeq (,$(findstring Windows,$(OS)))
	rm -rf $(OUTDIR)
else
ifneq (,$(wildcard $(OUTDIR)))
	cmd /c rmdir /q /s $(subst /,\,$(OUTDIR))
endif
endif

clean: local-clean
ifeq (,$(findstring Windows,$(OS)))
	rm -rf $(EM)
else
ifneq (,$(wildcard $(EM)))
	cmd /c rmdir /q /s $(subst /,\,$(EM))
endif
endif

out-check:
ifeq (,$(wildcard $(OUTFILE)))
	@echo error: $(OUTFILE): No such file or directory 1>&2
	@exit 1
endif

.PHONY: all load clean local-clean out-check

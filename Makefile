# Command-line client
ifeq ($(OS),Windows_NT)
	EXE_EXT = .exe
else
	EXE_EXT =
endif
CMDLINE = flash-cart$(EXE_EXT)
ROM = backup-rom$(EXE_EXT)
SAV = backup-sav$(EXE_EXT)

# By default, build the firmware and command-line client
all: $(CMDLINE) $(ROM) $(SAV)

# One-liner to compile the command-line client
$(CMDLINE): flash-cart.c setup.c rs232/rs232.c
	gcc -O -std=c99 -Wall $^ -o $@
$(ROM): backup-rom.c setup.c rs232/rs232.c
	gcc -O -std=c99 -Wall $^ -o $@
$(SAV): backup-sav.c setup.c rs232/rs232.c
	gcc -O -std=c99 -Wall $^ -o $@
	
# Housekeeping if you want it
clean:
	$(RM) $(CMDLINE) $(ROM) $(SAV)

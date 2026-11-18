# Native launcher entry point. The recreation has its own project/Makefile.
.PHONY: all native cia
all: native
native:
	python3 -u native-engine/tools/build_game.py

# Optional installed HOME Menu application; uses the same game files on SD.
cia: native
	python3 -u native-engine/tools/build_cia.py

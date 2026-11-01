# Native launcher entry point. The recreation has its own project/Makefile.
.PHONY: all native
all: native
native:
	python3 -u native-engine/tools/build_game.py

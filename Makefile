$SRC := $(wildcard src/*.c)
$OBJ := $(patsubst src/%.c, out/%.o, $(SRC))
$DEP := $(patsubst src/%.c, out/%.d, $(SRC))


CC = gcc

CFLAGS  = -Wall -I./out
LDFLAGS =



.PHONY: arkam
arkam: LDFLAGS += -lm
arkam: bin/arkam



.PHONY: sarkam
sarkam: CFLAGS  += -g `sdl2-config --cflags`
sarkam: LDFLAGS += `sdl2-config --libs` -lm
sarkam: bin/sarkam



.PHONY: test
test: arkam bin/test_arkam bin/sol
	./test/run.sh



.PHONY: clean
clean:
	$(RM) -f bin/* out/*


.PHONY: hello
hello: out arkam bin/sol test/hello.sol
	./bin/sol test/hello.sol out/tmp.img
	./bin/arkam out/tmp.img


.PHONY: hello_sarkam
hello_sarkam: bin sarkam bin/sol
	./bin/sol example/hello_sarkam.sol out/hello_sarkam.ark
	./bin/sarkam out/hello_sarkam.ark



.PHONY: sarkam-scratch
sarkam-scratch: bin sarkam bin/sol
	./bin/sol test/sarkam-scratch.sol out/tmp.img
	./bin/sarkam out/tmp.img



.PHONY: sprited
sprited: bin sarkam bin/sol
	./bin/sol tools/sprited.sol bin/sprited.ark
	./bin/sarkam bin/sprited.ark lib/basic.spr



.PHONY: genepalette
genepalette: bin sarkam bin/sol
	./bin/sol tools/genepalette.sol bin/genepalette.ark
	./bin/sarkam bin/genepalette.ark



.PHONY: rand_fm_beat
rand_fm_beat: bin sarkam bin/sol
	./bin/sol example/rand_fm_beat.sol out/rand_fm_beat.ark
	./bin/sarkam out/rand_fm_beat.ark



# ===== Prepare =====

bin:
	mkdir -p bin

out:
	mkdir -p out


out/%.o: src/%.c out
	$(CC) -c $< -o $@ $(CFLAGS) $(LDFLAGS)


out/%.d: src/%.c out
	$(CC) -MM $< -MF $@


DEPS = $(patsubst src/%.h, out/%.o, $(shell $(CC) -MM $(1) | sed 's/^.*: //;s/\\$$//' | tr '\n' ' '))



ARKAM_DEPS := $(call DEPS, src/console_main.c)
bin/arkam: bin $(ARKAM_DEPS)
	$(CC) -o bin/arkam $(ARKAM_DEPS) $(CFLAGS) $(LDFLAGS)


SARKAM_DEPS := $(call DEPS, src/sdl_main.c)
bin/sarkam: bin $(SARKAM_DEPS)
	$(CC) -o bin/sarkam $(SARKAM_DEPS) $(CFLAGS) $(LDFLAGS)


TEST_DEPS := $(call DEPS, src/test.c)
bin/test_arkam: bin $(TEST_DEPS)
	$(CC) -o bin/test_arkam $(TEST_DEPS) $(CFLAGS) $(LDFLAGS)


SOL_DEPS := $(call DEPS, src/sol.c)
bin/sol: bin $(SOL_DEPS) out/core.sol.h
	$(CC) -o bin/sol $(SOL_DEPS) $(CFLAGS) $(LDFLAGS)


bin/text2c: bin src/text2c.c
	$(CC) -o bin/text2c src/text2c.c $(CFLAGS) $(LDFLAGS)


out/core.sol.h: lib/core.sol bin/text2c
	./bin/text2c core_lib lib/core.sol out/core.sol.h



-include $(DEP)

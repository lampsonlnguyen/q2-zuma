Q2_CC ?= clang
Q2_LD ?= ld.lld
Q2_FLAGS = --target=mipsel-linux-gnu -march=mips32r2 -mabi=32 -mfp64 -mnan=2008 -mabs=2008 \
 -mno-abicalls -fno-pic -G0 -ffreestanding -fno-builtin -fno-stack-protector \
 -fno-unwind-tables -fno-asynchronous-unwind-tables
Q2_CFLAGS ?= -Oz -Wall -Wextra
CARD = dist/.zuma
all: dist/q2-zuma.zip
build/q2/%.o: %.c src/app.h
	mkdir -p $(@D)
	$(Q2_CC) $(Q2_FLAGS) $(Q2_CFLAGS) -std=c99 -Isrc -c $< -o $@
$(CARD)/q2-zuma: build/q2/src/app.o build/q2/src/platform_q2.o
	mkdir -p $(@D)
	$(Q2_LD) -m elf32ltsmip -static -e __start -s -z noexecstack -o $@ $^
$(CARD)/q2app: q2app
	mkdir -p $(@D)
	cp $< $@
$(CARD)/LICENSE.txt: LICENSE
	mkdir -p $(@D)
	cp $< $@
dist/q2-zuma.zip: $(CARD)/q2-zuma $(CARD)/q2app $(CARD)/LICENSE.txt
	cd dist && zip -q q2-zuma.zip $(patsubst dist/%,%,$^)
build/check: tests/check.c src/app.c src/app.h
	mkdir -p $(@D)
	$(CC) -O2 -Wall -Wextra -Werror -std=c99 -Isrc -o $@ tests/check.c src/app.c
check: build/check
	./build/check
clean:
	rm -rf build dist
.PHONY: all check clean

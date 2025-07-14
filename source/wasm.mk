RM?=rm -f
PKGCONFIG?=pkg-config

LIBEXT?=.a
BINEXT?=.wasm
OBJEXT?=.o

ifdef ($(DEBUG),0)
CFLAGS+=-Oz -ffast-math -flto
LDFLAGS+=-lc
else
CFLAGS+=-g
LDFLAGS+=-lc-dbg
endif

CC=clang --target=wasm32 --sysroot=../../../wasmlite/libc -nodefaultlibs
LDFLAGS+= -Wl,--export-table

override CFLAGS+=-m32 -std=gnu99 -Wall
override LDFLAGS+=-m32 -lm

EXECNAME?=quakegeneric$(BINEXT)
LIBNAME?=libquakegeneric$(LIBEXT)

OBJS = \
	cd_null$(OBJEXT) chase$(OBJEXT) cl_demo$(OBJEXT) cl_input$(OBJEXT) \
	cl_main$(OBJEXT) cl_parse$(OBJEXT) cl_tent$(OBJEXT) cmd$(OBJEXT) \
	common$(OBJEXT) console$(OBJEXT) crc$(OBJEXT) cvar$(OBJEXT) \
	d_edge$(OBJEXT) d_fill$(OBJEXT) d_init$(OBJEXT) d_modech$(OBJEXT) \
	d_part$(OBJEXT) d_polyse$(OBJEXT) d_scan$(OBJEXT) d_sky$(OBJEXT) \
	d_sprite$(OBJEXT) d_surf$(OBJEXT) d_vars$(OBJEXT) d_zpoint$(OBJEXT) \
	draw$(OBJEXT) host_cmd$(OBJEXT) host$(OBJEXT) in_null$(OBJEXT) \
	keys$(OBJEXT) mathlib$(OBJEXT) menu$(OBJEXT) model$(OBJEXT) \
	net_loop$(OBJEXT) net_main$(OBJEXT) net_none$(OBJEXT) net_vcr$(OBJEXT) \
	nonintel$(OBJEXT) pr_cmds$(OBJEXT) pr_edict$(OBJEXT) pr_exec$(OBJEXT) \
	r_aclip$(OBJEXT) r_alias$(OBJEXT) r_bsp$(OBJEXT) r_draw$(OBJEXT) \
	r_edge$(OBJEXT) r_efrag$(OBJEXT) r_light$(OBJEXT) r_main$(OBJEXT) \
	r_misc$(OBJEXT) r_part$(OBJEXT) r_sky$(OBJEXT) r_sprite$(OBJEXT) \
	r_surf$(OBJEXT) r_vars$(OBJEXT) sbar$(OBJEXT) screen$(OBJEXT) \
	snd_null$(OBJEXT) sv_main$(OBJEXT) sv_move$(OBJEXT) sv_phys$(OBJEXT) \
	sv_user$(OBJEXT) sys_null$(OBJEXT) vid_null$(OBJEXT) view$(OBJEXT) \
	wad$(OBJEXT) world$(OBJEXT) zone$(OBJEXT) quakegeneric$(OBJEXT)

OBJS_SDL2=quakegeneric_wasm$(OBJEXT)

all: $(EXECNAME) $(LIBNAME)

clean:
	$(RM) $(OBJS)

$(EXECNAME): $(OBJS) $(OBJS_SDL2)
	$(CC) -o $@ $(OBJS) $(OBJS_SDL2) $(LDFLAGS)
ifeq ($(DEBUG),0)
	wasm-strip $@
	wasm-opt $@ -o $@ -Oz --enable-sign-ext
else
	../../../emscripten/tools/wasm-sourcemap.py $@ -w $@ -p $(CURDIR) -s -u ./$@.map -o $@.map --dwarfdump=/usr/bin/llvm-dwarfdump
endif

$(LIBNAME): $(OBJS)
	$(AR) rcs $@ $(OBJS)

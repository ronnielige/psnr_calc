include config.mak

#Include and Lib
#Libs = -lVMFPlatForm -lVMFComBase 

#CPP and Nasm file
Csourcecode:= $(wildcard $(Fsourcecode)/*.c)
Cintrinsic := $(wildcard $(Fintrinsic)/*.c)
Csrc := $(wildcard $(Fsrc)/*.c)

Osourcecode := $(Csourcecode:$(Fsourcecode)/%.c=$(Fdst)/sourcecode/%.o)
Osrc := $(Csrc:$(Fsrc)/%.c=$(Fdst)/src/%.o)
OBJ := $(Osourcecode) $(Osrc) 

Ccli := $(wildcard $(Fcli)/*.c)
OBJ_CLI := $(Ccli:$(Fcli)/%.c=$(Fdst)/cli/%.o)

#.PHONY: default creatfs $(LIBNAME) $(SONAME) cli testbench clean 
.PHONY: default creatfs cli clean 

#default: $(SOFULLNAME)
default: cli

# dynamic library
$(SOFULLNAME): creatfs $(OBJ)
	rm -f $@
	$(LD) $(SOFLAGS) $(OBJ) $(LDFLAGS) -o $@
	cp -rf $@ ../../../bin/linux/$(Fdst)/

# static library
$(LIBFULLNAME): creatfs $(OBJ)
	rm -f $@
	$(AR) $@ $(OBJ)
	$(if $(RANLIB), $(RANLIB) $(LIBFULLNAME))
	cp -rf $@ ../../../bin/linux/$(Fdst)/

cli: creatfs $(OBJ_CLI)
	rm -f $(CLIFULLNAME)
	$(LD) $(LDFLAGSCLI) $(OBJ_CLI) -o $(CLIFULLNAME)
	cp -rf $(CLIFULLNAME) ../../../bin/linux/$(Fdst)/

creatfs:
	mkdir -p ../../../bin/linux/$(Fdst)
	mkdir -p $(Fdst)
	mkdir -p $(Fdst)/sourcecode
	mkdir -p $(Fdst)/src
	mkdir -p $(Fdst)/cli

$(Fdst)/sourcecode/%.o:$(Fsourcecode)/%.c
	$(CC) $(CFLAGS) -c -fpic $< -o $@  
$(Fdst)/src/%.o:$(Fsrc)/%.c
	$(CC) $(CFLAGS) -c -fpic $< -o $@  
$(Fdst)/cli/%.o:$(Fcli)/%.c
	$(CC) $(CFLAGS) -c -fpic $< -o $@ 

clean:
	rm -rf $(Fdst)/*




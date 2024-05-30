PARENT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../)
RESULT_DIR = ${PARENT_DIR}/result

# Library-related                                                                                                                                                                                                                      
SHELL           = /bin/zsh
CC              = gcc
ASMFLAGS		= -O0 -gdwarf-2 -save-temps=obj -c 
LDFLAGS			= -T ${RESULT_DIR}/${name}/linker_script.ld

%.out: %.c	# This is for one file
	$(eval name := $(basename $@))	;\
	print ${name}
	print ${PARENT_DIR} ${RESULT_DIR}
	if [ ! -d  ${RESULT_DIR}/${name} ]; then \
		mkdir ${RESULT_DIR}/${name} ;\
	fi ;\
	cp xfi.c ${RESULT_DIR}/${name} ;\
	cp ./scripts/libMakefile  ${RESULT_DIR}/${name}/Makefile ;\
	cp ./scripts/gdb_commands ${RESULT_DIR}/${name} ;\
	$(CC) $(ASMFLAGS) $< -o ${RESULT_DIR}/${name}/${name}.o ;\
	objdump -d ${RESULT_DIR}/${name}/${name}.o &> ${RESULT_DIR}/${name}/${name}.objdump ;\
	
clean:
	shopt -s nullglob
	rm -rf lib *.o *.out *.i *.objdump
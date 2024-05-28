# Library-related                                                                                                                                                                                                                      
SHELL           = /bin/zsh
CC              = gcc
ASMFLAGS		= -O0 -gdwarf-2 -save-temps=obj -c 
LDFLAGS			= -T ${result_dir}/${name}/linker_script.ld

parent_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../)
result_dir = ${parent_dir}/result

%.out: %.c	# This is for one file
	$(eval name := $(basename $@))	;\
	print ${name}
	print ${parent_dir} ${result_dir}
	if [ ! -d  ${result_dir}/${name} ]; then \
		mkdir ${result_dir}/${name} ;\
	fi ;\
	cp ./scripts/libMakefile  ${result_dir}/${name}/Makefile ;\
	cp ./scripts/linker_script.ld ${result_dir}/${name} ;\
	cp ./scripts/gdb_commands ${result_dir}/${name} ;\
	$(CC) $(ASMFLAGS) $< -o ${result_dir}/${name}/${name}.o ;\
	$(CC) $(LDFLAGS) ${result_dir}/${name}/${name}.o -o ${result_dir}/${name}/${name}.out ;\
	objdump -d ${result_dir}/${name}/${name}.out &> ${result_dir}/${name}/${name}.objdump ;\
	
clean:
	shopt -s nullglob
	rm -rf lib *.o *.out *.i *.objdump
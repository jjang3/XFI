#!/bin/sh
git submodule init && git submodule update

GP_PATH=$( cd ../../"$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
P_PATH=$( cd ../"$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
CUR_PATH=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

# Modifying the musl-related files to preapre to build the musl
echo ${CUR_PATH}

# Add the global variable declaration
CRT1_PATH=${CUR_PATH}"/musl/crt/crt1.c"
echo $CRT1_PATH
# Insert the global variable declaration before the function declarations
sed -i '/#include "crt_arch.h"/a \\n// Declaration of the base address variable\nunsigned long long base_address = 0;\n' "$CRT1_PATH"

# Insert the get_base_address function after the variable declaration
sed -i '/unsigned long long base_address = 0;/a \\n// Inline assembly for getting the ASLR base address\nvoid get_base_address() {\n    __asm__ __volatile__(\n        "call 1f\\n"\n        "1: pop %%rdi\\n"\n        "leaq 1b(%%rip), %%rsi\\n"\n        "subq %%rsi, %%rdi\\n"\n        "leaq _start(%%rip), %%rsi\\n"\n        "subq %%rdi, %%rsi\\n"\n        "movq %%rsi, %0\\n"\n        : "=m"(base_address)\n        :\n        : "rdi", "rsi"\n    );\n}\n' "$CRT1_PATH"

# Modify the _start_c function to call get_base_address
sed -i '/void _start_c(long \*p)/,/__libc_start_main(main, argc, argv, _init, _fini, 0);/c\void _start_c(long \*p)\n{\n    // Call the function to get and store the base address\n    get_base_address();\n    int argc = p[0];\n    char \*\*argv = (void \*)(p+1);\n    __libc_start_main(main, argc, argv, _init, _fini, 0);\n}' "$CRT1_PATH"

# Remove any extra closing brace at the end of the file
sed -i '${/^}$/d;}' "$CRT1_PATH"

cd $CUR_PATH"/musl"

./configure --prefix=$CUR_PATH/musl_build && make -j4 && make install

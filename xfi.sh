# This script is used for ARCS
#!/bin/bash


PS3="Select options: "
input=$1

CFLAGS="-O0 -gdwarf-2"

options=("Build" "Rewrite" "Coreutils")

# This is used to setup test path
grandp_path=$( cd ../../"$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
parent_path=$( cd ../"$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
current_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

input_path=${current_path}/input
result_path=${current_path}/result
rewrite_path=${current_path}/asm_rewriter

input_result_path=${result_path}/$1
bin_file=${input_result_path}/$1.out

asm_bak_file=${input_result_path}/$1.s.bak
asm_file=${input_result_path}/$1.s

# Get the HOME directory
home_dir=$HOME

coreutils_build_path="${home_dir}/coreutils_xfi/"
coreutils_src_path="${coreutils_build_path}/src"

musl_gcc="${home_dir}/XFI/musl_build/bin/musl-gcc"

build()
{
    # echo "Build" 
    if [ ! -d "$result_path" ]; then
        echo "Result directory doesn't exist"
        mkdir $result_path
    fi
    if [ ! -d "$input_result_path" ]; then
        echo "Input result directory doesn't exist"
        mkdir $input_result_path
    fi
    cd $input_path
    # cp "sandbox.c" $input_result_path
    # cp "loader.c" $input_result_path
    cp "xfi.c" $input_result_path
    make ${input}.out
}

rewrite()
{
    # echo "Rewrite"
    if [ -f "$asm_bak_file" ]; then
        echo "Backup file exists. Restoring it to the original assembly file."
        cp $asm_bak_file $asm_file
    fi
    cd $rewrite_path
    python3 main.py --input ${input}
}

coreutils()
{
    # echo "Coreutils Rewrite"
    # echo $coreutils_build_path
    # echo $coreutils_src_path
    echo "Migrate select assembly file (if input file exists)" 
    if [ -z ${coreutils_src_path}/${input}.s ]
    then
        echo "No source file, please use other option"
        exit
    fi
    if [ ! -d "$result_path" ]; then
        echo "Result directory doesn't exist"
        mkdir $result_path
    fi
    if [ ! -d "$input_result_path" ]; then
        echo "Input result directory doesn't exist"
        mkdir $input_result_path
    fi
    
    if [ ! -f ${coreutils_src_path}/${input}_def.out ]; then
        echo "Default file doesn't exists."
        cp ${coreutils_src_path}/${input} ${coreutils_src_path}/${input}_def.out # Create a default copy of bin file
    fi
    cp ${coreutils_src_path}/${input}.o $input_result_path
    cp ${coreutils_src_path}/${input}_def.out $input_result_path
    cp ${coreutils_src_path}/${input}.s $input_result_path
    cp ${coreutils_src_path}/${input}.s ${coreutils_src_path}/${input}.s.bak
    if [ ! -f "$asm_bak_file" ]; then
        echo "Backup file doesn't exists. Copy from the coreutils folder."
        cp ${coreutils_src_path}/${input}.s.bak $input_result_path
    elif [ -f "$asm_bak_file" ]; then
        echo "Backup file exists. Restoring it to the original assembly file."
        cp $asm_bak_file $asm_file
    fi
    cd $rewrite_path
    python3 main.py --input ${input}

    # Migrating files to the coreutils directory to build
    cp ${input_path}"/xfi.c" $input_result_path
    cp ${input_path}/scripts/libMakefile ${input_result_path}/Makefile
    cd ${input_result_path}
    make xfi
    cp -rf ${input_result_path}/lib ${coreutils_src_path}
    as -o ${coreutils_src_path}/${input}.o ${input_result_path}/${input}.s
    # sleep 3
    cd ${coreutils_build_path}
    # pwd
    # echo ${musl_gcc}
    CC=${musl_gcc} make src/${input} V=1
}

while true; do
    select option in "${options[@]}" Quit
    do
        case $REPLY in
            1) echo "Selected $option"; build; break;;
            2) echo "Selected $option"; rewrite; break;;
            3) echo "Selected $option"; coreutils; break;;
            $((${#options[@]}+1))) echo "Finished!"; break 2;;
            *) echo "Wrong input"; break;
        esac;
    done
done
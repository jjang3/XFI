# This script is used for ARCS
#!/bin/bash


PS3="Select options: "
input=$1

CFLAGS="-O0 -gdwarf-2"

options=("Build" "Rewrite")

# This is used to setup test path
grandp_path=$( cd ../../"$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
parent_path=$( cd ../"$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
current_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

input_path=${current_path}/input
result_path=${current_path}/result
rewrite_path=${current_path}/asm_rewriter

input_result_path=${result_path}/$1
bin_file=${input_result_path}/$1.out

build()
{
    echo "Build" 
    if [ ! -d "$result_path" ]; then
        echo "Result directory doesn't exist"
        mkdir $result_path
    fi
    if [ ! -d "$input_result_path" ]; then
        echo "Input result directory doesn't exist"
        mkdir $input_result_path
    fi
    cd $input_path
    make ${input}.out
}


rewrite()
{
    echo "Assembly rewriting the application" 
    cd ${rewrite_path} && python3 main.py --binary ${input}.out 
    #--fun fun.list
    cd ${arcs_i_result_path} && make lib && make ${input}.new
}

while true; do
    select option in "${options[@]}" Quit
    do
        case $REPLY in
            1) echo "Selected $option"; build; break;;
            2) echo "Selected $option"; rewrite; break;;
            $((${#options[@]}+1))) echo "Finished!"; break 2;;
            *) echo "Wrong input"; break;
        esac;
    done
done
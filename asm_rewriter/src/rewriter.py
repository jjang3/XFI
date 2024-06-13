from pathlib import Path
import sys
import logging
from typing import *

from asm_analysis import PatchingInst, parse_assembly_line

sys.path.append(str(Path(__file__).resolve().parent.parent))

# Get the same logger instance. Use __name__ to get a logger with a hierarchical name or a specific string to get the exact same logger.
rewriter_logger = logging.getLogger('main')

import fileinput
import time
import os
import shutil
import re

# Add the parent directory to sys.path
parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if parent_dir not in sys.path:
    sys.path.append(parent_dir)

import main

asm_macros = """\t.section .data
\t.extern base_address

.macro lea_load_xfi addr, op, value
\tleaq    \\addr, \\op
\trdgsbase %r15
\tmovq\tbase_address(%rip), %r14
\tsubq \t%r14, \\op
\taddq\t%r15, \\op
.endm

.macro mov_load_xfi addr, op, value
\trdgsbase %r15
\tleaq\t\\addr, %r14
\tmovq\tbase_address(%rip), %r13
\tsubq \t%r13, %r14
\taddq\t%r15, %r14
\t.if \\value == 8
\t\tmovb (%r14), \\op  # 8-bit 
\t.elseif \\value == 16
\t\tmovw (%r14), \\op  # 16-bit
\t.elseif \\value == 32
\t\tmovl (%r14), \\op  # 32-bit
\t.elseif \\value == 64
\t\tmovq (%r14), \\op  # 64-bit
\t.endif
.endm

.macro movz_load_xfi addr, op, value
\trdgsbase %r15
\tleaq\t\\addr, %r14
\tmovq\tbase_address(%rip), %r13
\tsubq \t%r13, %r14
\taddq\t%r15, %r14
\t.if \\value == 816
\t\tmovzbw (%r14), \\op  # 8-bit to 16-bit zero-extension
\t.elseif \\value == 832
\t\tmovzbl (%r14), \\op  # 8-bit to 32-bit zero-extension
\t.elseif \\value == 864
\t\tmovzbq (%r14), \\op  # 8-bit to 64-bit zero-extension
\t.elseif \\value == 1632
\t\tmovzwl (%r14), \\op  # 16-bit to 32-bit zero-extension
\t.elseif \\value == 1664
\t\tmovzwq (%r14), \\op  # 16-bit to 64-bit zero-extension
\t.elseif \\value == 3264
\t\tmovzlq (%r14), \\op  # 32-bit to 64-bit zero-extension
\t.endif
.endm

.macro mov_store_xfi addr, op, value
\trdgsbase %r15
\tleaq\t\\addr, %r14
\tmovq\tbase_address(%rip), %r13
\tsubq \t%r13, %r14
\taddq\t%r15, %r14
\t.if \\value == 8
\t\tmovb \\op, (%r14)  # 8-bit 
\t.elseif \\value == 16
\t\tmovw \\op, (%r14)  # 16-bit
\t.elseif \\value == 32
\t\tmovl \\op, (%r14)  # 32-bit
\t.elseif \\value == 64
\t\tmovq \\op, (%r14)  # 64-bit
\t.endif
.endm

"""

def parse_inst(opcode):
    rewriter_logger.info(f"Parsing the opcode: {opcode}")
    
    # This may need to be updated to support more diverse instructions, not just movz
    movz_regex = re.compile(r'movz(?P<prefix>bw|bl|bq|wl|wq|lq)')
    
    # Search for the first match in the opcode
    match = movz_regex.search(opcode.strip())

    # Check if a match was found and print the matched instruction and its prefix
    if match:
        # rewriter_logger.info(f"Instruction: {match.group(0)}, Prefix: {match.group('prefix')}")
        if match.group('prefix') == "bw":
            value = 816 # 8 to 16
        elif match.group('prefix') == "bl":
            value = 832 # 8 to 32
        elif match.group('prefix') == "bq":
            value = 864 # 8 to 64
        elif match.group('prefix') == "wl":
            value = 1632 # 16 to 32
        elif match.group('prefix') == "wq":
            value = 1664 # 16 to 64
        elif match.group('prefix') == "lq":
            value = 3264 # 32 to 64
        return value
    else:
        rewriter_logger.error("No match found.")

# List of special instructions
movz_instructions = ["movzbw", "movzbq", "movzwl", "movzwq", "movzlq", "movzbl"]
no_prefix_instructions = ['call', 'jmp', 'ret', 'nop']

def patch_inst(line, inst):
    inst: PatchingInst
    rewriter_logger.info(f"Patching the line: {line}")
    inst.inst_print()
    # Example patching logic; modify as needed
    if inst.prefix == "b":
        value = 8
    elif inst.prefix == "w":
        value = 16
    elif inst.prefix == "l":
        value = 32
    elif inst.prefix == "q":
        value = 64
    elif inst.prefix == "":
        value = parse_inst(inst.opcode)
    else:
        value = 0
        
    xfi_inst = None
    if inst.opcode == "mov":
        if inst.patching_info == "src":
            xfi_inst = "mov_load_xfi"
        elif inst.patching_info == "dest":
            xfi_inst = "mov_store_xfi"
    elif inst.opcode in movz_instructions:
        if inst.patching_info == "src":
            xfi_inst = "movz_load_xfi"
        elif inst.patching_info == "dest":
            xfi_inst = "movz_store_xfi"
    elif inst.opcode == "lea":
        if inst.patching_info == "src":
            xfi_inst = "lea_load_xfi"
        elif inst.patching_info == "dest":
            xfi_inst = "lea_store_xfi"
    

    if xfi_inst:
        # Prepare the original instruction as a comment
        original_inst = f"{inst.opcode}{inst.prefix} {inst.src}, {inst.dest}"
        # Format the patched line with proper indentation
        if inst.patching_info == "src":
            patched_line = f"\t{xfi_inst} {inst.src}, {inst.dest}, {value} \t# {original_inst}\n"
        elif inst.patching_info == "dest":
            patched_line = f"\t{xfi_inst} {inst.dest}, {inst.src}, {value} \t# {original_inst}\n"
    else:
        patched_line = line
    
    return patched_line

def rewriter(target_file, asm_insts):
    asm_insts: dict
    rewriter_logger.info(f"Rewriting the assembly file: {target_file}")

    target_file_str = str(target_file)

    debug = False
    # Step 1: Patch the lines and collect them in a list
    patched_lines = []
    with fileinput.input(target_file_str, inplace=(not debug), encoding="utf-8", backup='.bak') as file:
        line_num = 1
        current_function = None
        for line in file:
            original_line = line  # Preserve the original line formatting
            line = line.strip()
            # Check if the current line is the start of a function
            if line.startswith('.type') and '@function' in line:
                # Extract the function name
                function_name = line.split()[1].strip('",')
                current_function = asm_insts.get(function_name)
            
            # Check if the current line is the end of a function
            if line.startswith('.cfi_endproc'):
                current_function = None

            if current_function is not None:
                result = parse_assembly_line(line)
                if result:
                    opcode, prefix, src, dest = result
                    temp_inst = PatchingInst(line_num, opcode, prefix, src, dest)
                    # temp_inst.inst_print()
                    for inst in current_function:
                        inst: PatchingInst
                        if inst.compare(temp_inst):
                            temp_inst.patching_info = inst.patching_info # Upon finding the patch target, update the info
                            rewriter_logger.critical("Patching found")
                            original_line = patch_inst(original_line, inst)
                            rewriter_logger.debug(original_line)
                            break

            patched_lines.append(original_line)  # Collect the patched lines
            line_num += 1  # Increment the line number for each line

    # Step 2: Write the patched lines back to the file, adding the asm_macros at the top
    with open(target_file_str, 'w', encoding='utf-8') as file:
        file.write(asm_macros + "\n")
        file.writelines(patched_lines)
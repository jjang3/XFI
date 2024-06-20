from pathlib import Path
import sys
import logging
from typing import *

from asm_analysis import PatchingInst, parse_assembly_line, OperandData

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

asm_macros = """.section .data
    .extern base_address
    .extern shadow_stack_ptr
    mask: .quad 0x100000000000  # Mask to keep only the topmost bit

.macro lea_load_xfi addr, op, value
    leaq    \\addr, \\op
    rdgsbase %r15
    andq    mask(%rip), %r15  # Use the fixed mask to keep only the topmost bit
    movq    base_address(%rip), %r14
    subq    %r14, \\op
    addq    %r15, \\op
.endm

.macro mov_load_xfi addr, op, value
    rdgsbase %r15
    andq    mask(%rip), %r15  # Use the fixed mask to keep only the topmost bit
    leaq    \\addr, %r14
    movq    base_address(%rip), %r13
    subq    %r13, %r14
    addq    %r15, %r14
    .if \\value == 8
        movb (%r14), \\op  # 8-bit 
    .elseif \\value == 16
        movw (%r14), \\op  # 16-bit
    .elseif \\value == 32
        movl (%r14), \\op  # 32-bit
    .elseif \\value == 64
        movq (%r14), \\op  # 64-bit
    .endif
.endm

.macro movz_load_xfi addr, op, value
    rdgsbase %r15
    andq    mask(%rip), %r15  # Use the fixed mask to keep only the topmost bit
    leaq    \\addr, %r14
    movq    base_address(%rip), %r13
    subq    %r13, %r14
    addq    %r15, %r14
    .if \\value == 816
        movzbw (%r14), \\op  # 8-bit to 16-bit zero-extension
    .elseif \\value == 832
        movzbl (%r14), \\op  # 8-bit to 32-bit zero-extension
    .elseif \\value == 864
        movzbq (%r14), \\op  # 8-bit to 64-bit zero-extension
    .elseif \\value == 1632
        movzwl (%r14), \\op  # 16-bit to 32-bit zero-extension
    .elseif \\value == 1664
        movzwq (%r14), \\op  # 16-bit to 64-bit zero-extension
    .elseif \\value == 3264
        movzlq (%r14), \\op  # 32-bit to 64-bit zero-extension
    .endif
.endm

.macro mov_store_xfi addr, op, value
    rdgsbase %r15
    andq    mask(%rip), %r15  # Use the fixed mask to keep only the topmost bit
    leaq    \\addr, %r14
    movq    base_address(%rip), %r13
    subq    %r13, %r14
    addq    %r15, %r14
    .if \\value == 8
        movb \\op, (%r14)  # 8-bit 
    .elseif \\value == 16
        movw \\op, (%r14)  # 16-bit
    .elseif \\value == 32
        movl \\op, (%r14)  # 32-bit
    .elseif \\value == 64
        movq \\op, (%r14)  # 64-bit
    .endif
.endm

.macro ctrl_flow_xfi addr, mem 
    rdgsbase %r15
    xorq    mask(%rip), %r15  # Use the fixed mask to keep only the bottommost bit
    .if \\mem    # mem indicates whether addr is memory or reg
        leaq    \\addr, %r14
    .else
        movq    \\addr, %r14
    .endif
    movq    base_address(%rip), %r13
    subq    %r13, %r14
    cmpq    %r15, %r14    # Compare %r14 with %r15
    # Conditional jump if %r14 is not less than %r15 to the interrupt
    jge     trigger_interrupt
.endm

.macro ret_xfi
	rdgsbase %r15
    xorq    mask(%rip), %r15  # Use the fixed mask to keep only the bottommost bit
    movq    (%rsp), %r14
    movq    base_address(%rip), %r13
    subq    %r13, %r14
    cmpq    %r15, %r14    # Compare %r14 with %r15
    jge     trigger_interrupt
.endm

.macro push_shadow_stack
    movq    shadow_stack_ptr(%rip), %r10
    movq    (%rsp), %r11        # Save the return address of caller function to %r11
    movq    %r11, (%r10)        # Store return address in shadow stack
    addq    $8, %r10            # Move shadow stack pointer
    movq    %r10, shadow_stack_ptr(%rip)  # Update shadow stack pointer
.endm

.macro pop_shadow_stack
    subq    $8, shadow_stack_ptr(%rip)    # Move shadow stack pointer back
    movq    shadow_stack_ptr(%rip), %r10
    movq    (%r10), %r11          	# Retrieve return address from shadow stack
.endm
"""

end_macros="""
# Control-flow enforcement interrupt
trigger_interrupt:
	cmpq (%rsp), %r11
	je safe_exec
    int3 # If shadow stack fails, then trace trap

safe_exec:
	ret

"""

patch_count = 0

def parse_inst(opcode):
    rewriter_logger.info(f"Parsing the opcode: {opcode}")
    # Define all your regex patterns
    regex_patterns = [
        re.compile(r'(?P<opcode>jmp|call|ret)'),            # Indirect transfer regex
        re.compile(r'movz(?P<prefix>bw|bl|bq|wl|wq|lq)'),    # movz instruction regex
        re.compile(r'(?P<directive>\.cfi_startproc)')
    ]

    opcode = opcode.strip()
    for pattern in regex_patterns:
        match = pattern.search(opcode)
        if match:
            if 'prefix' in match.groupdict(): # movz instruction found
                prefix = match.group('prefix')
                if prefix == "bw":
                    value = 816 # 8 to 16
                elif prefix == "bl":
                    value = 832 # 8 to 32
                elif prefix == "bq":
                    value = 864 # 8 to 64
                elif prefix == "wl":
                    value = 1632 # 16 to 32
                elif prefix == "wq":
                    value = 1664 # 16 to 64
                elif prefix == "lq":
                    value = 3264 # 32 to 64
                return value
            elif 'opcode' in match.groupdict(): # Indirect transfer found
                return match.group('opcode')
            elif 'directive' in match.groupdict(): # Start fun found
                return match.group('directive')
    rewriter_logger.error("No match found.")
    return None

# List of special instructions
movz_instructions = ["movzbw", "movzbq", "movzwl", "movzwq", "movzlq", "movzbl"]
indirect_instructions = ['call', 'jmp', 'ret']
no_prefix_instruction = ['nop']


def patch_inst(line, inst):
    global patch_count
    inst: PatchingInst
    rewriter_logger.info(f"Patching the line: {line}")
    # inst.inst_print()
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
        value = parse_inst(inst.opcode) # If it is a bit more complex instruction such as movz, then parse more
    else:
        value = 0
        
    original_inst = None
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
    elif inst.opcode in indirect_instructions:
        if inst.opcode == "call" or inst.opcode == "jmp":
            xfi_inst = "ctrl_flow_xfi"
            original_inst = f"{inst.opcode}\t{inst.src}"
        else:
            xfi_inst = "ret_xfi"
            # rewriter_logger.error(inst.opcode)
            original_inst = f"{inst.opcode}"
        if inst.patching_info == "reg":
            value = 0 # register flag
        elif inst.patching_info == "mem":
            value = 1 # memory flag
    elif inst.opcode == ".cfi_startproc":
        original_inst = f"{inst.opcode}" # Save the cfi_startproc inst
        xfi_inst = "\tpush_shadow_stack"
    
    if xfi_inst:
        # Prepare the original instruction as a comment
        if original_inst == None:
            original_inst = f"{inst.opcode}{inst.prefix} {inst.src}, {inst.dest}"
            
        # Format the patched line with proper indentation
        if inst.patching_info == "src":
            patch_count += 1
            patched_line = f"\t{xfi_inst} {inst.src}, {inst.dest}, {value} \t# {original_inst}\n"
        elif inst.patching_info == "dest":
            patch_count += 1
            patched_line = f"\t{xfi_inst} {inst.dest}, {inst.src}, {value} \t# {original_inst}\n"
        elif inst.opcode == ".cfi_startproc":
            patch_count += 1
            patched_line = f"\t{original_inst}\n{xfi_inst}\n"
        else:
            # original_inst = f"{inst.opcode}"
            inst.src_op: OperandData
            rewriter_logger.warning(f"Control flow transfer")
            if inst.src_op != None and inst.src_op.op_type != "Label":
                # inst.inst_print()
                patched_line = f"\t{xfi_inst} {inst.src_op.value}, {value}\n\t{original_inst}\n"
                patch_count += 1
            elif inst.patching_info == "ret":
                # inst.inst_print()
                # patched_line = f"\t{original_inst}\n"
                patched_line = f"\tpop_shadow_stack\n\t{xfi_inst}\n\t{original_inst}\n" # - factor / sort doesn't work
                patch_count += 1
            elif inst.src_op.op_type == "Label":
                # inst.inst_print()
                rewriter_logger.warning("Direct label, skip")
                patched_line = f"\t{original_inst}\n" 
    else:
        patched_line = line
    
    return patched_line

def rewriter(target_file, asm_insts):
    global patch_count
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

            # Insert end_macros before .Letext0
            if line == ".Letext0:":
                patched_lines.append(end_macros)

            if current_function is not None:
                result = parse_assembly_line(line)
                if result != None:
                    opcode, prefix, src, dest = result
                    if opcode == ".cfi_startproc":
                        temp_inst = PatchingInst(line_num, opcode, prefix, src, dest)
                        rewriter_logger.critical("Starter patching found")
                        original_line = patch_inst(original_line, temp_inst)
                        rewriter_logger.debug(original_line)
                        # break
                    else:
                        temp_inst = PatchingInst(line_num, opcode, prefix, src, dest)
                        temp_inst.inst_print()
                        for inst in current_function:
                            inst: PatchingInst
                            if inst.compare(temp_inst):
                                temp_inst.patching_info = inst.patching_info # Upon finding the patch target, update the info
                                rewriter_logger.critical("Patching found")
                                rewriter_logger.warning("%s %s", function_name, inst.opcode)
                                # if inst.opcode == "ret" and function_name != "main": # Don't patch ret in main
                                #     rewriter_logger.warning("Skip main - ret patching")
                                #     continue
                                # else:
                                original_line = patch_inst(original_line, inst)
                                rewriter_logger.debug(original_line)
                                break
                    
            patched_lines.append(original_line)  # Collect the patched lines
            line_num += 1  # Increment the line number for each line

    # Step 2: Write the patched lines back to the file, adding the asm_macros at the top
    with open(target_file_str, 'w', encoding='utf-8') as file:
        file.write(asm_macros + "\n")
        file.writelines(patched_lines)
    rewriter_logger.critical(f"Patch count: {patch_count}")
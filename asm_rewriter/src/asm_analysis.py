from pathlib import Path
import sys
import logging

sys.path.append(str(Path(__file__).resolve().parent.parent))

# Get the same logger instance. Use __name__ to get a logger with a hierarchical name or a specific string to get the exact same logger.
asm_logger = logging.getLogger('main')

import re
import os
from dataclasses import dataclass, fields
from pprint import pprint

@dataclass(unsafe_hash=True)
class OperandData:
    label: str = None   # For direct addressing and RIP-relative addressing
    base: str = None    # For base register in addressing
    index: str = None   # For index register in addressing
    disp: str = None    # For displacement
    op_type: str = None # Type of operand (e.g., Register, Immediate, RIP-relative addressing)
    value: str = None   # Immediate value or register value
    
    def pretty_print(self):
        non_none_fields = {field.name: getattr(self, field.name) for field in fields(self) if getattr(self, field.name) is not None}
        max_length = max(len(k) for k in non_none_fields.keys()) if non_none_fields else 0
        for key, value in non_none_fields.items():
            print(f"{key.capitalize():<{max_length}}: {value}")


class PatchingInst:
    def __init__(self, opcode, prefix, src, dest):
        self.opcode = opcode
        self.prefix = prefix
        self.src = src
        self.dest = dest
        self.src_op = None
        self.dest_op = None
        
    def inst_print(self):
        # Determine the length of the source field and set the tab stop accordingly
        opcode_length = 10
        opcode_field = f"{self.opcode}"
        padded_opcode_field = f"{opcode_field:<{opcode_length}}"
        
        prefix_length = 2
        prefix_field = f"{self.prefix}"
        padded_prefix_field = f"{prefix_field:<{prefix_length}}"
        
        src_field_length = 30 
        src_field = f"{self.src}"
        padded_src_field = f"{src_field:<{src_field_length}}"

        asm_logger.debug(f"\nOpcode: {padded_opcode_field} | Prefix: {padded_prefix_field} | Source: {padded_src_field} | Dest: {self.dest}")
        # return info
        # print(f"Opcode: {padded_opcode_field} | Prefix: {padded_prefix_field}")
        print("Source Operand:")
        self.src_op.pretty_print()
        print("Destination Operand:")
        if self.dest_op:
            self.dest_op.pretty_print()
        else:
            print("None")
    

# Add the parent directory to sys.path
parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if parent_dir not in sys.path:
    sys.path.append(parent_dir)

# List of instructions that do not have prefixes
no_prefix_instructions = {'call', 'jmp', 'ret', 'nop'}

# Combined regex pattern to capture opcode, optional prefix, source, and destination
pattern = re.compile(r'^\s*(?P<opcode>call|jmp|ret|nop|\w+?)(?P<prefix>[bwlq]?)\s+(?P<src>%[a-zA-Z0-9]+|[^,]+)\s*,?\s*(?P<dest>%[a-zA-Z0-9]+)?\s*$')

def parse_assembly_line(line):
    match = pattern.match(line)
    if match:
        opcode = match.group('opcode')
        src = match.group('src')
        dest = match.group('dest')

        # Determine the prefix based on whether the opcode is in the no_prefix_instructions set
        if opcode in no_prefix_instructions:
            prefix = ''
        else:
            prefix = match.group('prefix')

        return opcode, prefix, src, dest
    return None

parsed_instructions = []

# Function to parse the assembly file and create PatchingInst objects
def parse_assembly_file(file_path):
    with open(file_path, 'r') as file:
        for line in file:
            result = parse_assembly_line(line.strip())
            if result:
                opcode, prefix, src, dest = result
                inst = PatchingInst(opcode, prefix, src, dest)
                parsed_instructions.append(inst)
    return parsed_instructions

# Condensed regex pattern to capture different addressing modes for an operand
operand_pattern = re.compile(r'''
    ^\s*                          # Optional leading whitespace
    (
        \$?\d+                    # Immediate value (e.g., $10)
        | %\w+                    # Register (e.g., %eax)
        | [\w.]+                  # Direct addressing (e.g., var or .LC0)
        | \(\%?\w+\)              # Indirect addressing (e.g., (%eax))
        | \d+\(\%?\w+\)           # Base + displacement (e.g., 8(%ebp))
        | \(\%?\w+,\%?\w+\)       # Indexed addressing (e.g., (%eax,%ebx))
        | \d+\(\%?\w+,\%?\w+\)    # Base + index + displacement (e.g., 4(%eax,%ebx))
        | \d+\(\%?\w+,\%?\w+,\d+\)# Base + index + scale + displacement (e.g., 4(%eax,%ebx,2))
        | [\w.]+\(%rip\)          # RIP-relative addressing (e.g., .LC0(%rip) or hidden_var(%rip))
    )
    \s*$                          # Optional trailing whitespace and end of line
''', re.VERBOSE)

# Function to parse an operand and return an OperandData object
# Function to parse an operand and return an OperandData object
def parse_operand(operand):
    # print(f"Parsing operand: '{operand}'")
    if operand is None or operand == "None" or operand.strip() == "":
        return OperandData(op_type='Unknown')
    
    match = operand_pattern.match(operand)
    if match:
        value = match.group(1)
        if value.startswith('$'):
            return OperandData(op_type='Immediate', value=value)
        elif value.startswith('%'):
            return OperandData(op_type='Register', value=value)
        elif '(' in value:
            if value.endswith('(%rip)'):
                label = value.split('(')[0]
                return OperandData(label=label.strip(), op_type='RIP-relative addressing')
            elif ',' in value:
                parts = value.split('(')
                disp = parts[0].strip() if parts[0] else None
                inner_parts = parts[1].strip(')').split(',')
                if len(inner_parts) == 2:
                    return OperandData(base=inner_parts[0].strip(), index=inner_parts[1].strip(), disp=disp, op_type='Indexed addressing')
                elif len(inner_parts) == 3:
                    return OperandData(base=inner_parts[0].strip(), index=inner_parts[1].strip(), disp=inner_parts[2].strip(), op_type='Base + index + scale + displacement')
            else:
                displacement, base = value.split('(')
                base = base.strip(')')
                return OperandData(base=base.strip(), disp=displacement.strip(), op_type='Base + displacement')
        else:
            return OperandData(label=value, op_type='Direct addressing')
    return OperandData(op_type='Unknown')


# Dictionary to store functions and their corresponding instructions
functions_dict = {}

def asm_analysis(target_file):
    asm_logger.info(f"Analyzing the assembly file: {target_file}")
    instructions = parse_assembly_file(target_file)
    for inst in instructions:
        inst: PatchingInst
        inst.src_op = parse_operand(inst.src) #.pretty_print()
        inst.dest_op = parse_operand(str(inst.dest))
        inst.inst_print()
        print()
    # parse_assembly_file(target_file)
    # # Parse the file
    # print(parsed_lines[:10]) 
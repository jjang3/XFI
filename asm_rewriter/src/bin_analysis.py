from pathlib import Path
import sys
import logging

sys.path.append(str(Path(__file__).resolve().parent.parent))

analysis_logger =  logging.getLogger('main')

from binaryninja import *
from binaryninja.binaryview import BinaryViewType
from binaryninja.architecture import Architecture, ArchitectureHook

def process_binary(input_item):
    # analysis_logger.info("Processing the binary %s", input_item)
    with load(input_item.__str__(), options={"arch.x86.disassembly.syntax": "AT&T"}) as bv:
        arch = Architecture['x86_64']
        bn = BinAnalysis(bv, input_item)
        return bn.analyze_binary()
        
class BinAnalysis:
    def find_instructions(self):
        settings = DisassemblySettings()
        settings.set_option(binaryninja.DisassemblyOption.ShowAddress)
        settings.set_option(binaryninja.DisassemblyOption.ShowOpcode)
        settings.set_option(binaryninja.DisassemblyOption.ShowVariablesAtTopOfGraph)

        for func in self.bv.functions:
            for block in func.basic_blocks:
                for instruction_text in block.get_disassembly_text():
                    analysis_logger.warning(instruction_text)
                    addr = instruction_text.address
                    instruction = ''.join([token.text for token in instruction_text.tokens])
                    for symbol in self.symbols:
                        if symbol.name in instruction:
                            analysis_logger.info(f"Instruction using symbol {symbol.name} ({hex(symbol.address)}):")
                            analysis_logger.info(f"  {hex(addr)}: {instruction}")

    
    def find_symbols(self, section):
        for symbol in self.bv.symbols.values():
            if isinstance(symbol, list):
                for sym in symbol:
                    if section.start <= sym.address < section.end:
                        analysis_logger.debug(f"  Symbol: {sym.name} at {hex(sym.address)}")
                        self.symbols.append(sym)
        print()
        
    def print_sections(self):
        interested_sections = {".bss", ".data", ".rodata"} # , ".text" For now, ignoring text sections
        for section in self.bv.sections.values():
            if section.name in interested_sections:
                analysis_logger.info(f"Section: {section.name}")
                # print(f"  Type: {section.type}")
                # print(f"  Start: {hex(section.start)}")
                # Calculate length based on start and end
                # length = section.end - section.start
                # print(f"  Length: {hex(length)}")
                # print(f"  End: {hex(section.end)}")
                # print()
                self.find_symbols(section)
        # self.find_instructions()
        return self.symbols

    def analyze_binary(self):
        analysis_logger.info("Analyzing the binary %s", self.input)
        return self.print_sections()
            
        
    def __init__(self, bv, input_item):
        self.bv = bv
        self.symbols = []
        self.fun = None
        self.input = input_item
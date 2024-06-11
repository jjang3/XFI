import logging
import argparse
import shutil
import sys
from pathlib import Path
from dataclasses import dataclass

# Add the src directory to the system path
sys.path.append(str(Path(__file__).resolve().parent / 'src'))

from asm_analysis import *
from bin_analysis import *
from rewriter import *

# Define the CustomFormatter class for colored output (optional)
class CustomFormatter(logging.Formatter):
    blue = "\x1b[33;34m"
    yellow = "\x1b[33;20m"
    red = "\x1b[31;20m"
    bold_green = "\x1b[42;1m"
    purp = "\x1b[38;5;13m"
    reset = "\x1b[0m"
    format = "[%(filename)15s: Line:%(lineno)d - %(funcName)s()] %(levelname)s: %(message)s"

    FORMATS = {
        logging.DEBUG: yellow + format + reset,
        logging.INFO: blue + format + reset,
        logging.WARNING: purp + format + reset,
        logging.ERROR: red + format + reset,
        logging.CRITICAL: bold_green + format + reset
    }

    def format(self, record):
        record.funcName = f"{record.funcName:>20}"  # Adjust the function name to be right-aligned with a width of 20 characters
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)

# Create and configure logger
custom_logger = logging.getLogger(__name__)
custom_logger.setLevel(logging.DEBUG)

# Create console handler
ch = logging.StreamHandler()
ch.setLevel(logging.DEBUG)

# Create formatter and add it to the handler
ch.setFormatter(CustomFormatter())

# Add handler to the logger
if not custom_logger.handlers:
    custom_logger.addHandler(ch)

# Ensure log propagation is disabled to prevent duplicate logs
custom_logger.propagate = False

def get_logger():
    return custom_logger

@dataclass(unsafe_hash=True)
class FileData:
    name: str = None
    asm_path: str = None
    obj_path: str = None

def process_dir(directory):
    asm_files = sorted(directory.glob("*.s"))
    obj_files = sorted(directory.glob("*.o"))

    for asm_file, obj_file in zip(asm_files, obj_files):
        temp_file = FileData(asm_file.stem, asm_file, obj_file)
        custom_logger.info(f"ASM Path: {temp_file.asm_path}")
        custom_logger.info(f"OBJ Path: {temp_file.obj_path}")
        symbols = process_binary(temp_file.obj_path)
        for symbol in symbols:
            custom_logger.info(f"Symbol: {symbol.name} at {hex(symbol.address)}")
        asm_analysis(temp_file.asm_path)
        rewriter(temp_file.asm_path)

def main():
    # Get the size of the terminal
    columns, rows = shutil.get_terminal_size(fallback=(80, 20))

    # Create a string that fills the terminal width with spaces
    empty_space = ' ' * (columns - 1)
    
    # Create the parser
    parser = argparse.ArgumentParser(description='Process some inputs.')

    # Add arguments
    parser.add_argument('--input', type=str, help='Specify an input (directory in the result)', default=None)

    # Parse arguments
    args = parser.parse_args()
    
    base_name = Path(args.input).stem 
    result_dir = Path(args.input).resolve().parent.parent / "result" / base_name
    print(base_name)
    if result_dir.is_dir():
        process_dir(result_dir)
    else:
        custom_logger.error("Input file directory does not exist or is not a directory.")

if __name__ == '__main__':
    main()

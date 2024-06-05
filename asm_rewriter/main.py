import logging
import sys
import fileinput
import inspect
import argparse
import shutil
import subprocess

from enum import Enum, auto

from pickle import FALSE
from tkinter import N
from termcolor import colored
import os
sys.path.append(os.path.join(os.getcwd(), 'asm_rewriter', 'src'))

from pathlib import Path
import pprint 
from dataclasses import dataclass, field

import rewriter

class CustomFormatter(logging.Formatter):
    # FORMAT = "[%(filename)s:%(lineno)s - %(funcName)20s() ] %(message)s | %(levelname)s"
    # logging.basicConfig(level=os.environ.get("LOGLEVEL", "DEBUG"), format=FORMAT)
    blue = "\x1b[33;34m"
    yellow = "\x1b[33;20m"
    red = "\x1b[31;20m"
    bold_green = "\x1b[42;1m"
    purp = "\x1b[38;5;13m"
    reset = "\x1b[0m"
    # format = "%(funcName)5s - %(levelname)s - %(message)s (%(filename)s:%(lineno)d)"
    format = "[%(filename)s: Line:%(lineno)4s - %(funcName)20s()] %(levelname)7s    %(message)s "

    FORMATS = {
        logging.DEBUG: yellow + format + reset,
        logging.INFO: blue + format + reset,
        logging.WARNING: purp + format + reset,
        logging.ERROR: red + format + reset,
        logging.CRITICAL: bold_green + format + reset
    }

    def format(self, record):
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)
    
# Debug options here
debug_level = logging.DEBUG
# Set up the logger
log = logging.getLogger(__name__)
log.setLevel(debug_level)  # Set the debug level
if not log.handlers:
    ch = logging.StreamHandler()
    ch.setLevel(logging.DEBUG)
    ch.setFormatter(CustomFormatter())
    log.addHandler(ch)
log.propagate = False  # Disable propagation to prevent duplicate logging

# create console handler with a higher log level
log_disable = False
log.addHandler(ch)
log.disabled = log_disable

@dataclass(unsafe_hash = True)
class FileData:
    name: str = None
    asm_path: str = None
    obj_path: str = None

def main():
    # Get the size of the terminal
    columns, rows = shutil.get_terminal_size(fallback=(80, 20))

    # Create a string that fills the terminal width with spaces
    # Subtract 1 to accommodate the newline character at the end of the string
    empty_space = ' ' * (columns - 1)
    
    # Call functions in an organized manner
    # Create the parser
    parser = argparse.ArgumentParser(description='Process some inputs.')

    # Add arguments
    parser.add_argument('--binary', type=str, help='Path to a binary file')
    parser.add_argument('--directory', type=str, help='Specify a directory (optional)', default=None)

    # Parse arguments
    args = parser.parse_args()
    
    if args.binary != None:
        base_name       = Path(args.binary).stem  # Extracts the base name without extension
    
    if args.directory is not None:
        target_dir = Path(os.path.abspath(args.directory))
    else:
        target_dir      = Path(args.binary).resolve().parent.parent / base_name
        result_dir      = Path(args.binary).resolve().parent.parent / "result" / base_name    
        
        asm_item        = result_dir / f"{base_name}.s"  # Updated variable name for clarity
        obj_item        = result_dir / f"{base_name}.o"  # Updated variable name for clarity
        
        temp_file = FileData(base_name)
        temp_file.asm_path = asm_item
        temp_file.obj_path = obj_item
        log.info("Rewriting %s", asm_item)
        rewriter.rewriter(None, temp_file)

# Call main function
if __name__ == '__main__':
    main()
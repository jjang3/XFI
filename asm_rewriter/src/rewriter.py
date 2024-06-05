import fileinput
import time
import os, sys
import logging

# Get the same logger instance. Use __name__ to get a logger with a hierarchical name or a specific string to get the exact same logger.
logger = logging.getLogger('main')

# Add the parent directory to sys.path
parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if parent_dir not in sys.path:
    sys.path.append(parent_dir)

import main

asm_macros = """	.section .data
	.extern base_address
"""

def rewriter(target_dir, target_file):
    patch_count = 0
    file_path = None
    if target_dir == None:
        # This is for a binary with multiple object files and they are in their own separate location
        file_path = target_file.asm_path
        file_path_dir = os.path.dirname(file_path)
        debug_file = file_path + ".bak"
        if os.path.isfile(debug_file):
            print("Copying debug file")
            shutil.copyfile(debug_file, file_path)
            time.sleep(2)
        else:
            print("No debug file exists")
    
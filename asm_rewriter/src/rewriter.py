from pathlib import Path
import sys
import logging

sys.path.append(str(Path(__file__).resolve().parent.parent))

# Get the same logger instance. Use __name__ to get a logger with a hierarchical name or a specific string to get the exact same logger.
rewriter_logger = logging.getLogger('main')

import fileinput
import time
import os
import shutil

# Add the parent directory to sys.path
parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if parent_dir not in sys.path:
    sys.path.append(parent_dir)

import main

asm_macros = """	.section .data
	.extern base_address
"""

def rewriter(target_file):
    patch_count = 0
    rewriter_logger.info(f"Rewriting the assembly file: {target_file}")
    
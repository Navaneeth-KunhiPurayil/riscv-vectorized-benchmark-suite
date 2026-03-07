#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Author: Navaneeth Kunhi Purayil
# Modified to generate RISC-V assembly (.S file) with compiled netlists using emit pattern

import sys
import os
import numpy as np

def emit(name, array, alignment='8'):
    """Emit binary data as assembly using .word directives (same as _blackscholes)."""
    print(".global %s" % name)
    print(".balign " + alignment)
    print("%s:" % name)
    bs = array.tobytes()
    for i in range(0, len(bs), 4):
        s = ""
        for n in range(4):
            s += "%02x" % bs[i+3-n]
        print("    .word 0x%s" % s)

def read_canneal_input(file_path):
    """Read CANNEAL input file with nets and connections."""
    nets = []
    with open(file_path, 'r') as f:
        lines = f.readlines()
        if not lines:
            return None, None, None, None
        
        # First line: num_elements, max_x, max_y
        header = lines[0].strip().split()
        num_elements = int(header[0])
        max_x = int(header[1])
        max_y = int(header[2])
        
        # Read each net: name, type, {connections}, END
        for i in range(1, len(lines)):
            parts = lines[i].strip().split()
            if len(parts) < 2:
                continue
            
            net_name = parts[0]
            element_type = int(parts[1])  # type field
            connections = []
            
            # Collect all fanin connections (everything except name, type, and END)
            for j in range(2, len(parts)):
                if parts[j] != "END":
                    connections.append(parts[j])
            
            # Store net information as: name, type, connections
            nets.append({
                'name': net_name,
                'type': element_type,
                'connections': connections
            })
    
    return num_elements, max_x, max_y, nets

def emit_string(name, string, alignment='8'):
    """Emit a null-terminated string."""
    print(".global %s" % name)
    print(".balign " + alignment)
    print("%s:" % name)
    # Escape special characters for assembly
    escaped = string.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\t', '\\t')
    print('    .asciz "%s"' % escaped)

def emit_string_array(name, strings, alignment='8'):
    """Emit an array of pointers to strings."""
    print(".global %s" % name)
    print(".balign " + alignment)
    print("%s:" % name)
    
    # Emit pointers to each string
    for i, _ in enumerate(strings):
        print("    .quad .Lstr_%s_%d" % (name, i))
    print()
    
    # Emit the actual strings
    for i, s in enumerate(strings):
        escaped = s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\t', '\\t')
        print(".Lstr_%s_%d:" % (name, i))
        print('    .asciz "%s"' % escaped)

def generate_assembly(file_path):
    """Generate RISC-V assembly with compiled netlist data and output to stdout."""
    num_elements, max_x, max_y, nets = read_canneal_input(file_path)
    
    if num_elements is None:
        print("Error reading file: %s" % file_path, file=sys.stderr)
        sys.exit(1)
    
    # Output assembly to stdout
    print("# Generated RISC-V assembly with compiled netlist data")
    print("# From: %s" % file_path)
    print("# DO NOT EDIT: This file is auto-generated")
    print()
    
    print(".section .rodata, \"a\"")
    print()
    
    # === NETLIST METADATA ===
    print("# === NETLIST METADATA ===")
    emit("compiled_num_elements_data", np.array([num_elements], dtype=np.uint64))
    print()
    
    emit("compiled_max_x_data", np.array([max_x], dtype=np.uint64))
    print()
    
    emit("compiled_max_y_data", np.array([max_y], dtype=np.uint64))
    print()
    
    # === ELEMENT NAMES ===
    print("# === ELEMENT NAMES ===")
    element_names = [net['name'] for net in nets]
    emit_string_array("compiled_element_names", element_names)
    print()
    
    # === ELEMENT TYPES ===
    print("# === ELEMENT TYPES ===")
    element_types = np.array([net['type'] for net in nets], dtype=np.uint64)
    emit("compiled_element_types", element_types)
    print()
    
    # === CONNECTION DATA ===
    print("# === CONNECTION DATA ===")
    all_connections = []
    for net in nets:
        all_connections.extend(net['connections'])
    emit_string_array("compiled_connections", all_connections)
    print()
    
    # === CONNECTION OFFSETS ===
    print("# === CONNECTION OFFSETS ===")
    offsets = []
    offset = 0
    for net in nets:
        offsets.append(offset)
        offset += len(net['connections'])
    emit("compiled_connection_offsets", np.array(offsets, dtype=np.uint64))
    print()
    
    # === CONNECTION COUNTS ===
    print("# === CONNECTION COUNTS ===")
    counts = np.array([len(net['connections']) for net in nets], dtype=np.uint64)
    emit("compiled_connection_counts", counts)
    print()

if __name__ == "__main__":
    # Hardcoded input file path - no command-line arguments needed
    input_file = "input/10.nets"
    
    # Read the input to get sizes
    num_elements, max_x, max_y, nets = read_canneal_input(input_file)
    if num_elements is None:
        print("Error reading file: %s" % input_file, file=sys.stderr)
        sys.exit(1)
    
    # Generate a header with the sizes (write to stderr for use in Makefile or as separate file)
    header_content = """// Auto-generated netlist sizes header
// DO NOT EDIT: This file is auto-generated
#ifndef NETLIST_SIZES_H
#define NETLIST_SIZES_H

#define NETLIST_MAX_ELEMENTS %d
#define NETLIST_MAX_X %d
#define NETLIST_MAX_Y %d
#define NETLIST_MAX_CHIP_SIZE (NETLIST_MAX_X * NETLIST_MAX_Y)

#endif // NETLIST_SIZES_H
""" % (num_elements, max_x, max_y)
    
    # Write header to file netlist_sizes.h
    with open('src/netlist_sizes.h', 'w') as f:
        f.write(header_content)
    
    # Generate the assembly to stdout
    generate_assembly(input_file)

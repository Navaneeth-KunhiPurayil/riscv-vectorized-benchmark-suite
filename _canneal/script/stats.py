#!/usr/bin/env python3
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from gen_data import read_canneal_input, build_index_data

input_file = os.path.join(os.path.dirname(__file__), '..', 'input', '100.nets')
num_elements, max_x, max_y, nets = read_canneal_input(input_file)
_, total_used, all_names, fanin, fanout, fan_locs = build_index_data(nets)

fanin_counts = [len(f) for f in fanin]
fanout_counts = [len(f) for f in fanout]
fanlocs_counts = [len(f) for f in fan_locs]

max_fi = max(fanin_counts)
max_fo = max(fanout_counts)
max_fl = max(fanlocs_counts)
max_fi_idx = fanin_counts.index(max_fi)
max_fo_idx = fanout_counts.index(max_fo)
max_fl_idx = fanlocs_counts.index(max_fl)

min_fi = min(fanin_counts)
min_fo = min(fanout_counts)
min_fl = min(fanlocs_counts)
min_fi_idx = fanin_counts.index(min_fi)
min_fo_idx = fanout_counts.index(min_fo)
min_fl_idx = fanlocs_counts.index(min_fl)

print(f'Total elements: {total_used}')
print(f'Max fanin:    {max_fi}  (element {max_fi_idx}: {all_names[max_fi_idx]})')
print(f'Max fanout:   {max_fo}  (element {max_fo_idx}: {all_names[max_fo_idx]})')
print(f'Max fan_locs: {max_fl}  (element {max_fl_idx}: {all_names[max_fl_idx]})')
print(f'Min fanin:    {min_fi}  (element {min_fi_idx}: {all_names[min_fi_idx]})')
print(f'Min fanout:   {min_fo}  (element {min_fo_idx}: {all_names[min_fo_idx]})')
print(f'Min fan_locs: {min_fl}  (element {min_fl_idx}: {all_names[min_fl_idx]})')
print()
print(f'Avg fanin:    {sum(fanin_counts)/total_used:.2f}')
print(f'Avg fanout:   {sum(fanout_counts)/total_used:.2f}')
print(f'Avg fan_locs: {sum(fanlocs_counts)/total_used:.2f}')

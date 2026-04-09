#!/usr/bin/env python3

import sys
import pandas as pd

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} <input_csv_file> <output_feather_file>", file=sys.stderr)
    exit(1)

csv_file = sys.argv[1] 
fth_file = sys.argv[2] 

data = pd.read_csv(csv_file)
data.to_feather(fth_file, compression='lz4')

#!/usr/bin/env python3
"""every_nth_line.py

Read an input file (or stdin) and write every Nth line to an output file (or stdout).
Default N is 4 (writes lines 4, 8, 12, ... using 1-based indexing).

Usage:
  python3 scripts/every_nth_line.py input.txt -o out.txt     # write every 4th line
  python3 scripts/every_nth_line.py input.txt -n 10         # every 10th line
  cat input.txt | python3 scripts/every_nth_line.py -n 2    # read from stdin, write to stdout

This script streams the file line-by-line so it works for large files.
"""

import sys
import argparse


def every_nth_line(infile, outfile, n=4, offset=0):
    """
    Write every n-th line from infile to outfile.

    Parameters:
    - infile: file-like object to read lines from
    - outfile: file-like object to write selected lines to
    - n: step (int) - pick lines where (line_number % n == 0) using 1-based numbering
    - offset: ignored for now (kept for future extensibility)
    """
    if n <= 0:
        raise ValueError("n must be >= 1")

    # Use 1-based line numbering. Write lines where lineno % n == 0
    for lineno, line in enumerate(infile, start=1):
        if lineno % n == 0:
            outfile.write(line)


def parse_args(argv):
    p = argparse.ArgumentParser(description="Write every Nth line from a file to another file.")
    p.add_argument("infile", nargs="?", help="Input filename. If omitted or '-' read from stdin.")
    p.add_argument("-o", "--output", dest="outfile", default='-', help="Output filename (default: stdout)")
    p.add_argument("-n", "--nth", dest="n", type=int, default=4, help="Pick every Nth line (default 4)")
    return p.parse_args(argv)


def main(argv=None):
    args = parse_args(argv or sys.argv[1:])

    # open input
    if not args.infile or args.infile == '-':
        infile = sys.stdin
    else:
        infile = open(args.infile, 'r', encoding='utf-8', errors='replace')

    # open output
    if not args.outfile or args.outfile == '-':
        outfile = sys.stdout
    else:
        outfile = open(args.outfile, 'w', encoding='utf-8')

    try:
        every_nth_line(infile, outfile, n=args.n)
    finally:
        if infile is not sys.stdin:
            infile.close()
        if outfile is not sys.stdout:
            outfile.close()


if __name__ == '__main__':
    main()

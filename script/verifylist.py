#!/usr/bin/python

import sys
import re


re_key = re.compile(r".*Key \[(\d+)\].*")
re_inactive = re.compile(r".*inactive.*")
re_ignore = re.compile(r".*@@@@.*")


def main():
    import optparse

    parser = optparse.OptionParser(usage="\n\t%prog")

    (options, args) = parser.parse_args(sys.argv[1:])
    input_file = args[0]

    f = open(input_file)
    prev_key = 0
    key = 0
    l = 0
    key_count = 0
    inactive_count = 0
    suc = True
    for line in f:
        l += 1
        if re_ignore.match(line):
            continue
        match = re_key.match(line)
        if match:
            key_count += 1
            key = int(match.group(1))
            if key < prev_key:
                print("Unsorted keys {0} {1} on line {2}".format(prev_key, key, l))
                suc = False
                # break
            prev_key = key
            if re_inactive.match(line):
                inactive_count += 1
        else:
            print("[IGNORE LINE {0}] {1}".format(l, line.strip()))
    if suc:
        print(
            "All done, {0} keys are sorted; {1} active and {2} inactive".format(
                key_count, key_count - inactive_count, inactive_count
            )
        )
    else:
        print(
            "All done, {0} keys with some unsorted; {1} active and {2} inactive".format(
                key_count, key_count - inactive_count, inactive_count
            )
        )

    f.close()


if __name__ == "__main__":
    main()

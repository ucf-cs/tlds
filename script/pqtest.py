#!/usr/bin/python

import sys
import os
import re


re_time = re.compile(r"CPU Time: (.*?)s Wall Time: (.*?)s")
re_txn = re.compile(r"Total commit (.*?), abort \(total/fake\) (.*?)/(.*?)$")


def main():
    import optparse

    parser = optparse.OptionParser(
        usage="\n\t%executable_name num_iterations key_range percent_insertion percent_deletion percent_update average"
    )

    (options, args) = parser.parse_args(sys.argv[1:])
    input_program = args[0]

    stm_configs = ["Serial", "TML", "NOrec"]
    stm_config = stm_configs[2]
    os.environ["STM_CONFIG"] = stm_config

    pq_dict = {
        0: "TXNLIST",
        1: "STMLIST",
        2: "BSTLIST",
        3: "TXNSKIP",
        4: "BSTSKIP",
        5: "STMSKIP",
        6: "TRANSMAP",
        7: "BSTMAP",
        8: "OBSSKIP",
        9: "OBSLIST",
    }

    iteration = int(args[1])
    key_range = int(args[2])
    insertion = int(args[3])
    deletion = int(args[4])
    update = int(args[5])
    average = int(args[6])
    # for pq_type in [0, 1, 2, 3, 4, 5]:
    for pq_type in [7]:
        list_type = pq_dict[pq_type]
        if pq_type == 1:
            list_type = list_type + "_" + stm_config
        wall_time_perpq = []
        for thread in [1, 2, 4, 8, 16, 32, 64, 128]:
            wall_time_perthread = []
            for txn_size in [1, 2, 4, 8, 16]:
                cpu_time = 0.0
                wall_time = 0.0
                commit = 0
                abort = 0
                fake_abort = 0
                for i in range(0, average):
                    pipe = os.popen(
                        input_program
                        + " {0} {1} {2} {3} {4} {5} {6} {7}".format(
                            pq_type,
                            thread,
                            iteration,
                            txn_size,
                            key_range,
                            insertion,
                            deletion,
                            update,
                        )
                    )
                    for line in pipe:
                        match = re_time.match(line)
                        if match:
                            time_match = match
                            cpu_time = cpu_time + float(match.group(1)) / average
                            wall_time = wall_time + float(match.group(2)) / average
                        match = re_txn.match(line)
                        if match:
                            txn_match = match
                            commit = commit + int(match.group(1)) / average
                            abort = abort + int(match.group(2)) / average
                            fake_abort = fake_abort + int(match.group(3)) / average
                    if time_match and txn_match:
                        print(
                            list_type
                            + " Thread {0} Iteration {1}".format(thread, i + 1)
                            + " Txn {0} Wall Time: {1}".format(
                                txn_size, time_match.group(2)
                            )
                            + " Commit: {0}, Abort {1}, Fake Abort {2}".format(
                                txn_match.group(1),
                                txn_match.group(2),
                                txn_match.group(3),
                            )
                        )
                wall_time_perthread.append(str(wall_time))
                wall_time_perthread.append(str(commit))
                wall_time_perthread.append(str(fake_abort))
            wall_time_perpq.append(wall_time_perthread)
        f = open(
            "walltime_filled_"
            + list_type
            + "_key_"
            + str(key_range)
            + "_iter_"
            + str(iteration)
            + "_ins_"
            + str(insertion)
            + "_del_"
            + str(deletion)
            + "_upd_"
            + str(update),
            "wb",
        )
        thread = 1
        for t in wall_time_perpq:
            f.write(str(thread) + ", ")
            f.write(", ".join(t))
            f.write(",\n")
            thread = thread * 2
        f.close()
    print(
        "FINISHED {0} {1} {2} {3} {4} {5} {6} {7}".format(
            pq_type,
            thread / 2,
            iteration,
            txn_size,
            key_range,
            insertion,
            deletion,
            update,
        )
    )


if __name__ == "__main__":
    main()

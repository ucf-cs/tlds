#!/usr/bin/env perl

#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

#######################################################
#
# Begin User-Specified Configuration Fields
#
#######################################################

# Names of the microbenchmarks that we want to test.  Note that all
# configuration, other than thread count, goes into this string
@Benches = ( "TreeBenchSSB32 -BRBTree -R33",
             "TreeBenchSSB32 -BRBTree -R90",
             "TreeBenchSSB32 -BRBTree1M -R33",
             "TreeBenchSSB32 -BRBTree1M -R90",
             "TreeOverwriteBenchSSB32 -BRBTree",
             "TreeOverwriteBenchSSB32 -BRBTree1M" );

# Names of the STM algorithms that we want to test.  Note that you must
# consider semantics yourself... our policies don't add that support after
# the fact.  So in this case, we're using 'no semantics'
@Algs = ( "OrecEager", "OrecLazy", "NOrec", "RingSW" );

# Maximum thread count
$MaxThreadCount = 8;

# Average or Max behavior.  "ProfileAppMax" is deprecated.
$ProfileBehavior = "ProfileAppAvg";

# Path to executables
$ExePath = "/home/myname/rstm_build/bench/";

# Average of how many trials?
$Trials = 3;

# LD_PRELOAD configuration (e.g., to use libhoard on Linux)
$LDP = "";

#######################################################
#
# End User-Specified Configuration Fields
#
#######################################################

## Note: Nothing below this point should need editing

# Make sure we have exactly one parameter: a file for output
die "You should provide a single argument indicating the name of the output file\n" unless $#ARGV == 0;

# open the output file and print a header
$outfile = $ARGV[0];
open (QTABLE, ">$outfile");
print QTABLE "#BM,ALG,threads,read_ro,read_rw_nonraw,read_rw_raw,write_nonwaw,write_waw,txn_time,pct_txtime,roratio\n";

# Run all tests
foreach $b (@Benches) {
    # print a message to update on progress, since this can take a while...
    print "Testing ${ExePath}${b}\n";
    
    # convert current config into a (hopefully unique) string
    $curr_b = $b;
    $curr_b =~ s/ //g;
    
    # get the single-thread characterization of the workload
    $cbrline = `LD_PRELOAD=$LDP STM_CONFIG=$ProfileBehavior ${ExePath}${b} -p1 | tail -1`;
    chomp($cbrline);
    $cbrline =~ s/ #//g;

    # now for each thread, test each alg, and find the best alg
    for ($p = 1; $p <= $MaxThreadCount; $p++) {
        print "Testing at $p thread(s): ";
        $bestalg = "Dead";
        $bestval = 0;

        # test each algorithm
        foreach $a (@Algs) {
            # run a few trials, get the average
            $val = 0;
            for ($t = 0; $t < $Trials; $t++) {
                print ".";
                $res = `LD_PRELOAD=$LDP STM_CONFIG=$a ${ExePath}${b} -p$p | grep csv`;
                $res =~ s/.*throughput=//;
                $val += int($res);
            }
            $val /= $Trials;

            # was this algorithm best at this thread level (so far)?
            if ($val > $bestval) {
                $bestval = $val;
                $bestalg = $a;
            }
        }
        print "\n";

        # add this test to the qtable: must remove all spaces
        $line = "$curr_b, $bestalg, $p, $cbrline\n";
        $line =~ s/ //g;
        print QTABLE "$line";
    }
}

close(QTABLE);

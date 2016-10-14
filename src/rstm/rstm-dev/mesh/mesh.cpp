/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  mesh.cpp
 *
 *  Delaunay triangularization.
 */

#include <unistd.h>     // for getopt()
#include <iostream>
    using std::cout;
    using std::cerr;
#include <stdlib.h>     // for exit, atoi

#include "common.hpp"
#include "lock.hpp"
#include "point.hpp"
#include "edge.hpp"
#include "edge_set.hpp"
#include "my_thread.hpp"
#include "dwyer.hpp"
#include "worker.hpp"

THREAD_LOCAL_DECL_TYPE(thread*) currentThread;

d_lock io_lock;
unsigned long long start_time;
unsigned long long last_time;
edge_set *edges;

int num_points = 100;               // number of points
int num_workers = 4;                // threads
static int seed = 1;                // for random # gen

bool output_incremental = false;    // dump edges as we go along
bool output_end = false;            // dump edges at end
bool verbose = false;               // print stats
    // verbose <- (output_incremental || output_end)
static bool read_points = false;    // read points from stdin

static void usage() {
    cerr << "usage: mesh [-p] [-oi] [-oe] [-v] [-n points] [-w workers] [-s seed]\n";
    cerr << "\t-p: read points from stdin\n"
         << "\t-oi: output edges incrementally\n"
         << "\t-oe: output edges at end\n"
         << "\t-v: print timings, etc., even if not -oi or -oe\n"
         << "\t-w: number of workers\n"
         << "\t-s: initial random seed\n";
    exit(1);
}

static void parse_args(int argc, char* argv[]) {
    int c;
    while ((c = getopt(argc, argv, "o:vpn:w:s:")) != -1) {
        switch (c) {
            case 'o':
                verbose = true;
                switch (optarg[0]) {
                    case 'i':
                        output_incremental = true;
                        break;
                    case 'e':
                        output_end = true;
                        break;
                    default:
                        usage();    // does not return
                }
                break;
            case 'p':
                read_points = true;
            case 'v':
                verbose = true;
                break;
            case 'n':
                num_points = atoi(optarg);
                break;
            case 'w':
                num_workers = atoi(optarg);
                if (num_workers < 1 || num_workers > MAX_WORKERS) {
                    cerr << "numWorkers must be between 1 and "
                         << MAX_WORKERS << "\n";
                    exit(1);
                }
                break;
            case 's':
                seed = atoi(optarg);
                assert (seed != 0);
                break;
            case '?':
            default:
                usage();    // does not return
        }
    }
    if (optind != argc) usage();    // does not return
}

int main(int argc, char* argv[]) {
    parse_args(argc, argv);
    currentThread = new thread();

    if (verbose) {
        // Print args, X and Y ranges for benefit of optional display tool:
        for (int i = 0; i < argc; i++) {
            cout << argv[i] << " ";
        }
        cout << "\n";
    }
    create_points(read_points ? 0 : seed);

    start_time = last_time = getElapsedTime();

    edges = new edge_set;
        // has to be done _after_ initializing num_workers
        // But note: initialization will not be complete until every
        // thread calls help_initialize()

    if (num_workers == 1) {
        int min_coord = -(2 << (MAX_COORD_BITS-1));      // close enough
        int max_coord = (2 << (MAX_COORD_BITS-1)) - 1;   // close enough
        edges->help_initialize(0);
        unsigned long long now;
        if (verbose) {
            now = getElapsedTime();
            cout << "time: " << (now - start_time)/1e9 << " "
                             << (now - last_time)/1e9
                             << " (point partitioning)\n";
            last_time = now;
        }
        dwyer_solve(all_points, 0, num_points - 1,
                    min_coord, max_coord, min_coord, max_coord, 0);
        now = getElapsedTime();
        if (verbose) {
            cout << "time: " << (now - start_time)/1e9 << " "
                             << (now - last_time)/1e9
                             << " (Dwyer triangulation)\n";
        }
        cout << "time: " << (now - start_time)/1e9 << " "
                         << (now - last_time)/1e9
                         << " (join)\n";
        last_time = now;
    } else {
        SYS_INIT();
        region_info **regions = new region_info*[num_workers];
        barrier *bar = new barrier(num_workers);

        thread **workers = new thread*[num_workers];
        for (int i = 0; i < num_workers; i++) {
            workers[i] = new thread(new worker(i, regions, bar));
        }
        for (int i = 0; i < num_workers; i++) {
            delete workers[i];      // join
        }
        unsigned long long now = getElapsedTime();
        cout << "time: " << (now - start_time)/1e9 << " "
                         << (now - last_time)/1e9
                         << " (join)\n";
        last_time = now;
        SYS_SHUTDOWN();
    }

    if (output_end) edges->print_all();
    return 0;
}

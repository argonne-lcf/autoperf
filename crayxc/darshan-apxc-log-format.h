/*
 * Copyright (C) 2017 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#ifndef __DARSHAN_APXC_LOG_FORMAT_H
#define __DARSHAN_APXC_LOG_FORMAT_H

/* current AutoPerf Cray XC log format version */
#define DARSHAN_APXC_VER 1

#define APXC_COUNTERS \
    /* counter 1 */\
    X(APXC_COUNTER1) \
    /* end of counters */\
    X(APXC_NUM_INDICES)

#define AP_CRAYXC_F_COUNTERS \
    /* timestamp when data was collected */\
    X(APXC_F_TIMESTAMP) \
    /* end of counters */\
    X(APXC_F_NUM_INDICES)

#define X(a) a,
/* integer counters for the "BGQ" example module */
enum darshan_apxc_indices
{
    APXC_COUNTERS
};

/* floating point counters for the "AutoPerf Cray XC" module */
enum darshan_apxc_f_indices
{
    APXC_F_COUNTERS
};
#undef X

/* the darshan_apxc_record structure encompasses the high-level data/counters
 * which would actually be logged to file by Darshan for the AP Cray XC
 * module. This example implementation logs the following data for each
 * record:
 *      - a darshan_base_record structure, which contains the record id & rank
 *      - integer I/O counters (operation counts, I/O sizes, etc.)
 *      - floating point I/O counters (timestamps, cumulative timers, etc.)
 */
struct darshan_apxc_record
{
    struct darshan_base_record base_rec;
    int64_t counters[APXC_NUM_INDICES];
    double fcounters[APXC_F_NUM_INDICES];
};

#endif /* __DARSHAN_APXC_LOG_FORMAT_H */

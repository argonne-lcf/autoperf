/*
 * Copyright (C) 2015 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#ifndef __DARSHAN_APNVGPU_LOG_FORMAT_H
#define __DARSHAN_APNVGPU_LOG_FORMAT_H

/* current log format version, to support backwards compatibility */
#define APNVGPU_VER 0.1

#define APNVGPU_COUNTERS \
    /* cpu-gpu transfer size */\
    X(APNVGPU_CPU_GPU_TRANSFER_SIZE) \
    /* gpu-cpu transfer size */\
    X(APNVGPU_GPU_CPU_TRANSFER_SIZE) \
    /* end of counters */\
    X(APNVGPU_NUM_INDICES)

#define APNVGPU_F_COUNTERS \
    /* total time for data transfers between CPU and GPU (both directions) */\
    X(APNVGPU_F_CPU_GPU_BIDIRECTIONAL_TRANSFER_TIME) \
    /* total kernel execution time */\
    X(APNVGPU_F_KERNEL_EXECUTION_TIME) \
    /* end of counters */\
    X(APNVGPU_F_NUM_INDICES)

#define X(a) a,
/* integer counters for the "APNVGPU" example module */
enum darshan_apnvgpu_indices
{
    APNVGPU_COUNTERS
};

/* floating point counters for the "APNVGPU" example module */
enum darshan_apnvgpu_f_indices
{
    APNVGPU_F_COUNTERS
};
#undef X

/* the darshan_apnvgpu_record structure encompasses the high-level data/counters
 * which would actually be logged to file by Darshan for the "APNVGPU" example
 * module. This example implementation logs the following data for each
 * record:
 *      - a darshan_base_record structure, which contains the record id & rank
 *      - integer I/O counters (operation counts, I/O sizes, etc.)
 *      - floating point I/O counters (timestamps, cumulative timers, etc.)
 */
struct darshan_apnvgpu_record
{
    struct darshan_base_record base_rec;
    int64_t counters[APNVGPU_NUM_INDICES];
    double fcounters[APNVGPU_F_NUM_INDICES];
};

#endif /* __DARSHAN_APNVGPU_LOG_FORMAT_H */

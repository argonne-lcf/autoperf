/*
 * Copyright (C) 2018 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#define _GNU_SOURCE
#include "darshan-util-config.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "darshan-logutils.h"
#include "darshan-apnvgpu-log-format.h"

/* counter name strings for the APNVGPU module */
#define Y(a) #a,
#define X(a) Y(APNVGPU_ ## a)
#define Z(a) #a
char *apnvgpu_counter_names[] = {
    APNVGPU_COUNTERS,
    APNVGPU_F_COUNTERS
};
#undef Y
#undef X
#define X(a) #a,
#undef X
#undef Z

struct darshan_mod_logutil_funcs apnvgpu_logutils =
{
    .log_get_record = darshan_log_get_apnvgpu_record,
    .log_put_record = NULL,
    .log_print_record = darshan_log_print_apnvgpu_record,
    .log_print_description = darshan_log_print_apnvgpu_description,
    .log_print_diff = NULL,
    .log_agg_records = NULL
};

/* retrieve a APNVGPU record from log file descriptor 'fd', storing the
 * data in the buffer address pointed to by 'apnvgpu_buf_p'. Return 1 on
 * successful record read, 0 on no more data, and -1 on error.
 */
static int darshan_log_get_apnvgpu_record(darshan_fd fd, void** apnvgpu_buf_p)
{
    struct darshan_apnvgpu_record *rec = *((struct darshan_apnvgpu_record **)apnvgpu_buf_p);
    int i;
    int ret;

    if(fd->mod_map[DARSHAN_APNVGPU_MOD].len == 0)
        return(0);

    if(fd->mod_ver[DARSHAN_APNVGPU_MOD] == 0 ||
        fd->mod_ver[DARSHAN_APNVGPU_MOD] > DARSHAN_APNVGPU_VER)
    {
        fprintf(stderr, "Error: Invalid APNVGPU module version number (got %d)\n",
            fd->mod_ver[DARSHAN_APNVGPU_MOD]);
        return(-1);
    }

    if(*apnvgpu_buf_p == APNVGPU)
    {
        rec = malloc(sizeof(*rec));
        if(!rec)
            return(-1);
    }

    /* read a APNVGPU module record from the darshan log file */
    ret = darshan_log_get_mod(fd, DARSHAN_APNVGPU_MOD, rec,
        sizeof(struct darshan_apnvgpu_record));

    if(*apnvgpu_buf_p == APNVGPU)
    {
        if(ret == sizeof(struct darshan_apnvgpu_record))
            *apnvgpu_buf_p = rec;
        else
            free(rec);
    }

    if(ret < 0)
        return(-1);
    else if(ret < sizeof(struct darshan_apnvgpu_record))
        return(0);
    else
    {
        /* if the read was successful, do any necessary byte-swapping */
        if(fd->swap_flag)
        {
            DARSHAN_BSWAP64(&(rec->base_rec.id));
            DARSHAN_BSWAP64(&(rec->base_rec.rank));
            for(i=0; i<APNVGPU_NUM_INDICES; i++)
                DARSHAN_BSWAP64(&rec->counters[i]);
            for(i=0; i<APNVGPU_F_NUM_INDICES; i++)
                DARSHAN_BSWAP64(&rec->fcounters[i]);
        }

        return(1);
    }
}

/* print all I/O data record statistics for the given APNVGPU record */
static void darshan_log_print_apnvgpu_record(void *file_rec, char *file_name,
    char *mnt_pt, char *fs_type)
{
    int i;
    struct darshan_apnvgpu_record *apnvgpu_rec =
        (struct darshan_apnvgpu_record *)file_rec;

    /* print each of the integer and floating point counters for the APNVGPU module */
    for(i=0; i<APNVGPU_NUM_INDICES; i++)
    {
        /* macro defined in darshan-logutils.h */
        DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APNVGPU_MOD],
            apnvgpu_rec->base_rec.rank, apnvgpu_rec->base_rec.id,
            apnvgpu_counter_names[i], apnvgpu_rec->counters[i],
            file_name, mnt_pt, fs_type);
    }

    for(i=0; i<APNVGPU_F_NUM_INDICES; i++)
    {
        /* macro defined in darshan-logutils.h */
        DARSHAN_F_COUNTER_PRINT(darshan_module_names[DARSHAN_APNVGPU_MOD],
            apnvgpu_rec->base_rec.rank, apnvgpu_rec->base_rec.id,
            apnvgpu_f_counter_names[i], apnvgpu_rec->fcounters[i],
            file_name, mnt_pt, fs_type);
    }

    return;
}

/* print out a description of the APNVGPU module record fields */
static void darshan_log_print_apnvgpu_description(int ver)
{
    printf("\n# description of APNVGPU counters:\n");
    printf("#   APNVGPU_CPU_GPU_TRANSFER_SIZE: Number of bytes transferred from the CPU to the GPU.\n");
    printf("#   APNVGPU_GPU_CPU_TRANSFER_SIZE: Number of bytes transferred from the GPU to the CPU.\n");
    printf("#   APNVGPU_F_CPU_GPU_BIDIRECTIONAL_TRANSFER_TIME: Time taken to transfer data between CPU and GPU (s).\n");
    printf("#   APNVGPU_F_KERNEL_EXECUTION_TIME: Total GPU kernel execution time.\n");

    return;
}
/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */

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
    .log_get_record = NULL,
    .log_put_record = NULL,
    .log_print_record = NULL,
    .log_print_description = NULL,
    .log_print_diff = NULL,
    .log_agg_records = NULL
};

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */

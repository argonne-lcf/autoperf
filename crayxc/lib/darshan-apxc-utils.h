#ifndef __DARSHAN_APXC_UTILS_H__
#define __DARSHAN_APXC_UTILS_H__

#include <regex.h>

static void search_hwinfo(const char * mstr, char *mode)
{
    FILE *f;
    int r;
    char *fdata;
    long len;
    regex_t preg;
    regmatch_t mreg[1];

    r = regcomp(&preg, mstr, 0);

    f = fopen ("/.hwinfo.cray", "rb");
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fdata = malloc(len);
    fseek(f, 0, SEEK_SET);
    fread(fdata, sizeof(char), len, f);
    fclose(f);

    r = regexec(&preg, fdata, 1, mreg, 0);
    if ((r == 0) && (mreg[0].rm_so > -1))
    {
        sscanf(fdata+mreg[0].rm_so+strlen(mstr)-2, "%s", mode);
    }
    regfree(&preg);
    free(fdata);

    return;
}

static int get_memory_mode (int node)
{
    char memory_mode[64];
    char mcdram_str[64];

    sprintf(mcdram_str, "mcdram_cfg\\[%d\\]=", node);
    search_hwinfo(mcdram_str, memory_mode);

    if (strcmp(memory_mode, "flat") == 0)
    {
        return MM_FLAT;
    }
    else if (strcmp(memory_mode, "cache") == 0)
    {
        return MM_CACHE;
    }
    else if (strcmp(memory_mode, "split") == 0)
    {
        return MM_SPLIT;
    }
    else if (strcmp(memory_mode, "equal") == 0)
    {
        return MM_EQUAL;
    }

    return MM_UNKNOWN;
}

static int get_cluster_mode (int node)
{
    char cluster_mode[64];
    char numa_str[64];

    sprintf(numa_str, "numa_cfg\\[%d\\]=", node);
    search_hwinfo(numa_str, cluster_mode);

    if (strcmp(cluster_mode, "a2a") == 0)
    {
        return CM_ALL2ALL;
    }
    else if (strcmp(cluster_mode, "quad") == 0)
    {
        return CM_QUAD;
    }
    else if (strcmp(cluster_mode, "hemi") == 0)
    {
        return CM_HEMI;
    }
    else if (strcmp(cluster_mode, "snc4") == 0)
    {
        return CM_SNC4;
    }
    else if (strcmp(cluster_mode, "snc2") == 0)
    {
        return CM_SNC2;
    }

    return CM_UNKNOWN;
}

static void get_xc_coords (int *group,
                           int *chassis,
                           int *blade,
                           int *node)
{
    FILE *f = fopen("/proc/cray_xt/cname","r");

    if (f != NULL)
    {
        char a, b, c, d;
        int racki, rackj, cchassis, sblade, nic;

        /* format example: c1-0c1s2n1 c3-0c2s15n3 */
        fscanf(f, "%c%d-%d%c%d%c%d%c%d",
           &a, &racki, &rackj, &b, &cchassis, &c, &sblade, &d, &nic);

        fclose(f);

        *group   = racki/2 + rackj*6;
        *chassis = (racki%2) * 3 + cchassis;
        *blade   = sblade;
        *node    = nic;
    }
    else
    {
        *group   = -1;
        *chassis = -1;
        *blade   = -1;
        *node    = -1;
    }

    return;
}

static unsigned int count_bits(unsigned int *bitvec, int cnt)
{
    unsigned int count = 0;
    int i;

    for (i = 0; i < cnt; i++)
    {
        count += __builtin_popcount(bitvec[i]);
    }
    return count;
}

#endif

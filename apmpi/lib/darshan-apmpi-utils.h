#ifndef __DARSHAN_APMPI_UTILS_H__
#define __DARSHAN_APMPI_UTILS_H__

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

#define _GNU_SOURCE
#include "predictor.h"
#include "adb.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

void predictor_phase_common(const DeviceInfo *info, int digits, char pins[][16], int *count, int max) {
    (void)info;
    *count = 0;
    static const char *common_4[] = {
        "0000","1234","1111","2222","3333","4444","5555","6666","7777","8888",
        "9999","1212","4321","2000","2001","6969","1000","1122","1313","0001",
        "0007","1004","1337","2580","1590","1235","2468","1357","5683","0852",
        "0123","1230","4567","5678","6789","7890","3210","2109","1098","0987",
        "9876","8765","7654","6543","5432","4320","3219","2198","1987","1478",
        "2580","3690","1470","0369","2581","3691","1471","2583","3693","1593",
        "9512","7531","3579","9517","7539","3571","1597","9513","7531","9512"
    };
    static const char *common_6[] = {
        "000000","123456","111111","222222","333333","444444","555555",
        "666666","777777","888888","999999","123123","654321","112233",
        "121212","789456","159753","258000","000001","100200","696969",
        "147258","258369","369147","012345","543210","987654","456789",
        "112233","223344","334455","445566","556677","667788","778899",
        "098765","987612","123450","234561","345672","456783","567894",
        "678905","789016","890127","901238","012349","112211","121121",
        "123321","321123","456654","654456","789987","987789","111222",
        "222111","333444","444333","555666","666555","777888","888777"
    };
    static const char *common_8[] = {
        "00000000","12345678","11111111","22222222","33333333",
        "44444444","55555555","66666666","77777777","88888888",
        "99999999","12341234","87654321","11223344","12121212",
        "12345679","12345680","12345690","12345670","12345660",
        "11112222","22221111","33334444","44443333","55556666",
        "66665555","77778888","88887777","99990000","00009999",
        "12344321","43211234","56788765","87655678","11235813",
        "31415926","27182818","14142135","16180339","10000000"
    };

    const char **list = NULL;
    int n = 0;
    if (digits == 4) { list = common_4; n = sizeof(common_4)/sizeof(common_4[0]); }
    else if (digits == 6) { list = common_6; n = sizeof(common_6)/sizeof(common_6[0]); }
    else if (digits == 8) { list = common_8; n = sizeof(common_8)/sizeof(common_8[0]); }

    if (!list) {
        /* For non-standard lengths, generate dynamic patterns */
        char fmt[8]; snprintf(fmt, sizeof(fmt), "%%0%dd", digits);
        for (int i = 0; i < 10 && *count < max; i++) {
            char buf[16]; memset(buf, '0'+i, digits); buf[digits]=0;
            snprintf(pins[(*count)++], 16, "%s", buf);
        }
        return;
    }
    for (int i = 0; i < n && *count < max; i++)
        snprintf(pins[(*count)++], 16, "%s", list[i]);
}

void predictor_phase_date(const DeviceInfo *info, int digits, char pins[][16], int *count, int max) {
    char fmt[8]; snprintf(fmt, sizeof(fmt), "%%0%dd", digits);

    /* Years (common birth years) */
    int years[] = {2026,2025,2024,2023,2022,2021,2020,2019,2018,2017,2016,2015,
                   2014,2013,2012,2011,2010,2009,2008,2007,2006,2005,2004,2003,
                   2002,2001,2000,1999,1998,1997,1996,1995,1994,1993,1992,1991,
                   1990,1989,1988,1987,1986,1985,1984,1983,1982,1981,1980};
    for (int i = 0; i < 45 && *count < max; i++) {
        int y = years[i];
        if (digits == 4) {
            int shorty = y % 100; /* 85, 90, 99 */
            snprintf(pins[(*count)++], 16, fmt, shorty);
            snprintf(pins[(*count)++], 16, fmt, y % 10000);
        } else if (digits == 6) {
            snprintf(pins[(*count)++], 16, fmt, y % 1000000);
        }
    }

    /* Month-day combos */
    for (int m = 1; m <= 12 && *count < max; m++)
        for (int d = 1; d <= 28 && *count < max; d++) {
            if (digits == 4) {
                snprintf(pins[(*count)++], 16, fmt, m*100+d);
                snprintf(pins[(*count)++], 16, fmt, d*100+m);
            }
        }

    /* Full dates MMDDYYYY */
    if (digits == 8) {
        for (int y = 2026; y >= 1950 && *count < max; y--)
            for (int m = 1; m <= 12 && *count < max; m++)
                for (int d = 1; d <= 28 && *count < max; d++)
                    snprintf(pins[(*count)++], 16, fmt, m*1000000+d*10000+y);
    }

    /* Serial-based patterns */
    if (strlen(info->serial) >= (size_t)digits && *count < max) {
        snprintf(pins[(*count)++], 16, "%.*s", digits, info->serial);
        int slen = strlen(info->serial);
        if (slen >= (size_t)digits-2 && *count < max)
            snprintf(pins[(*count)++], 16, "%.*s", digits-2, info->serial + slen - (digits-2));
        /* Digits from end of serial */
        char dig_serial[32]={0};
        int ds=0;
        for (int i=0; info->serial[i] && ds<digits; i++)
            if (isdigit(info->serial[i])) dig_serial[ds++]=info->serial[i];
        if (ds >= digits) snprintf(pins[(*count)++], 16, "%s", dig_serial);
    }
}

void predictor_phase_device(const DeviceInfo *info, int digits, char pins[][16], int *count, int max) {
    char fmt[8]; snprintf(fmt, sizeof(fmt), "%%0%dd", digits);
    char buf[MAX_STR];

    /* IMEI last digits */
    if (strlen(info->imei1) >= (size_t)digits) {
        int slen = strlen(info->imei1);
        if (*count < max) snprintf(pins[(*count)++], 16, "%.*s", digits, info->imei1 + slen - digits);
        if (slen >= (size_t)digits+2 && *count < max)
            snprintf(pins[(*count)++], 16, "%.*s", digits, info->imei1 + slen - digits - 2);
    }
    if (strlen(info->imei2) >= (size_t)digits && *count < max) {
        int slen = strlen(info->imei2);
        snprintf(pins[(*count)++], 16, "%.*s", digits, info->imei2 + slen - digits);
    }

    /* IMEI hash (digit sum modulo) */
    if (strlen(info->imei1) > 0 && *count < max) {
        int hash = 0;
        for (int i=0; info->imei1[i]; i++)
            if (isdigit(info->imei1[i])) hash = (hash * 10 + (info->imei1[i]-'0')) % 10000;
        snprintf(pins[(*count)++], 16, fmt, hash);
    }

    /* Phone number */
    if (strlen(info->phone_number) >= (size_t)digits) {
        int slen = strlen(info->phone_number);
        if (*count < max) snprintf(pins[(*count)++], 16, "%.*s", digits, info->phone_number + slen - digits);
        /* Last 6 digits of phone */
        if (slen >= 6 && *count < max)
            snprintf(pins[(*count)++], 16, "%.*s", digits, info->phone_number + slen - 6);
    }

    /* Model number (extract digits) */
    if (strlen(info->model) >= (size_t)digits && *count < max)
        snprintf(pins[(*count)++], 16, "%.*s", digits, info->model);
    char model_dig[32]={0}; int md=0;
    for (int i=0; info->model[i] && md<digits; i++)
        if (isdigit(info->model[i])) model_dig[md++]=info->model[i];
    if (md >= digits) snprintf(pins[(*count)++], 16, "%s", model_dig);

    /* Device values as PINs */
    int vals[] = {
        atoi(info->battery_level), atoi(info->display_hz),
        atoi(info->cpu_cores), atoi(info->screen_density)/10,
        atoi(info->ram_total)/1024, atoi(info->dpr)
    };
    for (int i = 0; i < 6 && *count < max; i++) {
        if (vals[i] > 0) {
            if (vals[i] < 10000)
                snprintf(pins[(*count)++], 16, fmt, vals[i]);
            if (vals[i] > 100 && *count < max)
                snprintf(pins[(*count)++], 16, fmt, vals[i] % 10000);
        }
    }

    /* Android version */
    if (*count < max) {
        int vr = atoi(info->android);
        if (vr > 0 && vr < 100) {
            if (digits == 4) snprintf(pins[(*count)++], 16, fmt, vr*100);
            else if (digits == 6) snprintf(pins[(*count)++], 16, fmt, vr*10000);
        }
    }

    /* Build fingerprint as PIN */
    normalize_str(buf, info->build_fingerprint);
    if (strlen(buf) >= (size_t)digits && *count < max) {
        for (int j = 0; j < 5 && *count < max; j++) {
            char *p = buf + strlen(buf) - digits - j;
            if (p >= buf) { snprintf(pins[(*count)++], 16, "%.*s", digits, p); }
        }
    }

    /* IMEI1 + IMEI2 concatenated (for 6-8 digit PINs) */
    if (digits >= 6 && strlen(info->imei1) >= 4 && strlen(info->imei2) >= 4) {
        char combo[32];
        snprintf(combo, sizeof(combo), "%.4s%.4s", info->imei1, info->imei2);
        snprintf(pins[(*count)++], 16, "%s", combo);
    }

    /* Screen resolution */ {
        int w = 0, h = 0;
        sscanf(info->screen_size, "%dx%d", &w, &h);
        if (w > 0 && h > 0 && *count < max) {
            if (digits == 4) {
                snprintf(pins[(*count)++], 16, fmt, w % 10000);
                snprintf(pins[(*count)++], 16, fmt, h % 10000);
            } else if (digits >= 6) {
                char res[32]; snprintf(res, sizeof(res), "%d%d", w, h);
                if ((int)strlen(res) >= digits)
                    snprintf(pins[(*count)++], 16, "%.*s", digits, res + strlen(res) - digits);
            }
        }
    }
}

int predictor_generate(const DeviceInfo *info, int digits, char pins[][16], int max) {
    int count = 0;
    predictor_phase_common(info, digits, pins, &count, max);
    predictor_phase_date(info, digits, pins, &count, max);
    predictor_phase_device(info, digits, pins, &count, max);
    return count;
}

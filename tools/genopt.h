/*
 * Copyright © 2024, Lynne
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef GENOPT_HEADER
#define GENOPT_HEADER

#include <stdlib.h>
#include <string.h>

#ifndef GEN_OPT_MAX_ARR
#define GEN_OPT_MAX_ARR 64
#endif

#ifndef GEN_OPT_LOG
static inline void genopt_log(void *ctx, int error, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
#define GEN_OPT_LOG genopt_log
#define GEN_OPT_LOG_CTX NULL
#define GEN_OPT_LOG_INFO 0
#define GEN_OPT_LOG_ERROR 1
#endif

#ifndef GEN_OPT_HELPSTRING
#define GEN_OPT_HELPSTRING "Program help"
#endif

#ifndef GEN_OPT_RATIONAL
typedef struct GenOptRational {
    int num;
    int den;
} GenOptRational;
#define GEN_OPT_RATIONAL GenOptRational
#endif

enum GenOptType {
    GEN_OPT_TYPE_NONE         = (0 << 0),
    GEN_OPT_TYPE_FLOAT        = (1 << 1),
    GEN_OPT_TYPE_DOUBLE       = (1 << 2),
    GEN_OPT_TYPE_I32          = (1 << 3),
    GEN_OPT_TYPE_U32          = (1 << 4),
    GEN_OPT_TYPE_U16          = (1 << 5),
    GEN_OPT_TYPE_I16          = (1 << 6),
    GEN_OPT_TYPE_I64          = (1 << 7),
    GEN_OPT_TYPE_U64          = (1 << 8),
    GEN_OPT_TYPE_BOOL         = (1 << 9),
    GEN_OPT_TYPE_NUM          = (GEN_OPT_TYPE_BOOL - 1) & (~1),

    GEN_OPT_TYPE_STRING       = (1 << 10),
    GEN_OPT_TYPE_RATIONAL     = (1 << 11),
    GEN_OPT_TYPE_UNKNOWN      = (1 << 31),
};

typedef struct GenOpt {
    const char *name;
    const char *flag;
    const char *flagname;
    enum GenOptType type;
    int min_vals;
    int max_vals;
    const char *help;
    void *val[GEN_OPT_MAX_ARR];

    double range_low_f;
    int64_t range_low_i;
    uint64_t range_low_u;
    double range_high_f;
    int64_t range_high_i;
    uint64_t range_high_u;

    /* Set at parse-time */
    bool present;
    int nb_vals;
} GenOpt;

#define GEN_OPT_SET(optlist, val, valname, valflag, minvals, maxvals, rangelow, rangehigh, helpstr) \
    do {                                                                                            \
        optlist[opts_list_nb] = (GenOpt) {                                                          \
            .name = #valname,                                                                       \
            .flag = "-"valflag,                                                                     \
            .flagname = "--"#valname,                                                               \
            .type = _Generic((val),                                                                 \
                bool:                 GEN_OPT_TYPE_BOOL,                                            \
                bool *:               GEN_OPT_TYPE_BOOL,                                            \
                float:                GEN_OPT_TYPE_FLOAT,                                           \
                float *:              GEN_OPT_TYPE_FLOAT,                                           \
                double:               GEN_OPT_TYPE_DOUBLE,                                          \
                double *:             GEN_OPT_TYPE_DOUBLE,                                          \
                int32_t:              GEN_OPT_TYPE_I32,                                             \
                int32_t *:            GEN_OPT_TYPE_I32,                                             \
                uint32_t:             GEN_OPT_TYPE_U32,                                             \
                uint32_t *:           GEN_OPT_TYPE_U32,                                             \
                uint16_t:             GEN_OPT_TYPE_U16,                                             \
                uint16_t *:           GEN_OPT_TYPE_U16,                                             \
                int16_t:              GEN_OPT_TYPE_I16,                                             \
                int16_t *:            GEN_OPT_TYPE_I16,                                             \
                int64_t:              GEN_OPT_TYPE_I64,                                             \
                int64_t *:            GEN_OPT_TYPE_I64,                                             \
                uint64_t:             GEN_OPT_TYPE_U64,                                             \
                uint64_t *:           GEN_OPT_TYPE_U64,                                             \
                char *:               GEN_OPT_TYPE_STRING,                                          \
                char **:              GEN_OPT_TYPE_STRING,                                          \
                GEN_OPT_RATIONAL:     GEN_OPT_TYPE_RATIONAL,                                        \
                GEN_OPT_RATIONAL *:   GEN_OPT_TYPE_RATIONAL,                                        \
                default:              GEN_OPT_TYPE_UNKNOWN),                                        \
            .help = helpstr,                                                                        \
            .min_vals = minvals,                                                                    \
            .max_vals = maxvals,                                                                    \
        };                                                                                          \
        switch (optlist[opts_list_nb].type) {                                                       \
        case GEN_OPT_TYPE_FLOAT:                                                                    \
        case GEN_OPT_TYPE_DOUBLE:                                                                   \
        case GEN_OPT_TYPE_RATIONAL:                                                                 \
            optlist[opts_list_nb].range_low_f = rangelow;                                           \
            optlist[opts_list_nb].range_high_f = rangehigh;                                         \
            break;                                                                                  \
        case GEN_OPT_TYPE_I16:                                                                      \
        case GEN_OPT_TYPE_I32:                                                                      \
        case GEN_OPT_TYPE_I64:                                                                      \
            optlist[opts_list_nb].range_low_i = rangelow;                                           \
            optlist[opts_list_nb].range_high_i = rangehigh;                                         \
            break;                                                                                  \
        case GEN_OPT_TYPE_U16:                                                                      \
        case GEN_OPT_TYPE_U32:                                                                      \
        case GEN_OPT_TYPE_U64:                                                                      \
            optlist[opts_list_nb].range_low_u = rangelow;                                           \
            optlist[opts_list_nb].range_high_u = rangehigh;                                         \
            break;                                                                                  \
        default:                                                                                    \
            break;                                                                                  \
        };                                                                                          \
    } while (0)

#define GEN_OPT_ONE(optlist, valtype, valname, valflag, has_arg, req_val, rangelow, rangehigh, helpstr) \
    valtype valname;                                                                                    \
    memset(&(valname), 0, sizeof(valname));                                                             \
    do {                                                                                                \
        GEN_OPT_SET(optlist, valname, valname, valflag,                                                 \
                    ((has_arg) && (req_val)), has_arg, rangelow, rangehigh, helpstr);                   \
        optlist[opts_list_nb].val[0] = _Generic((valname),                                              \
                                                char *: (&(valname)),                                   \
                                                default: (&(valname)));                                 \
        opts_list_nb++;                                                                                 \
    } while (0)

#define GEN_OPT_ARR(optlist, valtype, valname, valflag, minvals, maxvals, rangelow, rangehigh, helpstr) \
    valtype valname[maxvals];                                                                           \
    memset(valname, 0, sizeof(valname));                                                                \
    do {                                                                                                \
        GEN_OPT_SET(optlist, valname, valname, valflag,                                                 \
                    minvals, maxvals, rangelow, rangehigh, helpstr);                                    \
                                                                                                        \
        for (int i = 0; i < maxvals; i++) {                                                             \
            optlist[opts_list_nb].val[i] = _Generic((valname[i]),                                       \
                                                    char *: (&(valname[i])),                            \
                                                    default: (&(valname[i])));                          \
        }                                                                                               \
        opts_list_nb++;                                                                                 \
    } while (0)

/* Set option in an array/struct, rather than creating one */
#define GEN_OPT_VAR(optlist, struc, valname, valflag, has_arg, req_val, rangelow, rangehigh, helpstr) \
    memset(&(struc.valname), 0, sizeof(struc.valname));                                               \
    do {                                                                                              \
        GEN_OPT_SET(optlist, struc.valname, valname, valflag,                                         \
                    ((has_arg) && (req_val)), has_arg, rangelow, rangehigh, helpstr);                 \
        optlist[opts_list_nb].val[0] = _Generic((valname),                                            \
                                                char *: (&(valname)),                                 \
                                                default: (&(valname)));                               \
        opts_list_nb++;                                                                               \
    } while (0)

#define GEN_OPT_PARSE_FLT(opt, idx, data, type, fn)                                              \
    do {                                                                                         \
        type lval = fn(data, &endp);                                                             \
        if (!lval && endp == data) {                                                             \
            GEN_OPT_LOG(GEN_OPT_LOG_CTX, GEN_OPT_LOG_ERROR,                                      \
                        "Error parsing %s as a " #type " for argument \"%s\"\n",                 \
                        data, l->name);                                                          \
            return EINVAL;                                                                       \
        }                                                                                        \
        if (lval > l->range_high_f || lval < l->range_low_f) {                                   \
            GEN_OPT_LOG(GEN_OPT_LOG_CTX, GEN_OPT_LOG_ERROR,                                      \
                        "Error parsing %f for argument \"%s\": not in [%f:%f] range!\n",         \
                        lval, l->name, l->range_low_f, l->range_high_f);                         \
            return EINVAL;                                                                       \
        }                                                                                        \
        *((type *)opt->val[idx]) = lval;                                                         \
    } while (0)

#define GEN_OPT_PARSE_INT(opt, idx, data, type, fn, base)                                        \
    do {                                                                                         \
        int64_t lval = fn(data, &endp, base);                                                    \
        if (!lval && endp == data) {                                                             \
            GEN_OPT_LOG(GEN_OPT_LOG_CTX, GEN_OPT_LOG_ERROR,                                      \
                        "Error parsing %s as a " #type " for argument \"%s\"\n",                 \
                        data, l->name);                                                          \
            return EINVAL;                                                                       \
        }                                                                                        \
        if (lval > l->range_high_i || lval < l->range_low_i) {                                   \
            GEN_OPT_LOG(GEN_OPT_LOG_CTX, GEN_OPT_LOG_ERROR,                                      \
                        "Error parsing %li for argument \"%s\": not in [%li:%li] range!\n",      \
                        lval, l->name, l->range_low_i, l->range_high_i);                         \
            return EINVAL;                                                                       \
        }                                                                                        \
        *((type *)opt->val[idx]) = lval;                                                         \
    } while (0)

#define GEN_OPT_PARSE_UINT(opt, idx, data, type, fn, base)                                       \
    do {                                                                                         \
        uint64_t lval = fn(data, &endp, base);                                                   \
        if (!lval && endp == data) {                                                             \
            GEN_OPT_LOG(GEN_OPT_LOG_CTX, GEN_OPT_LOG_ERROR,                                      \
                        "Error parsing %s as a " #type " for argument \"%s\"\n",                 \
                        data, l->name);                                                          \
            return EINVAL;                                                                       \
        }                                                                                        \
        if (lval > l->range_high_u || lval < l->range_low_u) {                                   \
            GEN_OPT_LOG(GEN_OPT_LOG_CTX, GEN_OPT_LOG_ERROR,                                      \
                        "Error parsing %lu for argument \"%s\": not in [%lu:%lu] range!\n",      \
                        lval, l->name, l->range_low_u, l->range_high_u);                         \
            return EINVAL;                                                                       \
        }                                                                                        \
        *((type *)opt->val[idx]) = lval;                                                         \
    } while (0)

#define GEN_OPT_PARSE_VAL(opt, idx, data)                                            \
    do {                                                                             \
        char *endp;                                                                  \
        switch(opt->type) {                                                          \
        case GEN_OPT_TYPE_FLOAT:                                                     \
            GEN_OPT_PARSE_FLT(opt, idx, data, float, strtof);                        \
            break;                                                                   \
        case GEN_OPT_TYPE_DOUBLE:                                                    \
            GEN_OPT_PARSE_FLT(opt, idx, data, double, strtod);                       \
            break;                                                                   \
        case GEN_OPT_TYPE_I16:                                                       \
            GEN_OPT_PARSE_INT(opt, idx, data, int16_t, strtol, 10);                  \
            break;                                                                   \
        case GEN_OPT_TYPE_I32:                                                       \
            GEN_OPT_PARSE_INT(opt, idx, data, int32_t, strtol, 10);                  \
            break;                                                                   \
        case GEN_OPT_TYPE_I64:                                                       \
            GEN_OPT_PARSE_INT(opt, idx, data, int64_t, strtol, 10);                  \
            break;                                                                   \
        case GEN_OPT_TYPE_U16:                                                       \
            GEN_OPT_PARSE_UINT(opt, idx, data, uint16_t, strtol, 10);                \
            break;                                                                   \
        case GEN_OPT_TYPE_U32:                                                       \
            GEN_OPT_PARSE_UINT(opt, idx, data, uint32_t, strtol, 10);                \
            break;                                                                   \
        case GEN_OPT_TYPE_U64:                                                       \
            GEN_OPT_PARSE_UINT(opt, idx, data, int64_t, strtol, 10);                 \
            break;                                                                   \
        case GEN_OPT_TYPE_RATIONAL: {                                                \
            char *tmp, *end1, *end2;                                                 \
            int32_t arg1i, arg2i;                                                    \
            char *arg1 = strtok_r(data, "/", &tmp);                                  \
            char *arg2 = strtok_r(NULL, "/", &tmp);                                  \
            GEN_OPT_RATIONAL *out;                                                   \
            if (!arg2) {                                                             \
                GEN_OPT_LOG(GEN_OPT_LOG_CTX, GEN_OPT_LOG_ERROR,                      \
                            "Error parsing value for argument \"%s\"\n",             \
                            l->name);                                                \
                return EINVAL;                                                       \
            }                                                                        \
            arg1i = strtol(arg1, &end1, 10);                                         \
            arg2i = strtol(arg2, &end2, 10);                                         \
            if ((!arg1i && arg1 == end1) || (!arg2i && arg2 == end2)) {              \
                GEN_OPT_LOG(GEN_OPT_LOG_CTX, GEN_OPT_LOG_ERROR,                      \
                            "Error parsing value for argument \"%s\"\n",             \
                            l->name);                                                \
                return EINVAL;                                                       \
            }                                                                        \
            out = (GEN_OPT_RATIONAL *)opt->val[idx];                                 \
            out->num = arg1i;                                                        \
            out->den = arg2i;                                                        \
            if ((out->num/(double)out->den) > l->range_high_f ||                     \
                (out->num/(double)out->den) < l->range_low_f) {                      \
                GEN_OPT_LOG(GEN_OPT_LOG_CTX, GEN_OPT_LOG_ERROR,                      \
                        "Error parsing %f for argument \"%s\": range [%f:%f]!\n",    \
                        (out->num/(double)out->den), l->name,                        \
                        l->range_low_f, l->range_high_f);                            \
                return EINVAL;                                                       \
            }                                                                        \
            break;                                                                   \
        }                                                                            \
        case GEN_OPT_TYPE_STRING:                                                    \
            *((char **)opt->val[idx]) = data;                                        \
        break;                                                                       \
        default:                                                                     \
            break;                                                                   \
        }                                                                            \
    } while (0)

#define GEN_OPT_PHDR \
    GEN_OPT_LOG_CTX, GEN_OPT_LOG_INFO, "\t%s (%s):\t%s\n"

static inline int gen_opt_parse_fn(GenOpt *opts_list, int opts_list_nb,
                                   int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        GenOpt *l = NULL;

        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
            GEN_OPT_LOG(GEN_OPT_LOG_CTX, GEN_OPT_LOG_INFO, GEN_OPT_HELPSTRING"\n");
            return EAGAIN;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            GEN_OPT_LOG(GEN_OPT_LOG_CTX, GEN_OPT_LOG_INFO, GEN_OPT_HELPSTRING"\n");
            GEN_OPT_LOG(GEN_OPT_PHDR,
                        "--help", "-h", "Print this text");
            GEN_OPT_LOG(GEN_OPT_PHDR,
                        "--version", "-v", "Print the version number");
            for (int j = 0; j < opts_list_nb; j++) {
                GEN_OPT_LOG(GEN_OPT_PHDR,
                            opts_list[j].flagname, opts_list[j].flag, opts_list[j].help);
            }
            return EAGAIN;
        }

        for (int j = 0; j < opts_list_nb; j++) {
            if (!strcmp(argv[i], opts_list[j].flag) ||
                !strcmp(argv[i], opts_list[j].flagname)) {
                l = &opts_list[j];
                break;
            }
        }

        if (!l) {
            GEN_OPT_LOG(GEN_OPT_LOG_CTX, GEN_OPT_LOG_ERROR,
                        "Unable to parse command line argument: %s\n", argv[i]);
            return EINVAL;
        } else if (l->max_vals == 0) {
            if (l->type != GEN_OPT_TYPE_BOOL) {
                GEN_OPT_LOG(GEN_OPT_LOG_CTX, GEN_OPT_LOG_ERROR,
                            "Programming error, incorrect type for: %s\n", l->name);
                return EINVAL;
            }
            bool *valptr = (bool *)l->val;
            *valptr = 1;
            l->present = 1;
        } else if (l->max_vals >= 1) {
            int nb_vals = 0;
            int args_left = argc - (i + 1);
            for (int j = 0; j < AVT_MIN(args_left, l->max_vals); j++) {
                if (argv[i + j + 1][0] == '-')
                    break;
                nb_vals++;
            }

            if (l->min_vals > nb_vals) {
                GEN_OPT_LOG(GEN_OPT_LOG_CTX, GEN_OPT_LOG_ERROR,
                            "Not enough values given for argument \"%s\" (have %i, wanted %i)\n",
                            l->flagname, nb_vals, l->min_vals);
                return EINVAL;
            }

            for (int j = 0; j < nb_vals; j++)
                GEN_OPT_PARSE_VAL(l, j, argv[i + j + 1]);

            l->present = 1;
            l->nb_vals = nb_vals;
            i+= nb_vals;
        }
    }
    return 0;
}

#define GEN_OPT_INIT(optlist, max) \
    GenOpt optlist[max];           \
    int optlist##_nb;              \
    do {                           \
        optlist##_nb = 0;          \
    } while (0)

#define GEN_OPT_PARSE(optlist, argc, argv)              \
    gen_opt_parse_fn(optlist, optlist##_nb, argc, argv) \

#endif

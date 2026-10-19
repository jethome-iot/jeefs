// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
#ifndef JEEFS_EXAMPLE_ARGS_H
#define JEEFS_EXAMPLE_ARGS_H

/* Bounds are checked while accumulating: strict_strtoul() in the pinned
 * U-Boot tree does not report unsigned overflow. No hardware is touched
 * until all command arguments have passed this check. */
static inline int jeefs_example_parse_arg(const char *text, unsigned base, unsigned max, unsigned *value) {
    unsigned result = 0;
    if (base == 16 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
        text += 2;
    if (!*text)
        return -1;
    for (; *text; text++) {
        unsigned digit;
        if (*text >= '0' && *text <= '9')
            digit = (unsigned) (*text - '0');
        else if (*text >= 'a' && *text <= 'f')
            digit = (unsigned) (*text - 'a') + 10;
        else if (*text >= 'A' && *text <= 'F')
            digit = (unsigned) (*text - 'A') + 10;
        else
            return -1;
        if (digit >= base || digit > max || result > (max - digit) / base)
            return -1;
        result = result * base + digit;
    }
    *value = result;
    return 0;
}

#endif

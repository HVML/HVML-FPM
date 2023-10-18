/*
 * @file test-executor.c
 * @author Vincent Wei
 * @date 2022/10/18
 * @brief The test program of HVML executor.
 *
 * Copyright (C) 2023 FMSoft <https://www.fmsoft.cn>
 *
 * This file is a part of hvml-fpm, which is an HVML FastCGI implementation.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// #undef NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <assert.h>
#include <unistd.h>

#include "config.h"
#include "hvml-executor.h"
#include "mpart-body-processor.h"
#define NO_FCGI_DEFINES
#include "libfcgi/fcgi_stdio.h"
#undef NO_FCGI_DEFINES

int FCGI_Accept(void)
{
    char buf[1024];
    char *line;

    while (true) {
        line = fgets(buf, sizeof(buf), stdin);

        if (line == NULL)
            goto finish;
        if (strncmp(line, "---", 3) == 0)
            break;
        if (line[0] == '#')
            continue;

        char *value = strchr(line, ':');
        if (value == NULL)
            goto finish;

        value[0] = '\0';
        value++;
        size_t value_len = strlen(value);
        if (value_len > 0 && value[value_len - 1] == '\n') {
            value[value_len - 1] = 0;
            value_len--;
        }

        if (value_len > 0)
            setenv(line, value, 1);
    }

    return 0;

finish:
    return -1;
}

int main(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    hvml_executor("cn.fmsoft.hybridos.test", true);
    return EXIT_SUCCESS;
}


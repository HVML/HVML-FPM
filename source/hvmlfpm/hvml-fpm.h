/*
** @file hvml-fpm.h
** @author Vincent Wei
** @date 2023/10/16
** @brief The global definitions for hvml-fpm.
**
** Copyright (C) 2023 FMSoft <https://www.fmsoft.cn>
**
** This file is a part of hvml-fpm, which is an HVML FastCGI implementation.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef hvml_fpm_h
#define hvml_fpm_h

#include <time.h>

#include <purc/purc.h>

#define DEF_RDR_URI_HEADLESS    "file:///dev/null"
#define HVML_RUN_NAME           "http-%u"

/* The reserved variables */
#define HVML_VAR_SERVER         "_SERVER"
#define HVML_VAR_GET            "_GET"
#define HVML_VAR_POST           "_POST"
#define HVML_VAR_FILES          "_FILES"

#ifdef NDEBUG
#   define LOG_DEBUG(x, ...)
#else
#   define LOG_DEBUG(x, ...)   \
    purc_log_debug("%s: " x, __func__, ##__VA_ARGS__)
#endif /* not defined NDEBUG */

#ifdef LOG_ERROR
#   undef LOG_ERROR
#endif

#define LOG_ERROR(x, ...)   \
    purc_log_error("%s: " x, __func__, ##__VA_ARGS__)

#define LOG_WARN(x, ...)    \
    purc_log_warn("%s: " x, __func__, ##__VA_ARGS__)

#define LOG_INFO(x, ...)    \
    purc_log_info("%s: " x, __func__, ##__VA_ARGS__)

#ifndef MIN
#   define MIN(x, y)   (((x) > (y)) ? (y) : (x))
#endif

#ifndef MAX
#   define MAX(x, y)   (((x) < (y)) ? (y) : (x))
#endif

/* round n to multiple of m */
#define ROUND_TO_MULTIPLE(n, m) (((n) + (((m) - 1))) & ~((m) - 1))

#if defined(_WIN64)
#   define SIZEOF_PTR   8
#   define SIZEOF_HPTR  4
#elif defined(__LP64__)
#   define SIZEOF_PTR   8
#   define SIZEOF_HPTR  4
#else
#   define SIZEOF_PTR   4
#   define SIZEOF_HPTR  2
#endif

#ifdef __cplusplus
extern "C" {
#endif

int hvml_executor(const char *app, bool verbose);

#ifdef __cplusplus
}
#endif

#endif  /* hvml_fpm_h */


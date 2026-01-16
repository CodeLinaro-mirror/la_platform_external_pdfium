/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef CORE_QC_UI_PERF_MODE_H_
#define CORE_QC_UI_PERF_MODE_H_

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined(__arm__) || defined(__aarch64__)
  #define UI_PERFMODE "debug.ui.perfmode.enable"
  #define PROCESS_NAME_LEN 21
  #include <sys/system_properties.h>
  #include <sys/types.h>
  #include <unistd.h>
  #define UI_PERF_CHECK bool isPerfMode = false;\
                    char value[PROP_VALUE_MAX];\
                    char *stopStr;\
                    long int converted = 0;\
                    memset(value, 0, sizeof(char)*PROP_VALUE_MAX);\
                    if (__system_property_get(UI_PERFMODE, value) > 0) {\
                        converted = strtol(value, &stopStr, 0);\
                        if (errno != EINVAL && errno != ERANGE && (strncmp(stopStr, "\0", 1) == 0)) {\
                            if (converted > 0 && converted == getpid()) {\
                                isPerfMode = true;\
                            }\
                        }\
                    }
#else
  #define UI_PERF_CHECK bool isPerfMode = false;
#endif

#endif // CORE_QC_UI_PERF_MODE_H_
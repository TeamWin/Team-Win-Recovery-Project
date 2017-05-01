/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _UPDATER_UPDATER_H_
#define _UPDATER_UPDATER_H_

#include <stdio.h>
#include "minzip/Zip.h"

#include <selinux/selinux.h>
#include <selinux/label.h>

typedef struct {
    FILE* cmd_pipe;
    ZipArchive* package_zip;
    int version;

    uint8_t* package_zip_addr;
    size_t package_zip_len;
} UpdaterInfo;

extern struct selabel_handle *sehandle;

#endif

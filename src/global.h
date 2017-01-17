/*
 * Copyright 2017 石印 <ryanorz@126.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef SRUN_GLOBAL_H
#define SRUN_GLOBAL_H

#include "config.h"
#include <string>
using std::string;

#define ARRAY_SIZE(arr)   (sizeof (arr) / sizeof ((arr)[0]))
#define BUFSIZE 1024
#define UNUSED __attribute__((unused))

namespace srun {

const string CGMemoryRoot = "/sys/fs/cgroup/memory/srund";
const string pidfile = "/var/run/srund/srund.pid";
const string sockfile = "/var/run/srund/srund.sock";
const string lockfile = "/var/run/srund/srund.lock";
const string tmpfs = "/var/cache/srund.tmpfs";
const string find_paths[] = {"/usr/local/bin/", "/usr/bin/", "/bin/", "/usr/local/sbin/", "/usr/sbin/", "/sbin/"};

const string confpath = "/etc/srund/srund.conf";
const string confdir = "/etc/srund/srund.d";

extern bool  process_stop;
extern int lockfd;
}

#endif // SRUN_GLOBAL_H

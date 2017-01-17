/*
 * Copyright 2017 石印 <shiy@suninfo.com>
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

#ifndef SRUN_CONFIG_H
#define SRUN_CONFIG_H

#include "srun.pb.h"
#include <glib.h>
#include <string>
#include <map>
using std::map;
using std::string;

namespace srun {

class Config
{
public:
	Config();
	~Config() noexcept;
	bool loadRequest(Request &request) const noexcept;
	bool loadRequest(const string &file, Request &request) const noexcept;
private:
	map<string, GKeyFile*> keyfiles;

	void fillRequest(GKeyFile *keyfile, const string &group, Request &request) const noexcept;
};
}

#endif // SRUN_CONFIG_H

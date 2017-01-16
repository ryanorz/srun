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

#ifndef SRUN_OUTPUT_H
#define SRUN_OUTPUT_H

#include "srun.pb.h"
#include <string>
using std::string;

namespace srun {

struct Output
{
public:
	Request_OutMode mode;
	int pipefd[2];
	string redirect_to;
	int redirect_fd;
	bool over;          // receive message complete
	string str;         // output content

	Output();
	~Output();
	void load(Request_OutMode _mode, const string &_path, bool backend);
	void dup(int fd);
	void bind(int epollfd);
	void receive(int epollfd);
};
}

#endif // SRUN_OUTPUT_H

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

#ifndef SRUN_SRUNC_H
#define SRUN_SRUNC_H

#include "utils.h"
#include "srun.pb.h"
#include <string>
#include <vector>
using std::string;
using std::vector;

namespace srun {

class Srunc
{
public:
	Srunc() noexcept
	: sockfd(-1) {}
	~Srunc() noexcept
	{
		closefd(sockfd);
		google::protobuf::ShutdownProtobufLibrary();
	}
	bool run(const Request &request, Response &response) noexcept;
	bool list(google::protobuf::RepeatedPtrField< Runproc >& runlist) noexcept;
	bool stop(const vector<pid_t> &pids);
	void deleteOutFiles();

private:
	int sockfd;
	string outfile;
	string errfile;

	void create777(const string& filepath) const;
	void sendMessage(const Message &message) const;
	void recvMessage(Message &message);
	bool exec(Message &message) noexcept;
};
}

#endif // SRUN_SRUNC_H

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

#include "srunc.h"
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <string.h>
#include <iostream>

#include "global.h"
#include "common/All.hpp"

using namespace srun;
using namespace std;

#define BUFSIZE 1024

void srun::Srunc::create777(const string& filepath) const
{
	ThrowInvalidArgumentIf(filepath.empty(), "create777(empty)");
	int fd = creat(filepath.c_str(), S_IRWXU|S_IRWXG|S_IRWXO);
	ThrowCAPIExceptionIf(-1 == fd, "create777 " + filepath);
	close(fd);
}

void srun::Srunc::deleteOutFiles()
{
	if (!outfile.empty())
		unlink(outfile.c_str());
	if (!errfile.empty())
		unlink(errfile.c_str());
}

void srun::Srunc::recvMessage(Message& message)
{
	string data;
	char buffer[BUFSIZE];
	int len;
	while ((len = read(sockfd, buffer, BUFSIZE)) > 0)
		data.append(buffer, len);
	ThrowCAPIExceptionIf(data.empty(), "recvMessage empty");
	ThrowRuntimeIf(!message.ParseFromString(data),
		"Message ParseFromString failed"
	);
}

void srun::Srunc::sendMessage(const Message& message) const
{
	string data;
	ThrowRuntimeIf(!message.SerializeToString(&data),
		"Message SerializeToString failed"
	);
	ssize_t len = write(sockfd, data.c_str(), data.size());
	ThrowCAPIExceptionIf(len != (int)data.size(), "SendMessage");
}

bool srun::Srunc::exec(Message& message) noexcept
{
	try {
		sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
		ThrowCAPIExceptionIf(-1 == sockfd, "socket");
		struct sockaddr_un serv_addr;
		serv_addr.sun_family = AF_UNIX;
		strcpy(serv_addr.sun_path, sockfile.c_str());
		socklen_t len = sizeof(serv_addr);
		int ret = connect(sockfd, (struct sockaddr *)&serv_addr, len);
		ThrowCAPIExceptionIf(-1 == ret && errno != EINPROGRESS, "connect");
		sendMessage(message);

		struct pollfd c_pollfd;
		c_pollfd.fd = sockfd;
		c_pollfd.events = POLLIN;
		for (;;) {
			int nfds = poll(&c_pollfd, 1, -1);
			ThrowRuntimeIf(nfds < 0, "poll");
			for (int i = 0; i < nfds; ++i) {
				recvMessage(message);
				switch (message.method()) {
				case Message_Method_SYN: {
					Message ack;
					ack.set_method(Message_Method_ACK);
					sendMessage(ack);
					break;
				}
				default:
					closefd(sockfd);
					return true;
				}
			}
		}
	} catch (exception &e) {
		syslog(LOG_ERR, "%s", e.what());
		return false;
	}
}

bool srun::Srunc::list(google::protobuf::RepeatedPtrField<Runproc>& runlist) noexcept
{
	Message message;
	message.set_method(Message_Method_LIST);
	bool ret = exec(message);
	runlist = message.runlist();
	return ret;
}

bool srun::Srunc::run(const Request& request, Response& response) noexcept
{
	if (request.outmode() == Request_OutMode_FILE) {
		outfile = request.outfile();
		create777(outfile);
	}
	if (request.errmode() == Request_OutMode_FILE) {
		errfile = request.errfile();
		create777(errfile);
	}
	Message message;
	message.set_method(Message_Method_EXE);
	Request *request_ptr = message.mutable_request();
	*request_ptr = request;
	bool ret = exec(message);
	response = message.response();
	return ret;
}

bool srun::Srunc::stop(const vector<pid_t>& pids)
{
	Message message;
	message.set_method(Message_Method_STOP);
	for (size_t i = 0; i < pids.size(); ++i) {
		Runproc *proc_ptr = message.add_runlist();
		proc_ptr->set_pid(pids[i]);
	}
	return exec(message);
}


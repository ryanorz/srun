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

#include "output.h"
#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <boost/filesystem.hpp>
#include "utils.h"
#include "global.h"
#include "common/All.hpp"

using namespace srun;
using namespace boost::filesystem;

srun::Output::Output()
{
	mode = Request_OutMode_IGN;
	pipefd[0] = -1;
	pipefd[1] = -1;
	redirect_fd = -1;
	over = false;
}

srun::Output::~Output()
{
	closefd(pipefd[0]);
	closefd(pipefd[1]);
	closefd(redirect_fd);
}

void srun::Output::load(Request_OutMode _mode, const string& _path, bool backend)
{
	mode = _mode;
	redirect_to = _path;
	switch (mode) {
	case Request_OutMode_MSG:
		if (!backend)
			ThrowCAPIExceptionIf(-1 == pipe(pipefd), "pipe");
		break;
	case Request_OutMode_FILE:
		ThrowRuntimeIf(!exists(redirect_to), "redirect_to path doesn't exist.");
		ThrowCAPIExceptionIf(-1 == pipe(pipefd), "pipe");
		break;
	default:
		break;
	}
}

void srun::Output::dup(int fd)
{
	int target_fd = -1;
	if (pipefd[0] != -1) {
		closefd(pipefd[0]);
		target_fd = pipefd[1];
	} else {
		target_fd = open("/dev/null", O_WRONLY);
	}
	dup2(target_fd, fd);
	closefd(target_fd);
}

void srun::Output::bind(int epollfd)
{
	if (pipefd[0] != -1) {
		if (mode == Request_OutMode_FILE) {
			redirect_fd = open(redirect_to.c_str(), O_WRONLY | O_TRUNC);
			ThrowCAPIExceptionIf(-1 == redirect_fd, "open " + redirect_to);
		}
		closefd(pipefd[1]);
		fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
		struct epoll_event stdout_event;
		stdout_event.events  = EPOLLIN;
		stdout_event.data.fd = pipefd[0];
		int ret  = epoll_ctl(epollfd, EPOLL_CTL_ADD, pipefd[0], &stdout_event);
		ThrowCAPIExceptionIf(-1 == ret, "epoll_ctl");
	} else {
		over = true;
	}
}

void srun::Output::receive(int epollfd)
{
	int  len;
	char buffer[BUFSIZE];
	bool overlimit = false;
	int read_total_size = 0;
	if (mode == Request_OutMode_MSG) {
		while ((len = read(pipefd[0], buffer, BUFSIZE)) > 0) {
			read_total_size += len;
			// std_out cannot bigger than 8 MB
			if (str.size() >= (1 << 23)) {
				overlimit = true;
				break;
			}
			str.append(buffer, len);
		}
	} else if (mode == Request_OutMode_FILE) {
		while ((len = read(pipefd[0], buffer, BUFSIZE)) > 0) {
			read_total_size += len;
			off_t offset = lseek(redirect_fd, 0, SEEK_CUR);
			// std_out outfile cannot bigger than 32 MB
			if (offset == -1 && offset >= (1 << 25)) {
				overlimit = true;
				break;
			}
			write(redirect_fd, buffer, len);
		}
	}
	if (overlimit || read_total_size <= 0) {
		over = true;
		epoll_ctl(epollfd, EPOLL_CTL_DEL, pipefd[0], NULL);
		closefd(pipefd[0]);
		closefd(redirect_fd);
	}
}

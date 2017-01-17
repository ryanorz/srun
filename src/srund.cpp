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

#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <mntent.h>
#include <string.h>
#include <boost/filesystem.hpp>
#include "common/All.hpp"
#include "manager.h"
#include "global.h"
#include "utils.h"
#include "config.h"

using namespace boost::filesystem;
using namespace srun;
using namespace std;

#define MAX_LISTEN_QUEUE  10              // should not bigger than SOMAXCONN (128)

bool srun::process_stop = false;
int srun::lockfd = -1;

static void sigterm_handler(UNUSED int sig, siginfo_t *siginfo, UNUSED void *unused)
{
	if (siginfo->si_pid == 1)
		process_stop = true;
}

void sigchld_hander(__attribute__((unused))int sig)
{
	// do nothing
}

static void daemon_signal_register()
{
	struct sigaction act;
	act.sa_flags = SA_SIGINFO;
	act.sa_sigaction = sigterm_handler;
	sigemptyset(&act.sa_mask);
	sigaction(SIGTERM, &act, 0);

	act.sa_flags = 0;
	act.sa_handler = sigchld_hander;
	sigemptyset(&act.sa_mask);
	sigaction(SIGCLD, &act, 0);
}

static void mount_tmpfs(const string &mountpoint)
{
	if (!is_directory(mountpoint)) {
		create_directories(mountpoint);
	} else {
		struct mntent *ent;
		FILE *stream = setmntent("/proc/mounts", "r");
		ThrowCAPIExceptionIf(stream == NULL, "setmntent(/proc/mounts)");
		LOKI_ON_BLOCK_EXIT(fclose, stream);
		while (NULL != (ent = getmntent(stream))) {
			if (0 == strcmp(ent->mnt_dir, mountpoint.c_str())) {
				ThrowLogicIf(0 != strcmp(ent->mnt_type, "tmpfs"),
					"tmpfs have mount other filesystem.");
				return;
			}
		}
	}
	int ret = mount("tmpfs", mountpoint.c_str(), "tmpfs", MS_RELATIME, NULL);
	ThrowCAPIExceptionIf(-1 == ret, "mount tmpfs");
}

static void umount_tmpfs(const string &mountpoint)
{
	if (-1 == umount(mountpoint.c_str()))
		syslog(LOG_ERR, "umount tmpfs : %m");
}

static void clear_cgroup_setting()
{
	try {
		if (!is_directory(CGMemoryRoot))
			return;
		for (directory_iterator it(CGMemoryRoot); it != directory_iterator(); it++) {
			if (is_directory(it->path()))
				rmdir(it->path().c_str());
		}
	} catch (exception &e) {
		syslog(LOG_ERR, "%s", e.what());
	}
}

static void clear_resource()
{
	umount_tmpfs(tmpfs);
	close(lockfd);
	unlink(lockfile.c_str());
	clear_cgroup_setting();
}

int main() {
	try {
		setlogmask(LOG_UPTO(LOG_NOTICE));
		int ret;
		ThrowCAPIExceptionIf(daemon(0, 1) != 0, "daemon error");
		syslog(LOG_NOTICE, "srund start.");
		write_file(pidfile, to_string(getpid()));
		daemon_signal_register();
		mount_tmpfs(tmpfs);
		lockfd = open(lockfile.c_str(), O_CLOEXEC | O_CREAT | O_TRUNC | O_RDWR);
		ThrowCAPIExceptionIf(-1 == lockfd, "open lockfile");

		Config config;

		//NOTE SOCK_NONBLOCK, SOCK_CLOEXEC can be used since Linux 2.6.27.
		int sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
		ThrowCAPIExceptionIf(sockfd == -1, "create socket");
		unlink(sockfile.c_str());
		struct sockaddr_un server_addr, client_addr;
		server_addr.sun_family = AF_UNIX;
		strcpy(server_addr.sun_path, sockfile.c_str());
		socklen_t server_len = sizeof(server_addr);
		socklen_t client_len = sizeof(client_addr);
		ret = bind(sockfd, (struct sockaddr *)&server_addr, server_len);
		ThrowCAPIExceptionIf(-1 == ret, "socket bind");
		ret = chmod(sockfile.c_str(), S_IRWXU|S_IRWXG|S_IRWXO);
		ThrowCAPIExceptionIf(-1 == ret, "chmod 777 sockfile");
		ret = listen(sockfd, MAX_LISTEN_QUEUE);
		ThrowCAPIExceptionIf(-1 == ret, "listen");
		for (;;) {
			int connection_fd = accept4(sockfd,
						 (struct sockaddr *)&client_addr,
						 (socklen_t *)&client_len,
						 SOCK_CLOEXEC | SOCK_NONBLOCK);
			if (-1 == connection_fd) {
				if (errno == ECONNABORTED) {
					syslog(LOG_WARNING, "A connection has been aborted.");
				} else if (errno == EINTR) {
					pid_t pid;
					int stat;
					while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
						if (!WIFEXITED(stat))
							syslog(LOG_ERR, "%d exit abnormally.", pid);
					}
					if (process_stop) {
						syslog(LOG_NOTICE, "srund stop.");
						break;
					}
				} else {
					throw CAPIException("accept4");
				}
				continue;
			}
			pid_t pid = fork();
			ThrowCAPIExceptionIf(-1 == pid, "fork");
			if (pid == 0) {
				close(sockfd);
				try {
					manager_start(connection_fd, config);
				} catch (exception &e) {
					syslog(LOG_ERR, "%s", e.what());
					manager_destroy();
					return -1;
				}
			} else {
				close(connection_fd);
				syslog(LOG_DEBUG, "Accept connection and create child process %d", pid);
			}
		}
		clear_resource();
		return 0;
	} catch (exception &e) {
		syslog(LOG_ERR, "%s", e.what());
		clear_resource();
		return -1;
	}
}


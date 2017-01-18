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


#include "manager.h"
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/timerfd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sched.h>
#include <pwd.h>
#include <stdlib.h>
#include <boost/filesystem.hpp>

#include "utils.h"
#include "common/All.hpp"
#include "global.h"
#include "srun.pb.h"
#include "output.h"

using namespace srun;
using namespace std;
using namespace boost::filesystem;

#define MAX_EVENTS	20
#define BUFSIZE		1024
#define STACK_SIZE	(1024 * 1024)

enum ProcessStatus {
	NO_PROC,
	PROC_RUNNING,
	PROC_EXIT
};

static int connection = -1;
static pid_t clonepid = 0;
static int epollfd = -1;
static int timerfd = -1;
static Output out;
static Output err;
static int syn_timeout = 1000; // milliseconds
static bool is_syn = false;
static bool result_sendback = false;
static ProcessStatus child_status = NO_PROC;
static Response response;
static Request request;

static void signal_register();
static void wait_child();
static void send_message(const Message& message);
static void recv_message(const Config& config);
static void handle_request(const string &data);
static void set_timeout();
static int execute(__attribute__((unused))void* _message);

static bool exec_over()
{
	return (child_status == PROC_EXIT && out.over && err.over);
}

static void stop_child()
{
	syslog(LOG_DEBUG, "Stop child process if it exists");
	if (child_status == PROC_RUNNING)
		kill(clonepid, SIGTERM);
}

static void clear_clonepid() noexcept
{
	if (clonepid > 0) {
		syslog(LOG_DEBUG, "Clear clonepid resource");
		string clone_file = tmpfs + "/" + to_string(clonepid);
		string leaf = CGMemoryRoot + "/" + to_string(clonepid);
		clonepid = 0;
		flock(lockfd, LOCK_EX);
		unlink(clone_file.c_str());
		flock(lockfd, LOCK_UN);
		if (-1 == rmdir(leaf.c_str()))
			syslog(LOG_ERR, "rmdir %s : %m", leaf.c_str());
	}
}

void srun::manager_destroy() noexcept
{
	syslog(LOG_DEBUG, "Manager destroy");
	if (child_status == PROC_RUNNING) {
		kill(clonepid, SIGTERM);
		sleep(2);
		wait_child();
	}
	closefd(connection);
	closefd(epollfd);
	closefd(timerfd);
	google::protobuf::ShutdownProtobufLibrary();
}

void srun::manager_start(int connection_fd, const Config &config)
{
	connection = connection_fd;
	signal_register();
	epollfd = epoll_create1(EPOLL_CLOEXEC);
	ThrowCAPIExceptionIf(-1 == epollfd, "epoll_create1");
	struct epoll_event conn_event;
	conn_event.events  = EPOLLIN;
	conn_event.data.fd = connection;
	int ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, connection, &conn_event);
	ThrowCAPIExceptionIf(-1 == ret, "epoll_ctl");

	struct epoll_event events[MAX_EVENTS];
	int nfds;
	for (;;) {
		wait_child();

		// Deal with client disconnect and command exit
		if (exec_over()) {
			if (connection == -1 || process_stop) {
				manager_destroy();
				exit(0);
			} else if (!result_sendback) {
				Message message;
				message.set_method(Message_Method_EXE);
				Response *response_ptr = message.mutable_response();
				*response_ptr = response;
				response_ptr->set_outstr(out.str);
				response_ptr->set_errstr(err.str);
				send_message(message);
			}
		} else if (child_status == NO_PROC) {
			if (connection == -1) {
				manager_destroy();
				exit(0);
			}
		}

		if (process_stop)
			stop_child();

		nfds = epoll_wait(epollfd, events, MAX_EVENTS, syn_timeout);
		ThrowCAPIExceptionIf(nfds == -1 && errno != EINTR, "epoll_wait");

		// epoll_wait timeout, need to check client alive
		if (nfds == 0 && !request.backend() && connection != -1) {
			ThrowLogicIf(is_syn, "syn timeout");
			is_syn = true;
			Message syn;
			syn.set_method(Message_Method_SYN);
			send_message(syn);
		}

		for (int i = 0; i < nfds; ++i) {
			if (events[i].data.fd == connection) {
				recv_message(config);
			} else if (events[i].data.fd == out.pipefd[0]) {
				out.receive(epollfd);
			} else if (events[i].data.fd == err.pipefd[0]) {
				err.receive(epollfd);
			} else if (timerfd != -1 && events[i].data.fd == timerfd) {
				stop_child();
				epoll_ctl(epollfd, EPOLL_CTL_DEL, timerfd, NULL);
				closefd(timerfd);
				response.set_stat(Response_State_TIMEOUT);
				syslog(LOG_ERR, "Timeout, kill pid = %d", clonepid);
				for (int i = 0; i < request.args().size(); ++i) {
					syslog(LOG_ERR, "args[%d-%d] = %s",
						(request.args().size() - 1), i, request.args(i).c_str());
				}
			}
		}
	}
}

static void sigterm_handler(UNUSED int sig, siginfo_t *siginfo, UNUSED void *unused)
{
	if (siginfo->si_pid == 1 || siginfo->si_pid == getppid())
		process_stop = true;
}

static void signal_register()
{
	struct sigaction act;
	act.sa_flags = SA_SIGINFO;
	act.sa_sigaction = sigterm_handler;
	sigemptyset(&act.sa_mask);
	sigaction(SIGTERM, &act, 0);

	prctl(PR_SET_PDEATHSIG, SIGTERM);
}

static void wait_child()
{
	int stat;
	pid_t pid;
	while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
		if (clonepid != pid)
			continue;

		syslog(LOG_DEBUG, "Waitpid %d", clonepid);
		child_status = PROC_EXIT;
		if (WIFEXITED(stat)) {
			response.set_stat(Response_State_NORMAL);
			response.set_retval(WEXITSTATUS(stat));
		} else {
			response.set_stat(Response_State_ABNORMAL);
			if (WIFSIGNALED(stat)) {
				syslog(LOG_NOTICE, "Signal(%d) terminate %d", WTERMSIG(stat), clonepid);
			} else {
				syslog(LOG_ERR, "Command exit abnormally.");
				for (int i = 0; i < request.args_size(); ++i)
					syslog(LOG_ERR, " args[%d-%d] = %s",
						(request.args_size() - 1), i, request.args(i).c_str());
			}
		}
		clear_clonepid();
	}
}

static void send_message(const Message& message)
{
	syslog(LOG_DEBUG, "Send message, method : %d", message.method());
	string data;
	ThrowRuntimeIf(!message.SerializeToString(&data),
		"Message SerializeToString failed"
	);
	ThrowRuntimeIf(data.empty(), "Message SerializeToString empty.");
	int len = write(connection, data.c_str(), data.size());
	ThrowCAPIExceptionIf(len != (int)data.size(), "SendMessage");
	if (message.method() != Message_Method_SYN &&
	    message.method() != Message_Method_ACK)
		result_sendback = true;
}

static void recv_message(const Config &config)
{
	string data;
	char buffer[BUFSIZE];
	int len;
	while ((len = read(connection, buffer, BUFSIZE)) > 0)
		data.append(buffer, len);

	if (data.empty()) {
		syslog(LOG_DEBUG, "Connection is closed");
		syn_timeout = -1;
		epoll_ctl(epollfd, EPOLL_CTL_DEL, connection, NULL);
		closefd(connection);
		if (!request.backend() && !result_sendback) {
			syslog(LOG_ERR, "Read from client failed.");
			stop_child();
		}
		return;
	}
	Message message;
	ThrowRuntimeIf(!message.ParseFromString(data),
		"Message ParseFromString failed"
	);
	syslog(LOG_DEBUG, "Receive message, method : %d", message.method());
	Message sendback;
	switch (message.method()) {
	case Message_Method_SYN:
		sendback.set_method(Message_Method_ACK);
		send_message(message);
		break;
	case Message_Method_ACK:
		is_syn = false;
		break;
	case Message_Method_EXE:
		request = message.request();
		syslog(LOG_DEBUG, "Request confgroup : %s", request.confgroup().c_str());
		config.loadRequest(request);
		handle_request(data);
		break;
	case Message_Method_LIST:
		request.set_backend(false);
		try {
			flock(lockfd, LOCK_SH);
			for (directory_iterator it(tmpfs); it != directory_iterator(); it++) {
				string subpath = it->path().string();
				string tmpdata;
				Message proc_msg;
				read_file(subpath, tmpdata);
				ThrowRuntimeIf(!proc_msg.ParseFromString(tmpdata),
					"Message ParseFromString failed"
				);
				Runproc *proc = sendback.add_runlist();
				proc->set_pid(atoi(basename(subpath).c_str()));
				proc->set_since(last_write_time(it->path()));
				Request *proc_req = proc->mutable_request();
				*proc_req = proc_msg.request();
			}
			flock(lockfd, LOCK_UN);
		} catch (exception &e) {
			flock(lockfd, LOCK_UN);
			throw e;
		}
		sendback.set_method(Message_Method_LIST);
		send_message(sendback);
		break;
	case Message_Method_STOP:
		for (auto proc : message.runlist()) {
			string tmpfs_pid = tmpfs + "/" + to_string(proc.pid());
			try {
				flock(lockfd, LOCK_SH);
				if (exists(tmpfs_pid)) {
					syslog(LOG_NOTICE, "Client stop %d", proc.pid());
					kill(proc.pid(), SIGTERM);
				}
				flock(lockfd, LOCK_UN);
			} catch (exception &e) {
				flock(lockfd, LOCK_UN);
				throw e;
			}
		}
		sendback.set_method(Message_Method_STOP);
		send_message(sendback);
		break;
	default:
		break;
	}
}

static void set_timeout()
{
	int timeout = request.timeout();
	if (timeout < 0) {
		syslog(LOG_NOTICE, "Request no timeout, cmd = %s",
		       request.args(0).c_str());
		return;
	}
	///NOTE timerfd_create since Linux 2.6.27
	timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
	ThrowCAPIExceptionIf(-1 == timerfd, "timerfd_create");
	if (timeout == 0) {
		if (request.backend() == true)
			timeout = 7200;
		else
			timeout = 600;
	}
	syslog(LOG_DEBUG, "Set timeout %d", timeout);
	itimerspec new_timer, old_timer;
	new_timer.it_interval.tv_sec  = timeout;
	new_timer.it_interval.tv_nsec = 0;
	new_timer.it_value.tv_sec     = timeout;
	new_timer.it_value.tv_nsec    = 0;
	int ret = timerfd_settime(timerfd, 0, &new_timer, &old_timer);
	ThrowCAPIExceptionIf(-1 == ret, "timerfd_settime");
	struct epoll_event timeout_event;
	timeout_event.events  = EPOLLIN;
	timeout_event.data.fd = timerfd;
	ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, timerfd, &timeout_event);
	ThrowCAPIExceptionIf(-1 == ret, "epoll_ctl");
}

static bool find_command()
{
	if (request.args_size() == 0) {
		return false;
	} else {
		string cmd = request.args(0);
		if (cmd.empty()) {
			return false;
		} else if (cmd.at(0) == '/') {
			if (!exists(cmd) || is_directory(cmd))
				return false;
			else
				return true;
		} else {
			for (int k = 0; k < (int)ARRAY_SIZE(find_paths); ++k) {
				string abs = find_paths[k] + cmd;
				if (exists(abs) && !is_directory(abs)) {
					*(request.mutable_args()->begin()) = abs;
					return true;
				}
			}
			return false;
		}
	}
}


static void handle_request(const string &data)
{
	if (clonepid > 0) {
		syslog(LOG_WARNING, "Only can execute one command");
		return;
	}

	if (request.backend())
		syn_timeout = -1;

	if (!find_command()) {
		Message message;
		message.set_method(Message_Method_EXE);
		Response *response_ptr = message.mutable_response();
		response_ptr->set_stat(Response_State_BADARGS);
		send_message(message);
		return;
	}
	syslog(LOG_DEBUG, "Command : %s", request.args(0).c_str());
	syslog(LOG_DEBUG, "Outmode : %d, Errmode : %d", request.outmode(), request.errmode());
	syslog(LOG_DEBUG, "Network : %d, Backend : %d", request.network(), request.backend());

	out.load(request.outmode(), request.outfile(), request.backend());
	err.load(request.errmode(), request.errfile(), request.backend());

	/**
	* CLONE_NEWIPC, CLONE_NEWNET, CLONE_NEWNS, CLONE_NEWPID, or CLONE_NEWUTS
	* was specified by an unprivileged process (process without CAP_SYS_ADMIN).
	* CLONE_NEWNET (since Linux 2.6.24)
	*/
	char clone_stack[STACK_SIZE];
	char *stack_top = clone_stack + STACK_SIZE;
	int clone_flags = SIGCHLD;
	if (!request.network())
		clone_flags |= CLONE_NEWNET;
	clonepid = clone(execute, stack_top,  clone_flags, NULL);
	ThrowCAPIExceptionIf(clonepid == -1, "clone");
	child_status = PROC_RUNNING;

	syslog(LOG_DEBUG, "Clone a child with pid %d", clonepid);
	const string pidstr = to_string(clonepid);
	const string leaf = CGMemoryRoot + "/" + pidstr;
	ThrowRuntimeIf(!is_directory(leaf) && !create_directories(leaf),
			"Create " + leaf + " failed.");
	write_file(leaf + "/memory.limit_in_bytes", "300M");
	string memswlimit = leaf + "/memory.memsw.limit_in_bytes";
	if (exists(memswlimit))
		write_file(memswlimit, "1G");
	write_file(leaf + "/cgroup.procs", pidstr);
	write_file(leaf + "/notify_on_release", "1");

	try {
		flock(lockfd, LOCK_EX);
		write_file(tmpfs + "/" + to_string(clonepid), data);
		flock(lockfd, LOCK_UN);
	} catch (exception &e) {
		flock(lockfd, LOCK_UN);
		throw e;
	}

	out.bind(epollfd);
	err.bind(epollfd);
	set_timeout();

	if (request.backend()) {
		Message message;
		message.set_method(Message_Method_EXE);
		Response *response_ptr = message.mutable_response();
		response_ptr->set_stat(Response_State_NORMAL);
		response_ptr->set_retval(0);
		send_message(message);
	}
}

static int execute(__attribute__((unused))void* _message)
{
	try {
		out.dup(1);
		err.dup(2);

		struct passwd *pwd = NULL;
		if (request.user().empty())
			request.set_user("nobody");
		pwd = getpwnam(request.user().c_str());
		ThrowCAPIExceptionIf(pwd == NULL, "Get user error");
		ThrowCAPIExceptionIf(0 != setgid(pwd->pw_gid), "setgid");
		ThrowCAPIExceptionIf(0 != setuid(pwd->pw_uid), "setgid");

		if (!request.chroot().empty()) {
			int ret = chdir(request.chroot().c_str());
			ThrowCAPIExceptionIf(0 != ret, "chroot " + request.chroot());
		}

		char *argv[request.args().size() + 1];
		for (size_t i = 0; i < (size_t)request.args().size(); ++i)
			argv[i] = const_cast<char *>(request.args(i).c_str());
		argv[request.args().size()] = NULL;

		/*
		* PR_SET_PDEATHSIG (since Linux 2.1.57)
		*    Set  the  parent  death  signal  of  the  calling process to arg2 (either a signal value in the range 1..maxsig, or 0 to clear).
		*    This is the signal that the calling process will get when its parent dies.
		*    This value is cleared for the child of a fork(2) and
		*    (since Linux 2.4.36 / 2.6.23) when executing a set-user-ID or set-group-ID binary, or a binary that has associated capabilities (see capabilities(7)).
		*    This value is preserved across execve(2).
		*/
		prctl(PR_SET_PDEATHSIG, SIGTERM);

		execv(argv[0], argv);
		syslog(LOG_ERR, "execv(%s): %m", request.args(0).c_str());
		abort();
	} catch (exception &e) {
		syslog(LOG_ERR, "%s", e.what());
		exit(-1);
	}
}

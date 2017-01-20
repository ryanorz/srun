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
#include <getopt.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <sstream>

using namespace std;
using namespace srun;

void do_help()
{
	cout << "Usage: srunctl list \n";
	cout << "       srunctl stop PIDS\n";
	cout << "       srunctl [OPTIONS] -c CMDARGS" << endl << endl;
	cout << "\t-o outfile   Redirect stdout to outfile" << endl;
	cout << "\t-e errfile   Redirect stdout to outfile" << endl;
	cout << "\t-t timeout   Set command execute timeout" << endl;
	cout << "\t-u user      Set command execute user" << endl;
	cout << "\t-r chroot    Set command execute chroot" << endl;
	cout << "\t-m model     Set model name( group name of config )" << endl;
	cout << "\t-n           Use network" << endl;
	cout << "\t-q           Quiet mode, do not care output and errput" << endl;
	cout << "\t-b           Process execute at backend" << endl;
	cout << "\t-c CMDARGS   Command and arguments" << endl;
}

string time_diff(time_t since, time_t now)
{
	time_t diff = now - since;
	int seconds = diff % 60;
	int mins = diff / 60 % 60;
	int hours = diff / 60 / 60;
	std::ostringstream os;
	os << hours << ":" << mins << ":" << seconds;
	return os.str();
}

int main(int argc, char *argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	if (argc < 2) {
		do_help();
		return -1;

	}

	Srunc client;
	if (0 == strcmp("list", argv[1])) {
		google::protobuf::RepeatedPtrField< Runproc > runlist;
		if (!client.list(runlist)) {
			cerr << "Error. Please see the log." << endl;
			return -1;
		}
		time_t now = time(NULL);
		cout << "PID\tRUNTIME\tNETWORK\tBACKEND\tTIMEOUT\tCMD" << endl;
		for (auto proc : runlist) {
			cout << proc.pid() << "\t"
			<< time_diff(proc.since(), now) << "\t"
			<< proc.request().network() << "\t"
			<< proc.request().backend() << "\t"
			<< proc.request().timeout() << "\t";
			for (auto arg : proc.request().args())
				cout << arg.c_str() << " ";
			cout << endl;
		}
		return 0;
	} else if (0 == strcmp("stop", argv[1])) {
		if (argc == 2) {
			do_help();
			return -1;
		}
		vector<pid_t> pids;
		for (int i = 2; i < argc; ++i) {
			int pid = atoi(argv[i]);
			if (pid > 0)
				pids.push_back(pid);
		}
		if (!client.stop(pids)) {
			cerr << "Error. Please see the log." << endl;
			return -1;
		}
		return 0;
	}

	// execute command
	Request request;
	Response response;
	request.set_timeout(0);
	request.set_outmode(Request_OutMode_MSG);
	request.set_errmode(Request_OutMode_MSG);

	bool quiet = false;
	umask(0);
	while (1) {
		bool command_start = false;
		int opt = getopt(argc, argv, "o:e:qnbt:u:r:m:c");
		if (opt == -1)
			break;

		switch(opt) {
		case 'm':
			request.set_model(optarg);
			break;
		case 't':
			request.set_timeout(atoi(optarg));
			break;
		case 'u':
			request.set_user(optarg);
			break;
		case 'r':
			request.set_chroot(optarg);
			break;
		case 'b':
			request.set_backend(true);
			break;
		case 'q':
			quiet = true;
			if (request.outmode() == Request_OutMode_MSG)
				request.set_outmode(Request_OutMode_IGN);
			if (request.errmode() == Request_OutMode_MSG)
				request.set_errmode(Request_OutMode_IGN);
			break;
		case 'n':
			request.set_network(true);
			break;
		case 'o':
			request.set_outmode(Request_OutMode_FILE);
			request.set_outfile(optarg);
			break;
		case 'e':
			request.set_errmode(Request_OutMode_FILE);
			request.set_errfile(optarg);
			break;
		case 'c':
			command_start = true;
			break;
		default:
			do_help();
			exit(-1);
		}
		if (command_start)
			break;
	}

	if (optind >= argc) {
		do_help();
		return -1;
	}
	for (int i = optind; i < argc; ++i)
		request.add_args(argv[i]);

	if (!client.run(request, response)) {
		cerr << "Error. Please see the log." << endl;
		return -1;
	}

	if (quiet) {
		switch (response.stat()) {
			case Response_State_NORMAL:
				return response.retval();
			case Response_State_ABNORMAL:
				cerr << "Abnormal" << endl;
				return -1;
			case Response_State_TIMEOUT:
				cerr << "Timeout" << endl;
				return -1;
			case Response_State_BADARGS:
				cerr << "ArgsError" << endl;
				return -1;
			default:
				cerr << "Unknown response stat" << endl;
				return -1;
		}
	}

	switch (response.stat()) {
	case Response_State_NORMAL:
		if (!response.outstr().empty())
			cout << response.outstr() << endl;
		if (!response.errstr().empty())
			cout << response.errstr() << endl;
		return response.retval();
	case Response_State_ABNORMAL:
		cout << "stat: Abnormal" << endl;
		return -1;
	case Response_State_TIMEOUT:
		cout << "stat: Timeout" << endl;
		return -1;
	case Response_State_BADARGS:
		cout << "stat: ArgsError" << endl;
		return -1;
	default:
		cerr << "Unknown response stat" << endl;
		return -1;
	}
}


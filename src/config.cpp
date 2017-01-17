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

#include "config.h"
#include <syslog.h>
#include <iostream>
#include <boost/filesystem.hpp>
#include "common/All.hpp"
#include "global.h"

using namespace srun;
using namespace boost::filesystem;
using namespace std;

srun::Config::Config()
{
	ThrowLogicIf(!exists(confpath), "Default config file lost");
	if (exists(confpath)) {
		GKeyFile *keyfile = g_key_file_new();
		GError *error = NULL;
		if (!g_key_file_load_from_file (keyfile, confpath.c_str(), G_KEY_FILE_NONE, &error)) {
			syslog(LOG_ERR, "g_key_file_load_from_file(%s): %s",
			       confpath.c_str(), error->message);
			g_error_free(error);
			g_key_file_free(keyfile);
		} else {
			keyfiles.insert(pair<string, GKeyFile*>(confpath, keyfile));
		}
	}
	if (!is_directory(confdir))
		return;
	for (directory_iterator it(confdir); it != directory_iterator(); it++) {
		if (!is_regular_file(it->path()))
			continue;
		GKeyFile *keyfile = g_key_file_new();
		GError *error = NULL;
		if (!g_key_file_load_from_file (keyfile, it->path().c_str(), G_KEY_FILE_NONE, &error)) {
			syslog(LOG_WARNING, "g_key_file_load_from_file(%s): %s",
			       it->path().c_str(), error->message);
			g_error_free(error);
			g_key_file_free(keyfile);
		} else {
			keyfiles.insert(pair<string, GKeyFile*>(it->path().string(), keyfile));
		}
	}
}

srun::Config::~Config() noexcept
{
	for (auto keyfile: keyfiles)
		g_key_file_free(keyfile.second);
	keyfiles.clear();
}

void srun::Config::fillRequest(GKeyFile* keyfile, const string& group, Request& request) const noexcept
{
	GError *error = NULL;
	int timeout = g_key_file_get_integer(keyfile, group.c_str(), "timeout", &error);
	if (error)
		g_error_free(error);
	else
		request.set_timeout(timeout);

	gchar *gstr;
	gstr = g_key_file_get_string(keyfile, group.c_str(), "user", &error);
	if (error) {
		g_error_free(error);
	} else {
		request.set_user(gstr);
		g_free(gstr);
	}

	gstr = g_key_file_get_string(keyfile, group.c_str(), "chroot", &error);
	if (error) {
		g_error_free(error);
	} else {
		request.set_chroot(gstr);
		g_free(gstr);
	}

	gstr = g_key_file_get_string(keyfile, group.c_str(), "outmode", &error);
	if (error) {
		g_error_free(error);
	} else {
		if (0 == strcmp(gstr, "IGN"))
			request.set_outmode(Request_OutMode_IGN);
		else if (0 == strcmp(gstr, "MSG"))
			request.set_outmode(Request_OutMode_MSG);
		else if (0 == strcmp(gstr, "FILE"))
			request.set_outmode(Request_OutMode_FILE);
		g_free(gstr);
	}

	gstr = g_key_file_get_string(keyfile, group.c_str(), "errmode", &error);
	if (error) {
		g_error_free(error);
	} else {
		if (0 == strcmp(gstr, "IGN"))
			request.set_errmode(Request_OutMode_IGN);
		else if (0 == strcmp(gstr, "MSG"))
			request.set_errmode(Request_OutMode_MSG);
		else if (0 == strcmp(gstr, "FILE"))
			request.set_errmode(Request_OutMode_FILE);
		g_free(gstr);
	}

	gstr = g_key_file_get_string(keyfile, group.c_str(), "outfile", &error);
	if (error) {
		g_error_free(error);
	} else {
		request.set_outfile(gstr);
		g_free(gstr);
	}

	gstr = g_key_file_get_string(keyfile, group.c_str(), "errfile", &error);
	if (error) {
		g_error_free(error);
	} else {
		request.set_errfile(gstr);
		g_free(gstr);
	}

	gboolean network = g_key_file_get_boolean(keyfile, group.c_str(), "network", &error);
	if (error)
		g_error_free(error);
	else
		request.set_network(network);

	gboolean backend = g_key_file_get_boolean(keyfile, group.c_str(), "backend", &error);
	if (error)
		g_error_free(error);
	else
		request.set_backend(backend);
}

bool srun::Config::loadRequest(Request& request) const noexcept
{
	if (request.confgroup().empty())
		return true;
	string group = request.confgroup();
	for (auto keyfile: keyfiles) {
		if (g_key_file_has_group(keyfile.second, group.c_str())) {
			fillRequest(keyfile.second, group, request);
			return true;
		}
	}
	syslog(LOG_ERR, "Keyfiles doesn't have group %s", group.c_str());
	return false;
}

bool srun::Config::loadRequest(const string& file, Request& request) const noexcept
{
	if (request.confgroup().empty())
		return true;
	string group = request.confgroup();

	GError *error = NULL;
	GKeyFile *keyfile = g_key_file_new();
	if (!g_key_file_load_from_file (keyfile, file.c_str(), G_KEY_FILE_NONE, &error)) {
		syslog(LOG_WARNING, "g_key_file_load_from_file(%s): %s",
		       file.c_str(), error->message);
		g_error_free(error);
		g_key_file_free(keyfile);
		return false;
	} else {
		fillRequest(keyfile, group, request);
		g_key_file_free(keyfile);
		return true;
	}
}

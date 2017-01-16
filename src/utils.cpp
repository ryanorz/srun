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

#include "utils.h"
#include <unistd.h>
#include <fcntl.h>
#include <boost/filesystem.hpp>
#include "common/All.hpp"

using namespace srun;
using namespace boost::filesystem;

void srun::closefd(int &fd) noexcept
{
	if (fd != -1) {
		close(fd);
		fd = -1;
	}
}

void srun::write_file(const string &path, const string &data)
{
	FILE *stream = fopen(path.c_str(), "w");
	srun::ThrowCAPIExceptionIf(stream == nullptr, "fopen, w, " + path);
	LOKI_ON_BLOCK_EXIT(fclose, stream);
	size_t ret = fwrite(data.c_str(), 1, data.size(), stream);
	srun::ThrowCAPIExceptionIf(ret != data.size(), "fwrite");
}

void srun::read_file(const string &path, string &data)
{
	FILE *stream = fopen(path.c_str(), "r");
	srun::ThrowCAPIExceptionIf(stream == nullptr, "fopen, r, " + path);
	LOKI_ON_BLOCK_EXIT(fclose, stream);
	fseek(stream, 0, SEEK_END);
	long int size = ftell(stream);
	data.resize(size);
	rewind(stream);
	fread(&data[0], 1, data.size(), stream);
	srun::ThrowCAPIExceptionIf(0 != ferror(stream), "fread");
}

string srun::unique_path_in_dir(const string &dir)
{
	srun::ThrowLogicIf(!is_directory(dir),
			   "unique_path_in_dir::is_directory(" + dir + ")");
	string unique;
	if (dir[dir.size() - 1] != '/')
		unique = dir + "/%%%%%%";
	else
		unique = dir + "%%%%%%";
	return unique_path(unique).string();
}

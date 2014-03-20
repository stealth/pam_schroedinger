/*
 * This file is part of pam_schroedinger
 *
 * (C) 2014 by Sebastian Krahmer,
 *             sebastian [dot] krahmer [at] gmail [dot] com
 *
 * pam_schroedinger is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pam_schroedinger is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with pam_schroedinger. If not, see <http://www.gnu.org/licenses/>.
 */

#include "pty.h"
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <string>
#include <cstring>

using namespace std;

pty::pty(const pty &rhs)
{
	if (this == &rhs)
		return;
	_slave = dup(rhs._slave); _master = dup(rhs._master);
	m = rhs.m; s = rhs.s;
}

pty &pty::operator=(const pty &rhs)
{
	if (this == &rhs)
		return *this;
	_slave = dup(rhs._slave); _master = dup(rhs._master);
	m = rhs.m; s = rhs.s;
	return *this;
}


// Open master+slave terminal
// returns 0 on sucess, -1 on failure
int pty::open()
{
	char ptys[] = "/dev/ptyXY";
	const char *it1 = "pqrstuvwxyzPQRST",
	           *it2 = "0123456789abcdef";

	for (; *it1 != 0; ++it1) {
		ptys[8] = *it1;
		for (; *it2 != 0; ++it2) {
			ptys[9] = *it2;

			// do master-part
			if ((_master = ::open(ptys, O_RDWR|O_NOCTTY)) < 0) {
				if (errno == ENOENT) {
					serr = strerror(errno);
					return -1;
				} else
					continue;
			}
			m = ptys;
			ptys[5] = 't';

			// master finished. Do slave-part.
			if ((_slave = ::open(ptys, O_RDWR|O_NOCTTY)) < 0) {
				::close(_master);
				serr = strerror(errno);
				return -1;
			}
			s = ptys;
			return 0;
		}
	}

	// out of terminals
	serr = "Dance the funky chicken (or use Unix98 pty's).";
	return -1;
}


int pty::close()
{
	::close(_master); _master = -1;
	::close(_slave); _slave = -1;
	return 0;
}

int pty::grant(uid_t u, gid_t g, mode_t mode)
{
	if (chown(m.c_str(), u, g) < 0 || chown(s.c_str(), u, g) < 0) {
		serr = strerror(errno);
		return -1;
	}
	mode_t mask = umask(0);
	if (chmod(m.c_str(), mode) < 0 || chmod(s.c_str(), mode) < 0) {
		serr = strerror(errno);
		umask(mask);
		return -1;
	}
	umask(mask);
	return 0;
}

const char *pty::why()
{
	return serr.c_str();
}


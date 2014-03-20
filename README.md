pam_schroedinger
================

_pam_schroedinger_ prevents from dicitionary/brute-force attacks aganinst PAM accounts
by only returning PAM_SUCCESS if there was no previous login or attempt
within a certain timeframe. In a common scenario, users do not authenticate
more than once in a second. Everything else looks like a brute force.
pam_schroedinger prevents PAM accounts from dictionary attacks much better
than a sleep-based delay hardcoded in the authentication mechanism, as used
today in su or sudo for example.
The attacker will see no delay in his attack, but he will not see which
login tokens succeeds, even if he tried the right one.

Installation
------------

Just type

    make

to build it. Then

    # mkdir -m 0755 /var/run/schroedinger

and:

    # cp pam_schroedinger.so /lib64/security

on a 64bit machine, or:

    # cp pam_schroedinger.so /lib/security

for 32bit architectures. Then add it to the PAM stack by adding it to
_/etc/pam.d/common-auth_ or whatever service you want to protect:

    auth    required        pam_schroedinger.so dir=/var/run/schroedinger delay=1

These are all arguments that can be passed to _pam_schroedinger_ and also their
default values. The delay is in seconds.


Why's that necessary?
---------------------

Todays machines are fast enough to allow 1000's cracks/sec by cleverly
arranging pty's and processes like su or sudo to try a login token, despite
of sleep-delays which just add a constant delay of a few seconds to the overall
attack. This is due to todays computing power even on modest desktop machines
which have Gigs of RAM and multiple cores which allows to run multiple
100 instances of su/sudo in parallel. Just try:

   $ ./enabler -c sudo -n 200 < /usr/share/dict/words

on a core-i5 laptop for example.



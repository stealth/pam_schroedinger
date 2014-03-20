pam_schroedinger
================

pam_schroedinger prevents dicitionary/brute-force attacks aganinst PAM accounts
by only returning PAM_SUCCESS if there was no previous login attempt within a
certain timeframe.

Installation
------------

    # mkdir -m 0755 /var/run/schroedinger

    auth    required        pam_schroedinger.so dir=/var/run/schroedinger delay=1

These are all arguments that can be passed to pam_schroedinger and also their
default values. The delay is in seconds.


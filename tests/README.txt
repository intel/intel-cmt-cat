========================================================================
README for Basic Tests

May 2019
========================================================================

Contents
========
  - Dependencies
  - Virtual Environment
  - Environment File
  - Running tests
  - Additional Parameters
  - Legal Disclaimer

Dependencies
=============
  - python 3.6
  - pip
  - pytest
  - python-pexpect
  - memtester

Virtual Environment
===================
To run tests without installation of additional python packages it is possible
to use virtual environment.

To setup virtual environment:
	make test_env


Environment File
===============
In order to run unit tests, it is required to crerate environment file describing
hardware features. Please, follow schema env.json for full documentation.

The example.json contains example environment file for Intel(R) Xeon(R) Gold 6252N CPU.


Running tests
=============
It is assumed that pqos library will be installed in the system.

Run tests without virtualenv:
	sudo py.test --env=example.json testcases

Run tests using virtualenv:
	sudo bash -c "source test_env/bin/activate && py.test --env=example.json testcases; deactivate"


Additional Parameters
=====================
Custom py.test options:
  --pqos=PQOS           Path to pqos utility
  --rdtset=RDTSET       Path to rdtset utility
  --membw=MEMBW         Path to membw tool
  --env=ENV             Path to environment file
  --iface-msr           MSR interface
  -I, --iface-os        OS interface


Legal Disclaimer
================

THIS SOFTWARE IS PROVIDED BY INTEL"AS IS". NO LICENSE, EXPRESS OR
IMPLIED, BY ESTOPPEL OR OTHERWISE, TO ANY INTELLECTUAL PROPERTY RIGHTS
ARE GRANTED THROUGH USE. EXCEPT AS PROVIDED IN INTEL'S TERMS AND
CONDITIONS OF SALE, INTEL ASSUMES NO LIABILITY WHATSOEVER AND INTEL
DISCLAIMS ANY EXPRESS OR IMPLIED WARRANTY, RELATING TO SALE AND/OR
USE OF INTEL PRODUCTS INCLUDING LIABILITY OR WARRANTIES RELATING TO
FITNESS FOR A PARTICULAR PURPOSE, MERCHANTABILITY, OR INFRINGEMENT
OF ANY PATENT, COPYRIGHT OR OTHER INTELLECTUAL PROPERTY RIGHT.

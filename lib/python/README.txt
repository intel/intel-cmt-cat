========================================================================
README for PQoS Library Python Wrapper

March 2019
========================================================================


Contents
========

- Overview
- Installation
- Running tests
- Legal Disclaimer


Overview
========

This a Python interface for PQoS library. This wrapper requires Python 3.x
and libpqos installed in the system. The package is named 'pqos'.


Installation
============

To install the package run:
        make install

This will install the package using `python -m pip`.

To uninstall the 'pqos' package run:
        make uninstall


Running tests
=============

In order to run unit tests, create coverage report or check coding style
it is required to setup virtual environment first. All of the following commands
will setup it if it has not been created yet. The virtual environment
will be created in test_env/.

To run unit tests:
        make test

After running unit tests, the coverage report can be generated:
        make coverage

To check coding style with pylint run:
        make style

To clear a virtual environment and remove cache files:
        make clean


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

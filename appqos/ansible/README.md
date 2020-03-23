An Ansible Role that installs AppQoS (an API for Intel(R) Resource Director Technology - RDT)
on RHEL/CentOS/Fedora and Debian/Ubuntu Linux distributions.

AppQos Overview
===============
AppQoS is a proof-of-concept software created to demonstrate the use of Intel(R) RDT and Intel(R) Speed Select Technology (SST) to improve Quality of Service(QoS) for applications via partitioning system resources. Memory Bandwidth Allocation (MBA) and Cache Allocation (CAT) are demonstrated from Intel(R) RDT technologies. From Intel(R) SST technologies Base Frequency (BF) and Core Power (CP).
AppQoS allows applications to be assigned  to resource pools. A resource pool is defined by cores, memory bandwidth, cache ways and CPU frequency settings.
In the current version, each pool resource definition is static, i.e. AppQoS does not optimize pool resources at runtime. However, resource pool definition can be changed by the user at any time through REST API through Intel(R) SST BF and Intel(R) SST CP.
AppQoS allows resource pools to have asymmetric core frequency configurations. Pool resource allocation could depend on the pool’s priority.
AppQoS provides a simple, local, REST API management interface secured with HTTPS and "Basic HTTP Auth". REST API management interface allows the user to manage Apps and Pools. An App controlled by AppQoS could be a process, a container or a VNF.
AppQoS is reference code written in Python. It is fully configurable and can be easily modified to suit other use cases and allow remote management.

Dependencies
===============
Intel(R) RDT CAT and MBA configuration is done via ["libpqos"](https://github.com/intel/intel-cmt-cat) library and
Intel(R) SST BF and CP configuration via external ["Intel pwr"](https://github.com/intel/CommsPowerManagement) library.
More information can be found on [https://github.com/intel/intel-cmt-cat/tree/master/appqos](https://github.com/intel/intel-cmt-cat/tree/master/appqos)

CONFIGURATION
=============
AppQoS supports pools of resources, i.e. Pools. In each Pool one or more application can be deployed, where application is one or more PIDs.

Cache Allocation, Memory Bandwidth Allocation (via CAT and MBA), Base Frequency, Core Power (via SST BF and CP) and affinity are configured by AppQoS on per core basis.
All configurations are pre-defined in appqos.conf file which is "Read-Only" by default.
No runtime changes are saved in the configuration. However, user can make changes on runtime using REST API.
Not every platform supports all mentioned technologies so the default configuration includes only minimum number of settings that would work across most of the platforms.
Users can make changes in the configuration file and tune it for specific hardware platform. Example Intel(R) Xeon(R) 2nd Generation SP is available at Appqos.conf

Apart from installing configuration files, this Ansible script also:
-	Installs required packages and dependencies including pqos utility (RDT tool)
-	Generates self-signed certificate for SSL/TLS i.e. appqos.key and appqos.crt
-	Creates appqos configuration file with minimal resources (mentioned above)
-	Start REST-API flask framework in the background
-	Destroy.yml playbook to stop/terminate REST-API services running in the background

Role Variables
==============
All variables are stored in main.yml file under roles/appqos/default directory.
Based on user needs it can be changed.

Requirements
============
Server with Speed Select - Base Frequency functionality (e.g Intel(R) Xeon(R) 5218N / 6230N / 6252N )
Linux* kernel >= 5.1
Python >= 3.6
Ansible* >= 2.5
msr-tools installed

Further information
==================
[Intel(R) Resource Directory Technology](https://www.intel.com/content/www/us/en/architecture-and-technology/resource-director-technology.html)
[Intel(R) Speed Select - Base frequency (SST-BF)](https://github.com/intel/CommsPowerManagement/blob/master/sst_bf.md)
[Intel(R) SST-BF configuration Python script](https://github.com/intel/CommsPowerManagement/blob/master/sst_bf.py)

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

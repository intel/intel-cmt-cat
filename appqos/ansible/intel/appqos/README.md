# README for Ansible Collection - intel.appqos
April 2020

## CONTENTS
- Introduction
- AppQoS Overview
- Dependencies
- Configuration
- Role Variables
- Example Playbook
- Requirements
- Further Information
- Legal Disclaimer

## INTRODUCTION
An Ansible Collection with single role that installs AppQoS,
an API for Intel(R) Resource Director Technology (RDT)
and Intel(R) Speed Select Technology (SST),
on RHEL/CentOS/Fedora and Debian/Ubuntu Linux distributions.


## APPQOS OVERVIEW
AppQoS is a proof-of-concept software created to demonstrate the use of
Intel(R) RDT and Intel(R) SST to improve Quality of Service (QoS) for
applications via partitioning system resources.

More information can be found in AppQoS README file available at:
https://github.com/intel/intel-cmt-cat/tree/master/appqos/README


## DEPENDENCIES
Intel(R) RDT CAT and MBA configuration is done via "libpqos" library:
https://github.com/intel/intel-cmt-cat

Intel(R) SST BF and CP configuration via external "Intel pwr" library:
https://github.com/intel/CommsPowerManagement

More information can be found in AppQoS README file available at:
https://github.com/intel/intel-cmt-cat/tree/master/appqos/README


## CONFIGURATION
AppQoS supports pools of resources, i.e. Pools. In each Pool one or more
application can be deployed, where application is one or more PIDs.

Cache Allocation, Memory Bandwidth Allocation (via CAT and MBA),
Base Frequency, Core Power (via SST BF and CP) and affinity are configured
by AppQoS on per core basis.

All configurations are pre-defined in appqos.conf file which is
"Read-Only" by default. No runtime changes are saved in the configuration file.
However, user can make changes on runtime using REST API.

Not every platform supports all mentioned technologies so, the default
configuration includes only minimum number of settings that would work across
most of the platforms. Users can make changes in the configuration file and
tune it for specific hardware platform.
Example Intel(R) Xeon(R) 2nd Generation SP is available in appqos.conf

Apart from installing configuration files, this Ansible script also:
- Installs required packages and dependencies including libpqos
- Generates self-signed certificate for SSL/TLS i.e. appqos.key and appqos.crt
- Creates appqos configuration file with minimal resources (mentioned above)
- Start AppQoS in the background
- Stop/terminate AppQoS running in the background


## ROLE VARIABLES
All variables are stored in main.yml file under role's "default" directory.
Based on user needs it can be changed.

"state" variable controls action of the role:
- "present" (default) to deploy
- "stopped" to stop AppQoS instance


## EXAMPLE PLAYBOOK
Playbook to deploy AppQoS:
```
- name: Deploy AppQoS
  hosts: compute
  roles:
    - role: intel.appqos.appqos
      state: present
```

Playbook to stop AppQoS:
```
- name: Stop AppQoS
  hosts: compute
  roles:
    - role: intel.appqos.appqos
      state: stopped
```

## REQUIREMENTS
- Python >= 3.6
- Ansible* >= 2.5
- msr-tools installed


## FURTHER INFORMATION
Intel(R) Resource Directory Technology:
https://www.intel.com/content/www/us/en/architecture-and-technology/resource-director-technology.html

Intel(R) Speed Select - Base frequency (SST-BF):
https://github.com/intel/CommsPowerManagement/blob/master/sst_bf.md

Intel(R) SST-BF configuration Python script:
https://github.com/intel/CommsPowerManagement/blob/master/sst_bf.py


## LEGAL DISCLAIMER
THIS SOFTWARE IS PROVIDED BY INTEL"AS IS". NO LICENSE, EXPRESS OR
IMPLIED, BY ESTOPPEL OR OTHERWISE, TO ANY INTELLECTUAL PROPERTY RIGHTS
ARE GRANTED THROUGH USE. EXCEPT AS PROVIDED IN INTEL'S TERMS AND
CONDITIONS OF SALE, INTEL ASSUMES NO LIABILITY WHATSOEVER AND INTEL
DISCLAIMS ANY EXPRESS OR IMPLIED WARRANTY, RELATING TO SALE AND/OR
USE OF INTEL PRODUCTS INCLUDING LIABILITY OR WARRANTIES RELATING TO
FITNESS FOR A PARTICULAR PURPOSE, MERCHANTABILITY, OR INFRINGEMENT
OF ANY PATENT, COPYRIGHT OR OTHER INTELLECTUAL PROPERTY RIGHT.

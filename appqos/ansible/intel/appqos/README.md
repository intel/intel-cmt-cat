# README for Ansible Collection - intel.appqos
May 2021

## CONTENTS
- Introduction
- App QoS Overview
- Dependencies
- Configuration
- Role Variables
- Example Playbook
- Requirements
- Further Information
- Legal Disclaimer

## INTRODUCTION
An Ansible Collection with single role that installs
Application Quality of Service (App QoS) an API for
Intel(R) Resource Director Technology (RDT)
and Intel(R) Speed Select Technology (SST),
on RHEL/CentOS/Fedora and Debian/Ubuntu Linux distributions.


## APP QOS OVERVIEW
App QoS is a proof-of-concept software created to demonstrate the use of
Intel(R) RDT and Intel(R) SST to improve Quality of Service (QoS) for
applications via partitioning system resources.

More information can be found in App QoS README file available at:
https://github.com/intel/intel-cmt-cat/tree/master/appqos/README


## DEPENDENCIES
Intel(R) RDT CAT and MBA configuration is done via "libpqos" library:
https://github.com/intel/intel-cmt-cat

Intel(R) SST BF and CP configuration via external "Intel pwr" library:
https://github.com/intel/CommsPowerManagement

More information can be found in App QoS README file available at:
https://github.com/intel/intel-cmt-cat/tree/master/appqos/README


## CONFIGURATION
App QoS supports pools of resources, i.e. Pools. In each Pool one or more
application can be deployed, where application is one or more PIDs.

Cache Allocation, Memory Bandwidth Allocation (via CAT and MBA),
Base Frequency, Core Power (via SST BF and CP) and affinity are configured
by App QoS on per core basis.

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
- Generates mTLS certificates
- Creates appqos.conf configuration file with minimal resources (mentioned above)
- Start App QoS in the background
- Stop/terminate App QoS running in the background

It is assumed that intel.appqos.appqos ansible role will be ran directly as root user.

## ROLE VARIABLES
All variables are stored in main.yml file under role's "default" directory.
Based on user needs it can be changed.

"state" variable controls action of the role:
- "present" (default) to deploy
- "stopped" to stop App QoS instance


## EXAMPLE PLAYBOOK
Playbook to deploy App QoS:
```
- name: Deploy App QoS
  hosts: compute
  roles:
    - role: intel.appqos.appqos
      state: present
```

Playbook to stop App QoS:
```
- name: Stop App QoS
  hosts: compute
  roles:
    - role: intel.appqos.appqos
      state: stopped
```

## USAGE EXAMPLES

1. Download intel.appqos.appqos role from Ansible-galaxy

$ ansible-galaxy collection install intel.appqos

2. Go to /root/.ansible/collections/ansible_collections/intel/appqos/playbooks/ directory

$ cd /root/.ansible/collections/ansible_collections/intel/appqos/playbooks/

3. Run ansible-playbook command for deploying App QoS (on localhost by default)

$ ansible-playbook -i inventory/hosts deploy.yml --extra-vars "cert_dir=/CERT_DIR"
... where 'CERT_DIR' is a directory where 'ca.crt', 'appqos.crt' and 'appqos.key'
are located

Ansible scripts need CA certificate and a App QoS certificate located in a given
directory - those files will be copied to App QoS 'ca/' sub-directory in place where
App QoS has been installed. More info about certificate for the App QoS can be
found at https://github.com/intel/intel-cmt-cat/blob/master/appqos/README in
section 'APP QOS AND CLIENT CERTIFICATE'.

By default, App QoS is installed in '/opt/intel/appqos_workspace/appqos/'. At the
end of the playbook, App QoS should be up and running, listening on port 5000.
Note that App QoS server supports one client at a time.

To stop the App QoS service, launch the 'stop.yml' playbook:

$ ansible-playbook -i inventory/hosts stop.yml


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

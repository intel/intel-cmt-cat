################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2023 Intel Corporation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
################################################################################

"""
Module config
"""

from collections import UserDict

from appqos import caps
from appqos.pqos_api import PQOS_API

class Config(UserDict):
#pylint: disable=too-many-public-methods
    """
    Configuration and helper functions
    """

    def get_pool_attr(self, attr, pool_id):
        """
        Get specific attribute from config

        Parameters:
            attr: Attribute to be found in config
            pool_id: Id of pool to find attribute

        Returns:
            attribute value or None
        """
        if pool_id is not None:
            for pool in self.data['pools']:
                if pool['id'] == pool_id:
                    return pool.get(attr)
        else:
            result = []
            for pool in self.data['pools']:
                if attr in pool:
                    if isinstance(pool[attr], list):
                        result.extend(pool[attr])
                    else:
                        result.append(pool[attr])
            if result:
                return result

        return None


    def get_app_attr(self, attr, app_id):
        """
        Get specific attribute from config

        Parameters:
            attr: Attribute to be found in config
            app_id: Id of app to find attribute

        Returns:
            attribute value or None
        """
        for app in self.data['apps']:
            if app['id'] == app_id:
                if attr in app:
                    return app[attr]

        return None


    def get_rdt_iface(self):
        """
        Get RDT interface from config

        Returns:
            configured RDT interface or "msr" by default
        """
        if 'rdt_iface' in self.data:
            return self.data['rdt_iface'].get('interface', 'msr')

        return "msr"


    def get_mba_ctrl_enabled(self):
        """
        Get RDT MBA CTRL Enabled from config

        Returns:
            configured RDT MBA CTRL Enabled value or "false" by default
        """
        if 'mba_ctrl' in self.data:
            return self.data['mba_ctrl']['enabled']

        return False


    def get_l3cdp_enabled(self):
        """
        Get RDT L3 CDP Enabled from config

        Returns:
            configured RDT L3 CDP Enabled value or "false" by default
        """
        if 'rdt' in self.data:
            return self.data['rdt'].get('l3cdp', False)

        return False


    def get_l2cdp_enabled(self):
        """
        Get RDT L2 CDP Enabled from config

        Returns:
            configured RDT L2 CDP Enabled value or "false" by default
        """
        if 'rdt' in self.data:
            return self.data['rdt'].get('l2cdp', False)

        return False


    def get_pool(self, pool_id):
        """
        Get pool

        Parameters
            data: configuration (dict)
            pool_id: pool id

        Return
            Pool details
        """
        if 'pools' not in self.data:
            raise KeyError("No pools in config")

        for pool in self.data['pools']:
            if pool['id'] == pool_id:
                return pool

        raise KeyError(f"Pool {pool_id} does not exists.")


    def is_any_pool_defined(self):
        """
        Check if there is at least one pool defined
        (other than "default" #0 one)

        Returns:
            result
        """
        if 'pools' not in self.data:
            return False

        for pool in self.data['pools']:
            if not pool['id'] == 0:
                return True

        return False


    def is_default_pool_defined(self):
        """
        Check is Default pool defined

        Returns:
            result
        """
        if 'pools' not in self.data:
            return False

        for pool in self.data['pools']:
            if pool['id'] == 0:
                return True

        return False


    def remove_default_pool(self):
        """
        Remove Default pool
        """
        if 'pools' not in self.data:
            return

        for pool in self.data['pools'][:]:
            if pool['id'] == 0:
                self.data['pools'].remove(pool)
                break


    def add_default_pool(self):
        """
        Update config with "Default" pool
        """
        iface = self.get_rdt_iface()

        if 'pools' not in self.data:
            self.data['pools'] = []

        # no Default pool configured
        default_pool = {}
        default_pool['id'] = 0

        if caps.mba_supported(iface):
            if self.get_mba_ctrl_enabled():
                default_pool['mba_bw'] = 2**32 - 1
            else:
                default_pool['mba'] = 100

        if caps.cat_l3_supported(iface):
            default_pool['l3cbm'] = PQOS_API.get_max_l3_cat_cbm()
            if self.get_l3cdp_enabled():
                default_pool['l3cbm_code'] = default_pool['l3cbm']
                default_pool['l3cbm_data'] = default_pool['l3cbm']

        if caps.cat_l2_supported(iface):
            default_pool['l2cbm'] = PQOS_API.get_max_l2_cat_cbm()
            if self.get_l2cdp_enabled():
                default_pool['l2cbm_code'] = default_pool['l2cbm']
                default_pool['l2cbm_data'] = default_pool['l2cbm']

        default_pool['name'] = "Default"

        # Use all unallocated cores
        default_pool['cores'] = PQOS_API.get_cores()
        for pool in self.data['pools']:
            default_pool['cores'] = \
                [core for core in default_pool['cores'] if core not in pool['cores']]

        self.data['pools'].append(default_pool)


    def get_app(self, app_id):
        """
        Get app

        Parameters
            data: configuration (dict)
            app_id: app id

        Return
            Pool details
        """
        if 'apps' not in self.data:
            raise KeyError(f"App {app_id} does not exist. No apps in config.")

        for app in self.data['apps']:
            if app['id'] == app_id:
                return app

        raise KeyError(f"App {app_id} does not exist.")


    def get_power(self, power_id):
        """
        Get power

        Parameters
            data: configuration (dict)
            id: power profile id

        Return
            Pool details
        """
        if 'power_profiles' not in self.data:
            raise KeyError("No power profiles in config")

        for profile in self.data['power_profiles']:
            if profile["id"] == power_id:
                return profile

        raise KeyError(f"Power profile {power_id} does not exists")


    def get_power_profile(self, power_profile_id):
        """
        Get power profile configuration

        Parameters:
            power_profile_id: id of power profile
        """
        return self.get_power(power_profile_id)


    def pid_to_app(self, pid):
        """
        Gets APP ID for PID

        Parameters:
            pid: PID to get APP ID for

        Returns:
            App ID, None on error
        """
        if not pid:
            return None

        for app in self.data['apps']:
            if not ('id' in app and 'pids' in app):
                continue
            if pid in app['pids']:
                return app['id']
        return None


    def app_to_pool(self, app):
        """
        Gets Pool ID for App

        Parameters:
            app: App ID to get Pool ID for

        Returns:
            Pool ID or None on error
        """
        if not app:
            return None

        for pool in self.data['pools']:
            if not ('id' in pool and 'apps' in pool):
                continue
            if app in pool['apps']:
                return pool['id']
        return None


    def pid_to_pool(self, pid):
        """
        Gets Pool ID for PID

        Parameters:
            PID: PID to get Pool ID for

        Returns:
            Pool ID or None on error
        """
        app_id = self.pid_to_app(pid)
        return self.app_to_pool(app_id)


    def get_global_attr(self, attr, default):
        """
        Get specific global attribute from config

        Parameters:
            attr: global attribute to be found in config

        Returns:
            attribute value or None
        """

        if attr not in self.data:
            return default

        return self.data[attr]

################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2020 Intel Corporation. All rights reserved.
# All rights reserved.
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
Module handling config file
"""

import json
from os.path import join, dirname
import re
import jsonschema

import caps
import common
import pid_ops
import power


class ConfigStore:
    """
    Class to handle config file operations
    """


    def __init__(self):
        self.namespace = common.MANAGER.Namespace()
        self.namespace.config = {}
        self.namespace.path = None
        self.changed_event = common.MANAGER.Event()


    def get_pool_attr(self, attr, pool_id):
        """
        Get specific attribute from config

        Parameters:
            attr: Attribute to be found in config
            pool_id: Id of pool to find attribute

        Returns:
            attribute value or None
        """

        data = self.get_config()

        if pool_id is not None:
            for pool in data['pools']:
                if pool['id'] == pool_id:
                    return pool.get(attr)
        else:
            result = []
            for pool in data['pools']:
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

        data = self.get_config()

        for app in data['apps']:
            if app['id'] == app_id:
                if attr in app:
                    return app[attr]

        return None


    def set_path(self, path):
        """
        Set path to configuration file

        Parameters:
            path: path to config file
        """
        self.namespace.path = path


    def get_path(self):
        """
        Get path to configuration file

        Returns:
            path: path to config file
        """
        return self.namespace.path


    def reset(self):
        """
        Reset configuration, reload config file
        """

        self.from_file(self.get_path())


    @staticmethod
    def get_pool(data, pool_id):
        """
        Get pool

        Parameters
            data: configuration (dict)
            pool_id: pool id

        Return
            Pool details
        """
        if 'pools' not in data:
            raise KeyError("No pools in config")

        for pool in data['pools']:
            if pool['id'] == pool_id:
                return pool

        raise KeyError("Pool {} does not exists.".format(pool_id))


    @staticmethod
    def get_app(data, app_id):
        """
        Get app

        Parameters
            data: configuration (dict)
            app_id: app id

        Return
            Pool details
        """
        if 'apps' not in data:
            raise KeyError("App {} does not exist. No apps in config.".format(app_id))

        for app in data['apps']:
            if app['id'] == app_id:
                return app

        raise KeyError("App {} does not exist.".format(app_id))


    @staticmethod
    def get_power(data, power_id):
        """
        Get power

        Parameters
            data: configuration (dict)
            id: power profile id

        Return
            Pool details
        """
        if 'power_profiles' not in data:
            raise KeyError("No power profiles in config")

        for profile in data['power_profiles']:
            if profile["id"] == power_id:
                return profile

        raise KeyError(("Power profile {} does not exists").format(power_id))


    @staticmethod
    def validate(data, power_admission_control=False):
        """
        Validate configuration

        Parameters
            data: configuration (dict)
        """

        # validates config schema
        schema, resolver = ConfigStore.load_json_schema('appqos.json')
        jsonschema.validate(data, schema, resolver=resolver)

        ConfigStore._validate_pools(data)
        ConfigStore._validate_apps(data)
        power.validate_power_profiles(data, power_admission_control)


    @staticmethod
    def _validate_pools(data):
        """
        Validate Pools configuration

        Parameters
            data: configuration (dict)
        """
        if not 'pools' in data:
            return

        # verify pools
        cores = set()
        pool_ids = []

        for pool in data['pools']:
            # id
            if pool['id'] in pool_ids:
                raise ValueError("Pool {}, multiple pools with same id.".format(pool['id']))
            pool_ids.append(pool['id'])

            # pool cores
            for core in pool['cores']:
                if not common.PQOS_API.check_core(core):
                    raise ValueError("Pool {}, Invalid core {}.".format(pool['id'], core))

            if cores.intersection(pool['cores']):
                raise ValueError("Pool {}, Cores {} already assigned to another pool."\
                    .format(pool['id'], cores.intersection(pool['cores'])))

            cores |= set(pool['cores'])

            # check app reference
            if 'apps' in pool:
                for app_id in pool['apps']:
                    ConfigStore.get_app(data, app_id)

            if 'cbm' in pool:
                result = re.search('1{1,32}0{1,32}1{1,32}', bin(pool['cbm']))
                if result or pool['cbm'] == 0:
                    raise ValueError("Pool {}, CBM {}/{} is not contiguous."\
                    .format(pool['id'], hex(pool['cbm']), bin(pool['cbm'])))
                if not caps.cat_supported():
                    raise ValueError("Pool {}, CBM {}/{}, CAT is not supported."\
                    .format(pool['id'], hex(pool['cbm']), bin(pool['cbm'])))

            if 'mba' in pool:
                if pool['mba'] > 100 or pool['mba'] <= 0:
                    raise ValueError("Pool {}, MBA rate {} out of range! (1-100)."\
                    .format(pool['id'], pool['mba']))
                if not caps.mba_supported():
                    raise ValueError("Pool {}, MBA rate {}, MBA is not supported."\
                    .format(pool['id'], pool['mba']))

            # check power profile reference
            if 'power_profile' in pool:
                ConfigStore.get_power(data, pool['power_profile'])


    @staticmethod
    def _validate_apps(data):
        """
        Validate Apps configuration

        Parameters
            data: configuration (dict)
        """
        if not 'apps' in data:
            return

        # verify apps
        pids = set()
        app_ids = []

        for app in data['apps']:
            # id
            if app['id'] in app_ids:
                raise ValueError("App {}, multiple apps with same id.".format(app['id']))
            app_ids.append(app['id'])

            # app's cores validation
            if 'cores' in app:
                for core in app['cores']:
                    if not common.PQOS_API.check_core(core):
                        raise ValueError("App {}, Invalid core {}.".format(app['id'], core))

            # app's pool validation
            app_pool = None
            for pool in data['pools']:
                if 'apps' in pool and app['id'] in pool['apps']:
                    if app_pool:
                        raise ValueError("App {}, Assigned to more than one pool."\
                            .format(app['id']))
                    app_pool = pool

            if app_pool is None:
                raise ValueError("App {} not assigned to any pool.".format(app['id']))

            if 'cores' in app:
                diff_cores = set(app['cores']).difference(app_pool['cores'])
                if diff_cores:
                    raise ValueError("App {}, cores {} does not match Pool {}."\
                        .format(app['id'], diff_cores, app_pool['id']))

            # app's pids validation
            for pid in app['pids']:
                if not pid_ops.is_pid_valid(pid):
                    raise ValueError("App {}, PID {} is not valid.".format(app['id'], pid))

            if pids.intersection(app['pids']):
                raise ValueError("App {}, PIDs {} already assigned to another App."\
                    .format(app['id'], pids.intersection(app['pids'])))

            pids |= set(app['pids'])


    def from_file(self, path):
        """
        Retrieve config from file

        Parameters:
            path: path to config file
        """
        self.set_path(path)
        data = self.load(path)

        if not self.is_default_pool_defined(data):
            self.add_default_pool(data)

        power_admission_check_cfg = data.get('power_profiles_verify', True)

        self.validate(data, power_admission_check_cfg)

        self.set_config(data)


    @staticmethod
    def load_json_schema(filename):
        """
        Loads the given schema file

        Parameters:
            filename: path to JSON schema file
        Returns:
            schema: schema
            resolver: resolver
        """
        # find path to schema
        relative_path = join('schema', filename)
        absolute_path = join(dirname(__file__), relative_path)
        # path to all schema files
        schema_path = 'file:' + str(join(dirname(__file__), 'schema')) + '/'
        with open(absolute_path, opener=common.check_link) as schema_file:
            # add resolver for python to find all schema files
            schema = json.loads(schema_file.read())
            return schema, jsonschema.RefResolver(schema_path, schema)


    @staticmethod
    def load(path):
        """
        Load configuration from file

        Parameters:
            path: Path of the configuration file

        Returns:
            schema validated configuration
        """
        with open(path, 'r', opener=common.check_link) as fd:
            raw_data = fd.read()
            data = json.loads(raw_data.replace('\r\n', '\\r\\n'))

            # validates config schema from config file
            schema, resolver = ConfigStore.load_json_schema('appqos.json')
            jsonschema.validate(data, schema, resolver=resolver)

            # convert cbm to int
            for pool in data['pools']:
                if 'cbm' in pool and not isinstance(pool['cbm'], int):
                    pool['cbm'] = int(pool['cbm'], 16)

            return data

        return None


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

        data = self.get_config()
        for app in data['apps']:
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
        data = self.get_config()
        for pool in data['pools']:
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


    def set_config(self, data):
        """
        Set shared (via IPC, namespace) configuration

        Parameters:
            data: new configuration
        """

        self.namespace.config = data
        self.changed_event.set()


    def get_config(self):
        """
        Get shared (via IPC, namespace) configuration
        Returns:
            shared configuration (dict)
        """
        return self.namespace.config


    def get_power_profile(self, power_profile_id):
        """
        Get power profile configuration

        Parameters:
            power_profile_id: id of power profile
        """
        config = self.get_config()

        return self.get_power(config, power_profile_id)


    def is_config_changed(self):
        """
        Check was shared configuration marked as changed

        Returns:
            result
        """
        try:
            self.changed_event.wait(0.1)
            result = self.changed_event.is_set()
            if result:
                self.changed_event.clear()
        except IOError:
            result = False

        return result


    @staticmethod
    def is_default_pool_defined(data):
        """
        Check is Default pool defined

        Returns:
            result
        """
        for pool in data['pools']:
            if pool['id'] == 0:
                return True

        return False

    @staticmethod
    def add_default_pool(data):
        """
        Update config with "Default" pool
        """

        # no Default pool configured
        default_pool = {}
        default_pool['id'] = 0

        if caps.mba_supported():
            default_pool['mba'] = 100

        if caps.cat_supported():
            default_pool['cbm'] = common.PQOS_API.get_max_l3_cat_cbm()

        default_pool['name'] = "Default"

        # Use all unallocated cores
        default_pool['cores'] = list(range(common.PQOS_API.get_num_cores()))
        for pool in data['pools']:
            default_pool['cores'] = \
                [core for core in default_pool['cores'] if core not in pool['cores']]

        data['pools'].append(default_pool)


    def get_new_pool_id(self, new_pool_data):
        """
        Get ID for new Pool

        Returns:
            ID for new Pool
        """
        # get max cos id for combination of allocation technologies
        alloc_type = []
        if 'mba' in new_pool_data:
            alloc_type.append(common.MBA_CAP)
        if 'cbm' in new_pool_data:
            alloc_type.append(common.CAT_CAP)
        max_cos_id = common.PQOS_API.get_max_cos_id(alloc_type)

        data = self.get_config()

        # put all pool ids into list
        pool_ids = []
        for pool in data['pools']:
            pool_ids.append(pool['id'])

        # no pool found in config, return highest id
        if not pool_ids:
            return max_cos_id

        # find highest available id
        new_ids = list(set(range(1, max_cos_id + 1)) - set(pool_ids))
        if new_ids:
            new_ids.sort()
            return new_ids[-1]

        return None


    def get_new_app_id(self):
        """
        Get ID for new App

        Returns:
            ID for new App
        """

        data = self.get_config()

        # put all ids into list
        app_ids = []
        for app in data['apps']:
            app_ids.append(app['id'])
        app_ids = sorted(app_ids)
        # no app found in config
        if not app_ids:
            return 1

        # add new app to apps
        # find an id
        new_ids = list(set(range(1, app_ids[-1])) - set(app_ids))
        if new_ids:
            return new_ids[0]

        return app_ids[-1] + 1


    def get_new_power_profile_id(self):
        """
        Get ID for new Power Profile

        Returns:
            ID for new Power Profile
        """

        data = self.get_config()

        # no profile found in config
        if 'power_profiles' not in data:
            return 0

        # put all ids into list
        profile_ids = []
        for profile in data['power_profiles']:
            profile_ids.append(profile['id'])

        profile_ids = sorted(profile_ids)

        # no profile found in config
        if not profile_ids:
            return 1

        # find first available profile id
        new_ids = list(set(range(1, profile_ids[-1])) - set(profile_ids))
        if new_ids:
            return new_ids[0]

        return profile_ids[-1] + 1


    def get_global_attr(self, attr, default):
        """
        Get specific global attribute from config

        Parameters:
            attr: global attribute to be found in config

        Returns:
            attribute value or None
        """

        data = self.get_config()

        if attr not in data:
            return default

        return data[attr]

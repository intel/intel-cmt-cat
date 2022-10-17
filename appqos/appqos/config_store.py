################################################################################
# BSD LICENSE
#
# Copyright(c) 2022 Intel Corporation. All rights reserved.
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
import log
import pid_ops
import power
from config import Config

class ConfigStore:
    """
    Class to handle config file operations
    """

    namespace = common.MANAGER.Namespace()
    namespace.config = {}
    namespace.path = None
    changed_event = common.MANAGER.Event()

    @staticmethod
    def set_path(path):
        """
        Set path to configuration file

        Parameters:
            path: path to config file
        """
        ConfigStore.namespace.path = path


    @staticmethod
    def get_path():
        """
        Get path to configuration file

        Returns:
            path: path to config file
        """
        return ConfigStore.namespace.path


    def reset(self):
        """
        Reset configuration, reload config file
        """

        self.from_file(self.get_path())
        self.process_config()


    def validate(self, cfg, power_admission_control=False):
        """
        Validate configuration

        Parameters
            cfg: configuration (dict)
        """

        # validates config schema
        schema, resolver = ConfigStore.load_json_schema('appqos.json')
        jsonschema.validate(cfg.data, schema, resolver=resolver)

        self._validate_pools(cfg)
        self._validate_apps(cfg)
        self._validate_rdt(cfg)
        power.validate_power_profiles(cfg, power_admission_control)


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
                raise ValueError(f"Pool {pool['id']}, multiple pools with same id.")
            pool_ids.append(pool['id'])

            # pool cores
            for core in pool['cores']:
                if not common.PQOS_API.check_core(core):
                    raise ValueError(f"Pool {pool['id']}, Invalid core {core}.")

            if cores.intersection(pool['cores']):
                raise ValueError(f"Pool {pool['id']}, " \
                    f"Cores {cores.intersection(pool['cores'])} already assigned to another pool.")

            cores |= set(pool['cores'])

            # check app reference
            if 'apps' in pool:
                for app_id in pool['apps']:
                    data.get_app(app_id)

            # check power profile reference
            if 'power_profile' in pool:
                data.get_power(pool['power_profile'])


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
                raise ValueError(f"App {app['id']}, multiple apps with same id.")
            app_ids.append(app['id'])

            # app's cores validation
            if 'cores' in app:
                for core in app['cores']:
                    if not common.PQOS_API.check_core(core):
                        raise ValueError(f"App {app['id']}, Invalid core {core}.")

            # app's pool validation
            app_pool = None
            for pool in data['pools']:
                if 'apps' in pool and app['id'] in pool['apps']:
                    if app_pool:
                        raise ValueError(f"App {app['id']}, Assigned to more than one pool.")
                    app_pool = pool

            if app_pool is None:
                raise ValueError(f"App {app['id']} not assigned to any pool.")

            if 'cores' in app:
                diff_cores = set(app['cores']).difference(app_pool['cores'])
                if diff_cores:
                    raise ValueError(f"App {app['id']}, " \
                        f"cores {diff_cores} does not match Pool {app_pool['id']}.")

            # app's pids validation
            for pid in app['pids']:
                if not pid_ops.is_pid_valid(pid):
                    raise ValueError(f"App {app['id']}, PID {pid} is not valid.")

            if pids.intersection(app['pids']):
                raise ValueError(f"App {app['id']}, " \
                    f"PIDs {pids.intersection(app['pids'])} already assigned to another App.")

            pids |= set(app['pids'])


    @staticmethod
    def _validate_rdt_cat_l3(data):
        """
        Validate L3 CAT RDT configuration

        Parameters
            data: configuration (dict)
        """
        l3cdp_enabled = data['rdt'].get('l3cdp', False) if 'rdt' in data \
            else False

        if l3cdp_enabled and not caps.cdp_l3_supported():
            raise ValueError("RDT Configuration. L3 CDP requested but not supported!")

        cdp_pool_ids = []

        for pool in data['pools']:
            for cbm in ['l3cbm', 'l3cbm_data', 'l3cbm_code']:
                if not cbm in pool:
                    continue

                result = re.search('1{1,32}0{1,32}1{1,32}', bin(pool[cbm]))
                if result or pool[cbm] == 0:
                    raise ValueError(f"Pool {pool['id']}, " \
                        f"L3 CBM {hex(pool['l3cbm'])}/{bin(pool[cbm])} is not contiguous.")
                if not caps.cat_l3_supported():
                    raise ValueError(f"Pool {pool['id']}, " \
                        f"L3 CBM {hex(pool['l3cbm'])}/{bin(pool[cbm])}, L3 CAT is not supported.")

            if 'l3cbm_data' in pool or 'l3cbm_code' in pool:
                cdp_pool_ids.append(pool['id'])

        if cdp_pool_ids:
            if not caps.cdp_l3_supported():
                raise ValueError(f"Pools {cdp_pool_ids}, L3 CDP is not supported.")
            if not l3cdp_enabled:
                raise ValueError(f"Pools {cdp_pool_ids}, L3 CDP is not enabled.")

    @staticmethod
    def _validate_rdt_cat_l2(data):
        """
        Validate L2 CAT RDT configuration
        Parameters
            data: configuration (dict)
        """
        l2cdp_enabled = data['rdt'].get('l2cdp', False) if 'rdt' in data \
            else False

        if l2cdp_enabled and not caps.cdp_l2_supported():
            raise ValueError("RDT Configuration. L2 CDP requested but not supported!")

        cdp_pool_ids = []

        for pool in data['pools']:
            for cbm in ['l2cbm', 'l2cbm_data', 'l2cbm_code']:
                if not cbm in pool:
                    continue

                result = re.search('1{1,32}0{1,32}1{1,32}', bin(pool[cbm]))
                if result or pool[cbm] == 0:
                    raise ValueError(f"Pool {pool['id']}, " \
                        f"L2 CBM {hex(pool['l2cbm'])}/{bin(pool[cbm])} is not contiguous.")
                if not caps.cat_l2_supported():
                    raise ValueError(f"Pool {pool['id']}, " \
                        f"L2 CBM {hex(pool['l2cbm'])}/{bin(pool[cbm])}, L2 CAT is not supported.")

            if 'l2cbm_data' in pool or 'l2cbm_code' in pool:
                cdp_pool_ids.append(pool['id'])

        if cdp_pool_ids:
            if not caps.cdp_l2_supported():
                raise ValueError(f"Pools {cdp_pool_ids}, L2 CDP is not supported.")
            if not l2cdp_enabled:
                raise ValueError(f"Pools {cdp_pool_ids}, L2 CDP is not enabled.")


    def _validate_rdt(self, data):
        """
        Validate RDT configuration (including MBA CTRL) configuration

        Parameters
            data: configuration (dict)
        """
        self._validate_rdt_cat_l2(data)
        self._validate_rdt_cat_l3(data)

        # if data to be validated does not contain RDT iface and/or MBA CTRL info
        # get missing info from current configuration
        rdt_iface = data['rdt_iface']['interface'] if 'rdt_iface' in data \
            else ConfigStore.get_config().get_rdt_iface()
        mba_ctrl_enabled = data['mba_ctrl']['enabled'] if 'mba_ctrl' in data \
            else ConfigStore.get_config().get_mba_ctrl_enabled()

        if mba_ctrl_enabled and rdt_iface != "os":
            raise ValueError("RDT Configuration. MBA CTRL requires RDT OS interface!")

        if mba_ctrl_enabled and not caps.mba_bw_supported():
            raise ValueError("RDT Configuration. MBA CTRL requested but not supported!")

        if not 'pools' in data:
            return

        mba_pool_ids = []
        mba_bw_pool_ids = []

        for pool in data['pools']:
            if 'mba' in pool:
                mba_pool_ids.append(pool['id'])

            if 'mba_bw' in pool:
                mba_bw_pool_ids.append(pool['id'])

        if (mba_pool_ids or mba_bw_pool_ids) and not caps.mba_supported():
            raise ValueError(f"Pools {mba_pool_ids + mba_bw_pool_ids}, MBA is not supported.")

        if mba_bw_pool_ids and not mba_ctrl_enabled:
            raise ValueError(f"Pools {mba_bw_pool_ids}, MBA BW is not enabled/supported.")

        if mba_pool_ids and mba_ctrl_enabled:
            raise ValueError(f"Pools {mba_pool_ids}, MBA % is not enabled. " \
                             "Disable MBA BW and try again.")

        return


    def from_file(self, path):
        """
        Retrieve config from file

        Parameters:
            path: path to config file
        """
        self.set_path(path)
        ConfigStore.namespace.config = self.load(path).data


    def process_config(self):
        """
        Processes/validates config
        """
        data = ConfigStore.get_config()

        if not data.is_default_pool_defined():
            data.add_default_pool()

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
        with open(absolute_path, opener=common.check_link, encoding='UTF-8') as schema_file:
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
        with open(path, 'r', opener=common.check_link, encoding='UTF-8') as fd:
            raw_data = fd.read()
            data = json.loads(raw_data.replace('\r\n', '\\r\\n'))

            # validates config schema from config file
            schema, resolver = ConfigStore.load_json_schema('appqos.json')
            jsonschema.validate(data, schema, resolver=resolver)

            # convert cbm to int
            for pool in data['pools']:
                if 'cbm' in pool:
                    log.warn("cbm property is deprecated, please use l3cbm instead")
                    if 'l3cbm' not in pool:
                        pool['l3cbm'] = pool['cbm']
                    pool.pop('cbm')

                for cbm in ['l2cbm', 'l2cbm_code', 'l2cbm_data', \
                            'l3cbm', 'l3cbm_code', 'l3cbm_data']:
                    if cbm in pool and not isinstance(pool[cbm], int):
                        pool[cbm] = int(pool[cbm], 16)

            return Config(data)

        return None


    @staticmethod
    def set_config(cfg):
        """
        Set shared (via IPC, namespace) configuration

        Parameters:
            cfg: new configuration
        """

        ConfigStore.namespace.config = cfg.data
        ConfigStore.changed_event.set()


    @staticmethod
    def get_config():
        """
        Get shared (via IPC, namespace) configuration
        Returns:
            shared configuration (dict)
        """
        return Config(ConfigStore.namespace.config)


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
    def recreate_default_pool():
        """
        Recreate Default pool
        """
        # not using get_config/set_config pair
        # not to trigger "changed_event"
        cfg = ConfigStore.get_config()

        if cfg.is_default_pool_defined():
            cfg.remove_default_pool()

        cfg.add_default_pool()

        ConfigStore.namespace.config = cfg.data


    @staticmethod
    def get_new_pool_id(new_pool_data):
        """
        Get ID for new Pool

        Returns:
            ID for new Pool
        """
        # get max cos id for combination of allocation technologies
        alloc_type = []
        if 'mba' in new_pool_data or 'mba_bw' in new_pool_data:
            alloc_type.append(common.MBA_CAP)
        if any(k in new_pool_data for k in ('l2cbm', 'l2cbm_data', 'l2cbm_code')):
            alloc_type.append(common.CAT_L2_CAP)
        if any(k in new_pool_data for k in ('l3cbm', 'l3cbm_data', 'l3cbm_code')):
            alloc_type.append(common.CAT_L3_CAP)
        max_cos_id = common.PQOS_API.get_max_cos_id(alloc_type)

        data = ConfigStore.get_config()

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


    @staticmethod
    def get_new_app_id():
        """
        Get ID for new App

        Returns:
            ID for new App
        """

        data = ConfigStore.get_config()

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


    @staticmethod
    def get_new_power_profile_id():
        """
        Get ID for new Power Profile

        Returns:
            ID for new Power Profile
        """

        data = ConfigStore.get_config()

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

################################################################################
# BSD LICENSE
#
# Copyright(c) 2019 Intel Corporation. All rights reserved.
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
import threading
from os.path import join, dirname
import jsonschema

import common


class ConfigStore(object):
    """
    Class to handle config file operations
    """


    def __init__(self):
        self.namespace = common.MANAGER.Namespace()
        self.namespace.config = {}
        self.namespace.path = None
        self.changed_event = common.MANAGER.Event()
        self.timer = None


    def get_attr_list(self, attr, priority):
        # pylint: disable=too-many-return-statements, too-many-branches
        """
        Get specific attribute from config

        Parameters:
            attr: Attribute to be found in config
            priority: Priority of pool to find attribute

        Returns:
            attribute value, None
        """

        data = self.namespace.config
        if priority is None:
            if str(attr) in data:
                return data[str(attr)]
        else:
            apps = []

            for pool in data['pools']:
                if priority is None:
                    if attr == 'min_cws':
                        return pool['min_cws']
                    if attr == 'pool_id':
                        return pool['id']
                    for app in pool['apps']:
                        apps.append(app)
                elif attr == 'min_cws' and pool['priority'] == priority:
                    return pool['min_cws']
                elif attr == 'pool_id' and pool['priority'] == priority:
                    return pool['id']
                elif 'apps' in pool and pool['priority'] == priority:
                    for app in pool['apps']:
                        apps.append(app)

            values = []
            for app in data['apps']:
                if app['id'] in apps:
                    if not attr in app:
                        continue
                    if isinstance(app[str(attr)], list):
                        values += app[str(attr)]
                    else:
                        values.append(str(app[attr]))

            return values

        return []


    def set_path(self, path):
        """
        Set path to configuration file

        Parameters:
            path: path to config file
        """
        self.namespace.path = path


    def from_file(self, path):
        """
        Retrieve config from file

        Parameters:
            path: path to config file
        """
        self.set_path(path)
        data = self.load(path)
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
        with open(absolute_path) as schema_file:
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
            validated configuration
        """
        with open(path, 'r+') as fd:
            raw_data = fd.read()
            data = json.loads(raw_data.replace('\r\n', '\\r\\n'))

            # validates config schema from config file
            schema, resolver = ConfigStore.load_json_schema('appqos.json')
            jsonschema.validate(data, schema, resolver=resolver)

            return data

        return None


    def get_pool_id(self, priority):
        """
        Get pool id from config

        Parameters:
            priority: Priority of pool to find attribute

        Returns:
            Pool ID, None on error
        """
        pool_id = self.get_attr_list('pool_id', priority)
        return pool_id or None


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
        Gets Pool ID and Priority for App

        Parameters:
            app: App ID to get Pool ID and Priority for

        Returns:
            Pool ID, Priority, or None, None on error
        """
        if not app:
            return None, None

        data = self.get_config()
        for pool in data['pools']:
            if not ('id' in pool and 'apps' in pool):
                continue
            if app in pool['apps']:
                return pool['id'], pool['priority']
        return None, None


    def pid_to_pool(self, pid):
        """
        Gets Pool ID and Priority for PID

        Parameters:
            PID: PID to get Pool ID and Priority for

        Returns:
            Pool ID, Priority, or None, None on error
        """
        app_id = self.pid_to_app(pid)
        return self.app_to_pool(app_id)


    def pool_to_prio(self, pool_id):
        """
        Gets Priority for Pool ID

        Parameters:
            pool_id: Pool ID to get Priority for

        Returns:
            Priority, None on error
        """
        if not pool_id:
            return None

        data = self.get_config()
        for pool in data['pools']:
            if 'id' not in pool:
                continue
            if pool_id == pool['id']:
                return pool['priority']
        return None


    def set_config(self, data):
        """
        Set shared (via IPC, namespace) configuration

        Parameters:
            data: new configuration
        """

        def clear_event():
            """
            Clears "changed_event" used to propagate info about config change
            """
            self.changed_event.clear()

        if self.timer is not None:
            self.timer.cancel()

        self.namespace.config = data
        self.changed_event.set()

        self.timer = threading.Timer(1, clear_event)
        self.timer.start()


    def get_config(self):
        """
        Get shared (via IPC, namespace) configuration
        Returns:
            shared configuration (dict)
        """
        return self.namespace.config


    def is_config_changed(self):
        """
        Check was shared configuration marked as changed

        Returns:
            result
        """
        return common.is_event_set(self.changed_event)

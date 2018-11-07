#!/usr/bin/env python2.7

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
CLI program
"""

#pylint: disable=too-many-lines

import cmd
import os
import json
import argparse
import getpass
import urllib3
from urllib3.exceptions import InsecureRequestWarning
import requests
from requests.auth import HTTPBasicAuth


class Config(object):
    """
    Config file handling
    """
    host = None
    port = None
    user = None
    password = None


    def __init__(self):
        pass


    def get_host(self):
        """
        Get REST API server hostname

        Returns:
            hostname, raises exception on error
        """
        if self.host is None:
            print "*** Invalid server configuration. Please update configuration"
            raise "*** Invalid server configuration. Please update configuration"
        return self.host


    def get_port(self):
        """
        Get REST API server's port

        Returns:
            port, raises exception on error
        """
        if self.port is None:
            print "*** Invalid server configuration. Please update configuration"
            raise "*** Invalid server configuration. Please update configuration"
        return self.port


    def get_user(self):
        """
        Get REST API server's username

        Returns:
            username
        """
        if self.user is None:
            Config.user = raw_input("User: ")
        return self.user


    def get_password(self):
        """
        Get REST API server's password

        Returns:
            password
        """
        if self.password is None:
            Config.password = getpass.getpass()
        return self.password


    @staticmethod
    def load(path):
        """
        Loads configuration from a file

        Parameters:
            path: path to config file
        """
        with open(path, 'r') as fd:
            raw_data = fd.read()
            data = json.loads(raw_data.replace('\r\n', '\\r\\n'))
            if "auth" in data:
                Config.user = data["auth"]["username"]
                if "password" in data["auth"]:
                    Config.password = data["auth"]["password"]
            if "server" in data:
                Config.host = data["server"]["host"]
                Config.port = data["server"]["port"]


    def save(self, path, store_password=False):
        """
        Saves configuration to a file

        Parameters:
            path: path to config file
            store_password(bool): flag to store password in config file
        """
        if self.host is None or self.port is None:
            return

        data = {}
        data["server"] = {}
        data["server"]["host"] = self.host
        data["server"]["port"] = self.port
        if self.user:
            data["auth"] = {}
            data["auth"]["username"] = self.user
            if self.password is not None and store_password:
                data["auth"]["password"] = self.password

        with open(path, 'w') as fd:
            fd.write(json.dumps(data))


class Api(object):
    """
    REST API helpers
    """


    class HttpError(Exception):
        """
        HttpError exception
        """


        def __init__(self, code, message):
            super(Api.HttpError, self).__init__()
            self.code = code
            self.message = message


        def __str__(self):
            return "%d - %s" % (self.code, self.message)


    def apps_get(self, app_id=None):
        """
        Get information about configured apps

        Parameters:
            app_id: app id

        Returns:
            Json containing app configuration
        """
        if app_id is not None:
            endpoint = 'apps/%d' % (app_id)
        else:
            endpoint = 'apps'
        return self.send('GET', endpoint)


    def apps_delete(self, app_id):
        """
        Remove app

        Parameters:
            app_id: app id to remove

        Returns:
            REST API server's response
        """
        endpoint = 'apps/%d' % (app_id)

        return self.send('DELETE', endpoint)


    def apps_move(self, app_id, pool_id):
        """
        Move app

        Parameters:
            app_id: app id to be moved
            pool_id: destination pool id

        Returns:
            REST API server's response
        """
        endpoint = 'apps/%d' % (app_id)
        data = {}
        data["pool_id"] = pool_id

        return self.send('PUT', endpoint, data)


    def pools_get(self, pool_id=None):
        """
        Get information about configured pools

        Parameters:
            pool_id: pool id

        Returns:
            Json containing pool configuration
        """
        if pool_id is not None:
            endpoint = 'pools/%d' % (pool_id)
        else:
            endpoint = 'pools'
        return self.send('GET', endpoint)


    def groups_get(self, group_id=None):
        """
        Get information about configured groups

        Parameters:
            group_id: group id

        Returns:
            Json containing group configuration
        """
        if group_id is not None:
            endpoint = 'groups/%d' % (group_id)
        else:
            endpoint = 'groups'
        return self.send('GET', endpoint)


    def stats_get(self):
        """
        Get stats

        Returns:
        Json representing stats
        """
        return self.send('GET', 'stats')


    @staticmethod
    def send(method, endpoint, data=None):
        """
        Perform REST API/HTTP request

        Parameters:
            method: http request method
            endpoint: server endpoint
            data: request's data

        Returns:
            REST API server's response
            Raises Api.HttpError on error
        """
        conf = Config()

        url = 'https://%s:%d/%s' % (conf.get_host(), conf.get_port(), endpoint)

        req = requests.request(method, url, json=data,
                               auth=HTTPBasicAuth(conf.get_user(), conf.get_password()),
                               verify=False)

        if req.status_code != 200 and req.status_code != 201:
            print '*** Request failed %d - %s' % (req.status_code, req.content)
            raise Api.HttpError(req.status_code, req.content)

        return json.loads(req.content)


API = Api()


class Shell(cmd.Cmd):
    """
    Base class for all commands
    """


    def __init__(self, **kwargs):
        cmd.Cmd.__init__(self, **kwargs)


    def emptyline(self):
        """
        Handles empty line
        """
        pass


class ShellApp(Shell):
    """
    Apps commands
    """


    def __init__(self, **kwargs):

        Shell.__init__(self, **kwargs)

        self.parser = argparse.ArgumentParser(prog='app', add_help=False)
        subparsers = self.parser.add_subparsers()

        self.parser_show = subparsers.add_parser('show', add_help=False,
                                                 help='Display information about selected APP\'s')
        group = self.parser_show.add_mutually_exclusive_group(required=True)
        group.add_argument('app_id', metavar="APP_ID", type=int, nargs='?', help='App id')
        group.add_argument('-a', '--all', action='store_true', help='All apps')
        self.parser_show.set_defaults(func=self.show)

        self.parser_move = subparsers.add_parser('move', add_help=False,
                                                 help='Move APP to different pool')
        self.parser_move.add_argument('app_id', metavar=("APP_ID"), type=int, nargs=1,
                                      help="App id")
        self.parser_move.add_argument('pool_id', metavar=("POOL_ID"), type=int, nargs=1,
                                      help="Pool id")
        self.parser_move.set_defaults(func=self.move)

        self.parser_remove = subparsers.add_parser('remove', add_help=False, help='Remove APP')
        self.parser_remove.add_argument('app_id', metavar=("APP_ID"), type=int, nargs=1,
                                        help="App id")
        self.parser_remove.set_defaults(func=self.remove)

        self.parser_add = subparsers.add_parser('add', add_help=False, help='Add new APP')
        self.parser_add.add_argument('pool_id', metavar=("POOL_ID"), type=int,
                                     nargs=1, help="Destination Pool ID")
        self.parser_add.add_argument('name', nargs=1, help="APP name")
        self.parser_add.add_argument('cores', type=int, nargs='+',
                                     help="Cores APP's VCPUs are pinned to")
        self.parser_add.add_argument('--pids', type=int, nargs='+',
                                     help="PIDs of APP's VCPUs")

        self.parser_add.set_defaults(func=self.add)


    def print_app(self, app):
        """
        Print app configuration

        Parameters:
            app: Structure containing app information
        """
        if isinstance(app, list):
            for _app in app:
                self.print_app(_app)
        else:
            print "app: %d (%s)" % (app["id"], app["name"])
            for field in sorted(app):
                if field == "id" or field == "name":
                    continue
                print "    %s: %s" % (field, str(app[field]))


    def show(self, args):
        """
        Handles app show command

        Parameters:
            args: command arguments
        """
        try:
            if args.all:
                app_id = None
            else:
                app_id = args.app_id

            data = API.apps_get(app_id)
            self.print_app(data)

        except Api.HttpError:
            pass
        except requests.ConnectionError:
            print '*** Could not connect to server'
        except Exception:
            pass


    @staticmethod
    def move(args):
        """
        Handles app move command

        Parameters:
            args: command arguments
        """
        try:
            API.apps_move(args.app_id[0], args.pool_id[0])
        except Api.HttpError:
            pass
        except requests.ConnectionError:
            print '*** Could not connect to server'
        except Exception:
            pass


    @staticmethod
    def remove(args):
        """
        Handles app remove command

        Parameters:
            args: command arguments
        """
        try:
            API.apps_delete(args.app_id[0])
        except Api.HttpError:
            pass
        except requests.ConnectionError:
            print '*** Could not connect to server'
        except Exception:
            pass


    @staticmethod
    def add(args):
        """
        Handles app add command

        Parameters:
            args: command arguments
        """
        data = {}
        data["pool_id"] = args.pool_id[0]
        data["name"] = args.name[0]
        data["cores"] = args.cores

        if args.pids:
            data["pids"] = args.pids

        try:
            data = API.send('POST', "apps", data)
        except Api.HttpError:
            pass
        except requests.ConnectionError:
            print '*** Could not connect to server'
        except Exception:
            pass


    def do_show(self, arg):
        """
        Parse show arguments
        Display app information

        Paramaters:
            arg: command arguments
        """
        try:
            args = self.parser_show.parse_args(arg.split())
            self.show(args)
        except SystemExit:
            return


    def do_move(self, arg):
        """
        Parse move arguments
        Move app

        Paramaters:
            arg: command arguments
        """
        try:
            args = self.parser_move.parse_args(arg.split())
            self.move(args)
        except SystemExit:
            return


    def do_remove(self, arg):
        """
        Parse remove arguments
        Remove app

        Paramaters:
            arg: command arguments
        """
        try:
            args = self.parser_remove.parse_args(arg.split())
            self.remove(args)
        except SystemExit:
            return


    def do_add(self, arg):
        """
        Parse add arguments
        Add app

        Paramaters:
            arg: command arguments
        """
        try:
            args = self.parser_add.parse_args(arg.split())
            self.add(args)
        except SystemExit:
            return


    def help_show(self):
        """
        Display help for show command
        """
        self.parser_show.print_help()


    def help_move(self):
        """
        Display help for move command
        """
        self.parser_move.print_help()


    def help_remove(self):
        """
        Display help for remove command
        """
        self.parser_remove.print_help()


    def help_add(self):
        """
        Display help for add command
        """
        self.parser_add.print_help()


    def do_help(self, arg):
        """
        Display help
        """
        if not arg:
            self.parser.print_help()
        else:
            cmd.Cmd.do_help(self, arg)


class ShellPool(Shell):
    """
    Pools commands
    """


    def __init__(self, **kwargs):
        Shell.__init__(self, **kwargs)

        self.parser = argparse.ArgumentParser(prog='pool', add_help=False)
        subparsers = self.parser.add_subparsers()

        self.parser_show = subparsers.add_parser('show', add_help=False,
                                                 help='Display information about selected pools')
        group = self.parser_show.add_mutually_exclusive_group(required=True)
        group.add_argument('pool_id', metavar="POOL_ID", type=int, nargs='?', help='Pool id')
        group.add_argument('-a', '--all', action='store_true', help='All pools')
        self.parser_show.set_defaults(func=self.show)


    def print_pool(self, pool):
        """
        Print pool information

        Parameters:
            pool: Structure containing pool information
        """
        if isinstance(pool, list):
            for _pool in pool:
                self.print_pool(_pool)
        else:
            print "pool: %d (%s)" % (pool["id"], pool["name"])
            for field in sorted(pool):
                if field == "id" or field == "name":
                    continue
                print "    %s: %s" % (field, str(pool[field]))


    def show(self, args):
        """
        Handles pool show command

        Parameters:
            args: command arguments
        """
        try:
            if args.all:
                pool_id = None
            else:
                pool_id = args.pool_id

            data = API.pools_get(pool_id)
            self.print_pool(data)
        except Api.HttpError:
            pass
        except requests.ConnectionError:
            print '*** Could not connect to server'
        except Exception:
            pass


    def do_show(self, arg):
        """
        Parse show arguments
        Display pool information

        Paramaters:
            arg: command arguments
        """
        try:
            args = self.parser_show.parse_args(arg.split())
            self.show(args)
        except SystemExit:
            return


    def help_show(self):
        """
        Display help for show command
        """
        self.parser_show.print_help()


    def do_help(self, arg):
        """
        Display help
        """
        if not arg:
            self.parser.print_help()
        else:
            cmd.Cmd.do_help(self, arg)


class ShellGroup(Shell):
    """
    Groups commands
    """


    def __init__(self, **kwargs):
        Shell.__init__(self, **kwargs)

        self.parser = argparse.ArgumentParser(prog='group', add_help=False)
        subparsers = self.parser.add_subparsers()

        self.parser_show = subparsers.add_parser('show', add_help=False,
                                                 help='Display information about selected groups')
        group = self.parser_show.add_mutually_exclusive_group(required=True)
        group.add_argument('group_id', metavar="GROUP_ID", type=int, nargs='?', help='Group id')
        group.add_argument('-a', '--all', action='store_true', help='All groups')
        self.parser_show.set_defaults(func=self.show)


    def print_group(self, group):
        """
        Print group information

        Parameters:
            group: Structure containing group information
        """
        if isinstance(group, list):
            for _group in group:
                self.print_group(_group)
        else:
            print "group: %d (%s)" % (group["id"], group["type"])
            for field in sorted(group):
                if field == "id" or field == "type":
                    continue
                print "    %s: %s" % (field, str(group[field]))


    def show(self, args):
        """
        Handles group show command

        Parameters:
            args: command arguments
        """
        try:
            if args.all:
                group_id = None
            else:
                group_id = args.group_id

            data = API.groups_get(group_id)
            self.print_group(data)
        except Api.HttpError:
            pass
        except requests.ConnectionError:
            print '*** Could not connect to server'
        except Exception:
            pass


    def do_show(self, arg):
        """
        Parse show arguments,
        performs command
        Display group information

        Paramaters:
            arg: command arguments
        """
        try:
            args = self.parser_show.parse_args(arg.split())
            self.show(args)
        except SystemExit:
            return


    def help_show(self):
        """
        Display help for show command
        """
        self.parser_show.print_help()


    def do_help(self, arg):
        """
        Display help
        """
        if not arg:
            self.parser.print_help()
        else:
            cmd.Cmd.do_help(self, arg)


class ShellStats(Shell):
    """
    Stats commands
    """


    def __init__(self, **kwargs):
        Shell.__init__(self, **kwargs)

        self.parser = argparse.ArgumentParser(prog='stats', add_help=False)
        subparsers = self.parser.add_subparsers()

        self.parser_show = subparsers.add_parser('show', add_help=False,
                                                 help='Display general statistics')


    def do_show(self, arg):
        """
        Parse show arguments,
        performs command
        Display general statistics

        Paramaters:
            arg: command arguments
        """
        try:
            self.parser_show.parse_args(arg.split())
        except SystemExit:
            return

        try:
            data = API.stats_get()

            print "stats"
            for field in data:
                desc = field
                if field == "num_flushes":
                    desc = "Number of cache flushes"
                elif field == "num_apps_moves":
                    desc = "Number of APPs moves"
                elif field == "num_err":
                    desc = "Number of errors"
                print "    %s: %s" % (desc, str(data[field]))

        except Api.HttpError:
            pass
        except requests.ConnectionError:
            print '*** Could not connect to server'
        except Exception:
            pass


    def help_show(self):
        """
        Display help for show command
        """
        self.parser_show.print_help()


    def do_help(self, arg):
        """
        Display help
        """
        if not arg:
            self.parser.print_help()
        else:
            cmd.Cmd.do_help(self, arg)


class ShellConfig(Shell):
    """
    Config commands
    """


    def __init__(self, **kwargs):
        Shell.__init__(self, **kwargs)

        self.parser = argparse.ArgumentParser(prog='config', add_help=False)
        subparsers = self.parser.add_subparsers()

        self.parser_load = subparsers.add_parser('load', add_help=False,
                                                 help='Load fonfiguration from file')
        self.parser_load.add_argument('path', metavar="PATH", default="cli.conf",
                                      help="Configuration file path")

        self.parser_save = subparsers.add_parser('save', add_help=False,
                                                 help='Save configuration to file')
        self.parser_save.add_argument('path', metavar="PATH", default="cli.conf",
                                      help="Configuration file path")
        self.parser_save.add_argument('--save-password', action="store_true",
                                      help="Save password in plaintext")

        self.parser_set = subparsers.add_parser('set', add_help=False, help='Update configuration')
        self.parser_set.add_argument('--host', metavar="HOST", help="Host")
        self.parser_set.add_argument('--port', metavar=("PORT"), type=int, nargs=1, help="Port")
        self.parser_set.add_argument('-u', '--user', metavar="USER", help="User")
        self.parser_set.add_argument('-p', '--password', metavar="PASSWORD", default=False,
                                     help="Password")


    def do_load(self, arg):
        """
        Parse load arguments,
        performs command
        Load configuration from file

        Paramaters:
            arg: command arguments
        """
        try:
            args = self.parser_load.parse_args(arg.split())
            Config().load(args.path)
        except SystemExit:
            return
        except IOError as ex:
            print ex
        except ValueError as ex:
            print ex
        except KeyError:
            print "*** Could not parse configuration file"


    def do_save(self, arg):
        """
        Parse save arguments,
        performs command
        Save configuration to file

        Paramaters:
            arg: command arguments
        """
        try:
            args = self.parser_save.parse_args(arg.split())
            Config().save(args.path, args.save_password)
        except SystemExit:
            return


    def do_set(self, arg):
        """
        Parse set arguments,
        performs command
        Update configuration

        Paramaters:
            arg: command arguments
        """
        try:
            args = self.parser_set.parse_args(arg.split())
            if args.host:
                Config.host = args.host
            if args.port:
                Config.port = args.port[0]
            if args.user:
                Config.user = args.user
            if args.password:
                Config.password = args.password

        except SystemExit:
            return


    def help_load(self):
        """
        Display help for load command
        """
        self.parser_load.print_help()


    def help_save(self):
        """
        Display help for save command
        """
        self.parser_save.print_help()


    def help_set(self):
        """
        Display help for set command
        """
        self.parser_set.print_help()


    def do_help(self, arg):
        """
        Display help
        """
        if not arg:
            self.parser.print_help()
        else:
            cmd.Cmd.do_help(self, arg)


class ShellMain(Shell):
    """
    Main shell
    """
    intro = 'Welcome to AppQoS. Type help or ? to list available commands.\n'
    prompt = 'AppQoS> '

    app = ShellApp()
    pool = ShellPool()
    group = ShellGroup()
    config = ShellConfig()
    stats = ShellStats()


    def complete_app(self, text, line, begidx, endidx):
        """
        app command autocompletion
        """
        return self.app.completenames(text, line, begidx, endidx)


    def do_app(self, arg):
        """
        Exec app command
        Operations on apps

        Parameters:
            arg: command arguments
        """
        self.app.onecmd(arg)


    def help_app(self):
        """
        Handle help for app command
        """
        self.app.onecmd('help')


    def complete_pool(self, text, line, begidx, endidx):
        """
        pool command autocompletion
        """
        return self.pool.completenames(text, line, begidx, endidx)


    def do_pool(self, arg):
        """
        Exec pool command
        Operations on pools

        Parameters:
            arg: command arguments
        """
        self.pool.onecmd(arg)


    def help_pool(self):
        """
        Handle help for pool command
        """
        self.pool.onecmd('help')


    def complete_group(self, text, line, begidx, endidx):
        """
        group command autocompletion
        """
        return self.group.completenames(text, line, begidx, endidx)


    def do_group(self, arg):
        """
        Exec group command
        Operations on groups

        Parameters:
            arg: command arguments
        """
        self.group.onecmd(arg)


    def help_group(self):
        """
        Handle help for group command
        """
        self.group.onecmd('help')


    def complete_stats(self, text, line, begidx, endidx):
        """
        stats command autocompletion
        """
        return self.stats.completenames(text, line, begidx, endidx)


    def do_stats(self, arg):
        """
        Exec stats command
        Statistics

        Parameters:
            arg: command arguments
        """
        self.stats.onecmd(arg)


    def help_stats(self):
        """
        Handle help for stats command
        """
        self.stats.onecmd('help')


    def complete_config(self, text, line, begidx, endidx):
        """
        config command autocompletion
        """
        return self.config.completenames(text, line, begidx, endidx)


    def do_config(self, arg):
        """
        Exec config command
        Operations on config

        Parameters:
            arg: command arguments
        """
        self.config.onecmd(arg)


    def help_config(self):
        """
        Handle help for config command
        """
        self.config.onecmd('help')


    def do_exit(self, arg):
        #pylint: disable=unused-argument,no-self-use
        """
        Exec exit command
        Exit AppQoS shell

        Parameters:
            arg: command arguments
        """
        return True


    def do_quit(self, arg):
        #pylint: disable=unused-argument,no-self-use
        """
        Exec quit command
        Quit AppQoS shell

        Parameters:
            arg: command arguments
        """
        return True


if __name__ == '__main__':

    PARSER = argparse.ArgumentParser()
    PARSER.add_argument('-D', '--disable_https_warnings', action='store_true',
                        help="Disable Unverified HTTPS request warnings")
    PARSER.add_argument('command', nargs=argparse.REMAINDER)
    CMD_ARGS = PARSER.parse_args()

    if CMD_ARGS.disable_https_warnings:
        # suppress warning from requests
        urllib3.disable_warnings(InsecureRequestWarning)

    SHELL = ShellMain()
    if os.path.isfile("cli.conf"):
        SHELL.onecmd("config load cli.conf")

    if CMD_ARGS.command:
        SHELL.onecmd(' '.join(CMD_ARGS.command))
    else:
        SHELL.cmdloop()

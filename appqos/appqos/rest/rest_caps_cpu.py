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
REST API module
RDT /caps/cpu requests.
"""
from flask_restful import Resource
from pqos.error import PqosErrorResource

from appqos.pqos_api import PQOS_API

class CapsCpus(Resource):
    """
    Handles /caps/cpu requests
    """

    @staticmethod
    def get():
        """
        Handles GET /caps/cpu request.

        Returns:
            response, status code
        """

        cpuinfo = PQOS_API.cpuinfo

        res = {
            "vendor": cpuinfo.get_vendor(),
            "cache": [],
            "core": []
        }

        cache_levels = []

        for level in [2, 3]:
            try:
                info = cpuinfo.get_cache_info(level=level)
                cache = {
                    "level": level,
                    "num_ways": info.num_ways,
                    "num_sets": info.num_sets,
                    "num_partitions": info.num_partitions,
                    "line_size": info.line_size,
                    "total_size": info.total_size,
                    "way_size": info.way_size
                }
                res["cache"].append(cache)

                cache_levels.append(level)
            except PqosErrorResource:
                pass

        sockets = cpuinfo.get_sockets()
        for socket in sockets:
            lcores = cpuinfo.get_cores(socket)
            for lcore in lcores:
                coreinfo = cpuinfo.get_core_info(lcore)

                info = {
                    "socket": coreinfo.socket,
                    "lcore": coreinfo.core,
                }

                if 2 in cache_levels:
                    info["L2ID"] = coreinfo.l2_id

                if 3 in cache_levels:
                    info["L3ID"] = coreinfo.l3_id

                res["core"].append(info)

        return res, 200

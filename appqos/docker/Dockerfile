################################################################################
# BSD LICENSE
#
# Copyright(c) 2021-2023, Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Intel Corporation nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
################################################################################

################################################################################
#       Base                                                                   #
################################################################################
FROM ubuntu:22.04 AS base
ENV DEBIAN_FRONTEND noninteractive

# update base image
RUN apt-get update -q && \
    apt-get -y purge --auto-remove \
        dirmngr \
        gpg-agent \
        libc-dev \
        libc-dev-bin && \
    apt-get upgrade -y --no-install-recommends && \
    apt-get install -y --no-install-recommends \
        python3-minimal \
        libpython3-stdlib && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

################################################################################
#       Build libpqos                                                          #
################################################################################
FROM base AS build-libpqos
ENV DEBIAN_FRONTEND noninteractive

# copy all intel-cmt-cat sources
COPY . /appqos_workspace

# install libpqos dependencies
RUN apt-get update -q && \
    apt-get install -y --no-install-recommends \
        make \
        gcc \
        libc-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /appqos_workspace
RUN make && make install

################################################################################
#       Prepare python environment                                             #
################################################################################
FROM base AS build-python
ENV DEBIAN_FRONTEND noninteractive

# python click dependency requires UTF charsets to be used
ARG LANG=C.UTF-8
ARG LC_ALL=C.UTF-8
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV WORKON_HOME=/venv/
ENV PYTHONUSERBASE=/python
ENV PIP_ROOT_USER_ACTION=ignore
ENV PIP_USER=1

# copy all intel-cmt-cat sources
COPY . /appqos_workspace

# install dependencies
RUN apt-get update -q && \
    apt-get install -y --no-install-recommends \
        python3-pip \
        git && \
    apt-get clean

RUN mkdir -p /python && \
    python3 -m pip install --no-cache-dir --no-warn-script-location --upgrade pip && \
    python3 -m pip install --no-cache-dir --no-warn-script-location /appqos_workspace/lib/python && \
    python3 -m pip install --no-cache-dir --no-warn-script-location /appqos_workspace/appqos && \
    python3 -m pip uninstall -y pip

################################################################################
#       Final image                                                            #
################################################################################
FROM base
ENV DEBIAN_FRONTEND noninteractive

# python click dependency requires UTF charsets to be used
ARG LANG=C.UTF-8
ARG LC_ALL=C.UTF-8
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV PYTHONUSERBASE=/python
ENV APPQOS_PORT=5000
ENV APPQOS_CONF_PATH=/opt/intel/appqos/appqos.conf
ENV APPQOS_ADDRESS=0.0.0.0

# copy libpqos
COPY --from=build-libpqos /usr/local/lib/libpqos.so* /lib/

# copy App QoS dependencies
COPY --from=build-python /python /python

# create config dir
RUN mkdir -p /opt/intel/appqos

# cleanup base packages
RUN apt-get -y purge --auto-remove --allow-remove-essential \
        grep \
        libpcre3 && \
    dpkg --force-depends -r libsqlite3-0 && \
    dpkg --force-depends -r libdb5.3 && \
    rm -rf /usr/bin/iconv && \
    rm -rf /usr/bin/nscd && \
    rm -rf /usr/sbin/chroot && \
    rm -rf /var/lib/apt/lists/*

CMD [ "sh", "-c", "python3 -m appqos -c ${APPQOS_CONF_PATH} --port ${APPQOS_PORT} --address ${APPQOS_ADDRESS}" ]

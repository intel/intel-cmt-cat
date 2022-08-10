################################################################################
#       Base                                                                   #
################################################################################
FROM ubuntu:22.04 AS base
ENV DEBIAN_FRONTEND noninteractive

# update base image
RUN apt-get update -q && \
    apt-get upgrade -y --no-install-recommends && \
    apt-get install -y --no-install-recommends \
        python3-minimal \
        libpython3-stdlib && \
    apt-get clean

# copy all intel-cmt-cat sources
COPY . /appqos_workspace

################################################################################
#       Build libpqos                                                          #
################################################################################
FROM base AS build-libpqos
ENV DEBIAN_FRONTEND noninteractive

# install libpqos dependencies
RUN apt-get install -y --no-install-recommends \
        make \
        gcc \
        libc-dev && \
    apt-get clean

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

# install dependencies
RUN apt-get install -y --no-install-recommends \
        python3-pip \
        git \
        make && \
    apt-get clean && \
    pip3 install pipenv

RUN mkdir -p /python && \
    PYTHONUSERBASE=/python PIP_USER=1 PIPENV_SYSTEM=1 VENV_DIR=/venv/ make -C /appqos_workspace/appqos setup

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
ENV APPQOS_CONF_PATH=ca/appqos.conf
ENV APPQOS_ADDRESS=0.0.0.0

# copy libpqos
COPY --from=build-libpqos /usr/local/lib/libpqos.so* /lib/

# copy App QoS dependencies
COPY --from=build-python /python /python

# cleanup base packages
RUN apt-get -y purge --auto-remove \
        dirmngr \
        gpg-agent \
        libc-dev \
        libc-dev-bin && \
    apt-get -y purge --auto-remove --allow-remove-essential \
        grep \
        libpcre3 && \
    dpkg --force-depends -r libsqlite3-0 && \
    dpkg --force-depends -r libdb5.3 && \
    rm -rf /usr/bin/iconv && \
    rm -rf /usr/bin/nscd && \
    rm -rf /usr/sbin/chroot && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /appqos_workspace/appqos
CMD python3 ./appqos -c ${APPQOS_CONF_PATH} --port ${APPQOS_PORT}

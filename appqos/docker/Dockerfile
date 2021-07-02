FROM ubuntu:20.04

ENV DEBIAN_FRONTEND noninteractive

# python click dependency requires UTF charsets to be used
# setting both for build and container runtime
ARG LANG=C.UTF-8
ARG LC_ALL=C.UTF-8
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV WORKON_HOME=/venv/
ENV APPQOS_PORT=5000
ENV APPQOS_CONF_PATH=ca/appqos.conf
ENV APPQOS_ADDRESS=0.0.0.0

# install dependencies
RUN apt-get update -q && \
    apt-get install -y make \
	gcc \
	python3-pip \
	python3 \
	git

# copy all intel-cmt-cat sources
COPY . /appqos_workspace

# install pqos
RUN cd /appqos_workspace && \
    make install && make clean

# install AppQoS
RUN pip3 install pipenv && \
    PIPENV_SYSTEM=1 VENV_DIR=/venv/ make -C /appqos_workspace/appqos setup

# cleanup build dependencies
RUN pip3 uninstall -y pipenv && \
    apt-get -y purge --auto-remove gcc g++ git python3-pip make dirmngr gpg-agent && \
    apt-get -y purge --auto-remove --allow-remove-essential grep libpcre3 gpgv gnupg2 && \
    rm -rf /usr/bin/iconv && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /appqos_workspace/appqos
CMD ./appqos.py -c ${APPQOS_CONF_PATH} --port ${APPQOS_PORT}

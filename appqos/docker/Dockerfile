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
    apt-get install -y make gcc python3-pip git

# copy all intel-cmt-cat sources
COPY . /appqos_workspace

# install pqos
RUN cd /appqos_workspace && \
    make install && make clean

# install AppQoS
RUN pip3 install pipenv && \
    VENV_DIR=/venv/ make -C /appqos_workspace/appqos setup

WORKDIR /appqos_workspace/appqos
CMD pipenv run ./appqos.py -c ${APPQOS_CONF_PATH} --port ${APPQOS_PORT}

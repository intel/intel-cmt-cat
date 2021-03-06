#!/bin/bash -e

echo -e "WARNING: TEST KEYS AND CERTIFICATES GENERATED BY THIS SCRIPT ARE FOR \
EVALUATING THE SETUP ONLY - NOT INTENDED FOR FOR A PRODUCTION USE\n"

set -x

# using test with 'sudo' because of case, where crt files are not visible for non-root users
if ! sudo test -f ./ca.crt; then
	# Generate root CA cert
	openssl req -nodes -x509 -newkey rsa:3072 -keyout ca.key -out ca.crt -subj "/O=AppQoS/OU=root/CN=localhost" 

    # giving access rights for the ca cert key only to the root user
    sudo chown root ca.key
fi

if ! sudo test -f ./appqos.crt; then
    openssl req -nodes -newkey rsa:3072 -keyout appqos.key -out appqos.csr -subj "/O=AppQoS/OU=AppQoS Server/CN=localhost"
    sudo openssl x509 -req -in appqos.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out appqos.crt

    # changing the ownership of the appqos cert and key - to match credentials of the user that will launch
    # the AppQoS server
    sudo chown root appqos.crt appqos.csr appqos.key
fi

if ! sudo test -f ./client_appqos.crt; then
    # Generate client_appqos cert and sign it
    openssl req -nodes -newkey rsa:3072 -keyout client_appqos.key -out client_appqos.csr -subj "/O=AppQoS/OU=AppQoS Client/CN=client_host"
    sudo openssl x509 -req -in client_appqos.csr -CA ca.crt -CAkey ca.key -CAserial ca.srl -out client_appqos.crt
fi

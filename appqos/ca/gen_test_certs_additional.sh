#!/bin/bash

# Create certificate for HAProxy
cat client_appqos.crt client_appqos.key > client_appqos.pem

# Restrict access rights for the HAProxy certificate only to the root user
sudo chmod 600 client_appqos.pem
sudo chown root:root client_appqos.pem

# Create certificate for browser
openssl pkcs12 -export -clcerts -in client_appqos.crt -inkey client_appqos.key -out client_appqos.p12

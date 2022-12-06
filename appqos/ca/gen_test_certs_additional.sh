#!/bin/bash

# Create certificate for HAProxy
cat client_appqos.crt client_appqos.key > client_appqos.pem

# Create certificate for browser
openssl pkcs12 -export -clcerts -in client_appqos.crt -inkey client_appqos.key -out client_appqos.p12

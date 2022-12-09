# App QoS Client

App QoS client is a web application that provides a Graphical User Interface (GUI) to configure Intel(R) Resource Director Technology (RDT). The application acts as a frontend to [App QoS](https://github.com/intel/intel-cmt-cat/tree/master/appqos), therefore requires a running instance of App QoS in order to make platform configuration changes via its REST API.

For more information on the App QoS client application, visit the wiki at: https://github.com/intel/intel-cmt-cat/wiki/AppQoS-Client  
_NOTE:_ App QoS client is intended for evaluating Intel(R) platform technologies and should not be deployed in production environments.

## Security
All communication between App QoS and the client application is sent securely over HTTPS (TLS v1.2 and later supported). Mutual TLS (mTLS) provides mutual authentication. This requires the generation of a client TLS certificate that can be imported into the browser in order to authenticate the client and establish a secure connection to the App QoS instance.

The App QoS client application temporarily stores data in the browser local storage upon login, and clears all data on logout. On the Google Chrome browser, application data can be viewed by pressing F12 to open the console window, navigating to "Application->Local Storage" and selecting the app site. Data can be manually deleted by right clicking on the site and clicking "Clear".

## Prerequisites

```
Node: 16.15.0 (node -v)
Package Manager: npm 8.12.1 (npm -v)
```

## Setup

### 1. AppQoS Client is available at GitHub

Clone the repo with: `git clone https://github.com/intel/intel-cmt-cat.git`

### 2. Configure mTLS Client Authentication in the browser (non-production)

_NOTE:_ For production deployment, certificates should be signed by a valid certificate authority.

#### Step 1 - Generate PKCS 12 certificate

The appqos/ca/ directory contains scripts to generate temporary self-signed certificates for testing purposes.
- `gen_test_certs.sh` script will generate appqos, CA and client certificates
- `gen_test_certs_additional.sh` script will generate a PKCS 12 certificate named _client_appqos.p12_ that can be imported into the browser to allow client authentication

#### Step 2 - Copy certificate to local machine
  To copy `client_appqos.p12` certificate to the local machine where the browser is running

#### Step 3 - Import `client_appqos.p12` certificate to the browser

  1. Open Google Chrome, click the **Customize and control Google Chrome** menu > go to **Settings** > at the left margin, click **Privacy and security** > click **Security**.
  2. Under **Advanced**, click **Manage certificates**.
  3. Click **Personal** > click **Import**.
  4. The Certificate Import Wizard starts. Click **Next**.
  5. Click **Browse** to navigate to the location where your certificate file is stored. If you don’t remember the location of the certificate, search for files with the extension .p12.
  6. Select the file that you want to import (make sure to choose the option **Personal Information Exchange** (\*.p12) or **All Files** to see those with the file extension .p12) > click **Open**.
  7. Click **Next**.
  8. Enter the password which was given while generating `client_appqos.p12`. Click **Next**.
  9. Click **Finish**.
  10. Click **OK**.

### 3. Install required packages

```
$ cd appqos_client
$ npm install
```

## Running using Angular Development Server

Run `npm start` or `ng serve` to start a dev server. Navigate to `http://localhost:4200/`.  
The application will automatically reload if you change any of the source files.

```
** Angular Live Development Server is listening on localhost:4200, open your browser on http://localhost:4200/ **


√ Compiled successfully.
```

## Building

Run `ng build` to build the project. The build artifacts will be stored in the `dist/` directory.

## Running unit tests

Run `ng test` to execute the unit tests via [Karma](https://karma-runner.github.io).

## Running unit tests with headless browser (no GUI)

Run `npm run test-headless` to execute the unit tests. Requires Chrome 59+.

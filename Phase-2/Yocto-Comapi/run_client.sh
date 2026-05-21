#!/bin/sh
export LD_LIBRARY_PATH=/usr/lib
export COMMONAPI_CONFIG="/RPiFiles/commonapi.ini"
export VSOMEIP_CONFIGURATION="/RPiFiles/vsomeip-client.json"
export VSOMEIP_APPLICATION_NAME="HelloClient"
export COMMONAPI_DEFAULT_BINDING="someip"
cd /RPiFiles && ./helloClient
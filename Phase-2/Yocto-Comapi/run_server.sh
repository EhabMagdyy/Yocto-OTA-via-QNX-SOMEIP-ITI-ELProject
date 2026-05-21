#!/bin/bash
rm -f /tmp/vsomeip*

export VSOMEIP_CONFIGURATION=$PWD/Config/vsomeip-server.json
export VSOMEIP_APPLICATION_NAME=helloService
export COMMONAPI_CONFIG=$PWD/commonapi.ini
export COMMONAPI_DEFAULT_BINDING=someip

$PWD/buildHostServer/helloServer

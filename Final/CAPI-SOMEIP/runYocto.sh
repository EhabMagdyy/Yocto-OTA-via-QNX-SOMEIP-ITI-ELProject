#!/bin/sh
export LD_LIBRARY_PATH=/usr/lib
export COMMONAPI_CONFIG="/capi-ota/commonapi.ini"
export VSOMEIP_CONFIGURATION="/capi-ota/yocto-config.json"
export VSOMEIP_APPLICATION_NAME="yoctoService"
export COMMONAPI_DEFAULT_BINDING="someip"
cd /capi-ota && ./yoctoService
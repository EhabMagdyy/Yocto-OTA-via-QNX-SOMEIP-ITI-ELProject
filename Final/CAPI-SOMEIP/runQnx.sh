#!/bin/sh
export LD_LIBRARY_PATH=/system/usr/lib
export VSOMEIP_CONFIGURATION=/system/capi-ota/qnx-config.json
export VSOMEIP_APPLICATION_NAME=qnxService
export VSOMEIP_PLUGIN_PATH=/system/usr/lib

/system/capi-ota/qnxService

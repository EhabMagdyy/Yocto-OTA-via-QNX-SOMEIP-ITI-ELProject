#!/bin/sh
export LD_LIBRARY_PATH=/usr/lib:/usr/usr/lib:/system/usr/lib
export VSOMEIP_CONFIGURATION=/system/comapiclient/vsomeip-client.json
export VSOMEIP_APPLICATION_NAME=HelloClient
export VSOMEIP_PLUGIN_PATH=/usr/usr/lib

/system/comapiclient/helloClient

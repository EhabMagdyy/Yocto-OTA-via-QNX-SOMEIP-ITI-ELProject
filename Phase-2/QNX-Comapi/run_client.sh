#!/bin/bash
export VSOMEIP_CONFIGURATION=$PWD/Config/vsomeip-client.json
export VSOMEIP_APPLICATION_NAME=helloClient
$PWD/buildHostServer/helloClient

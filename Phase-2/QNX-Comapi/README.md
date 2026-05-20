# CommonAPI SOME/IP on QNX With Host communication

---

## System Architecture

```
┌─────────────────────┐                      ┌───────────────────────┐
│     Host (Linux)    │◄──── Ethernet ────►  │     RPi4 (QNX 8.0)    │
│   IP: 192.168.50.1  │      SOME/IP         │   IP: 192.168.50.100  │ 
│   CommonAPI Server  │                      │    CommonAPI Client   │
└─────────────────────┘                      └───────────────────────┘
```

---

## Project Structure

```
.
├── CMakeLists.txt                          # Build script: compiles QNX client (cross-compile) and Linux host
├── Config
│   ├── vsomeip-client.json                 # vsomeip runtime config for the client: its unicast IP, app name, service discovery settings
│   └── vsomeip-server.json                 # vsomeip runtime config for the server: its unicast IP, network interface, service registration
├── fidl
│   ├── hello.fdepl                         # SOME/IP deployment spec: assigns Service ID, Method ID, Instance ID, and server unicast address
│   ├── hello.fidl                          # Interface definition: declares the HelloWorld service and sayHello() method signature
│   └── src-gen
│       └── v1
│           └── commonapi
│               ├── Hello.hpp                  # [generated] Core interface: defines the abstract HelloWorld interface class
│               ├── HelloProxyBase.hpp         # [generated] Proxy base: declares the proxy-side sayHello() method signatures
│               ├── HelloProxy.hpp             # [generated] Proxy template: client-side handle used to call remote methods
│               ├── HelloSomeIPCatalog.json    # [generated] SOME/IP catalog: maps interface names to Service/Instance IDs at runtime
│               ├── HelloSomeIPDeployment.cpp  # [generated] Deployment impl: registers SOME/IP serialization rules with the runtime
│               ├── HelloSomeIPDeployment.hpp  # [generated] Deployment header: declares deployment descriptors for the interface
│               ├── HelloSomeIPProxy.cpp       # [generated] Proxy impl: serializes outgoing calls and deserializes responses over SOME/IP
│               ├── HelloSomeIPProxy.hpp       # [generated] Proxy header: SOME/IP-specific proxy class declaration
│               ├── HelloSomeIPStubAdapter.cpp # [generated] Stub adapter impl: deserializes incoming SOME/IP requests and dispatches to server logic
│               ├── HelloSomeIPStubAdapter.hpp # [generated] Stub adapter header: bridges vsomeip transport layer to the CommonAPI stub
│               ├── HelloStubDefault.hpp       # [generated] Default stub: base class with no-op method implementations to override
│               └── HelloStub.hpp              # [generated] Stub interface: abstract class the server implementation must inherit from
├── run_client.sh                         # Convenience script: sets VSOMEIP_CONFIGURATION and VSOMEIP_APPLICATION_NAME then launches the client on host
├── run_server.sh                         # Convenience script: sets VSOMEIP_CONFIGURATION and VSOMEIP_APPLICATION_NAME then launches the server on host
└── src
    ├── Client.cpp                       # QNX client main: builds a proxy, waits for service discovery, then calls sayHello() in a loop
    ├── eventfd_stub.c                   # QNX workaround: implements the missing eventfd() syscall using a pipe, required by vsomeip
    └── Server.cpp                       # Linux server main: instantiates HelloStubImpl, registers the service, and runs until interrupted
```

---

## Build Instructions

### 1. Generate CommonAPI files
```sh
cd fidl
ccgen -sk hello.fidl
csgen hello.fdepl

# note:
# alias csgen='~/Downloads/commonapi_someip_generator/commonapi-someip-generator-linux-x86_64'
# alias ccgen='~/Downloads/commonapi_core_generator/commonapi-core-generator-linux-x86_64'
```

### 2. Host Server

```bash
cd ..
# Use the server CMakeLists, by uncommenting it in the CMakeList.txt & Commenting the client CMakeList.txt section
cmake -B buildHostServer -S . -DCMAKE_BUILD_TYPE=Release
cmake --build buildHostServer -j$(nproc)
```

### 3. QNX Client - Cross-Compilation

```bash
# Source QNX SDP environment
source ~/ITI/QNX/repo/qnx800/qnxsdp-env.sh

# Build with QNX toolchain
cmake -B build-qnx -DCMAKE_TOOLCHAIN_FILE=/home/ehab/qnxCTI/src/qnx_aarch64_toolchain.cmake -DCMAKE_BUILD_TYPE=Release -S .
cmake --build build-qnx -j$(nproc)
```

---

## Run Server & Client on host for testing

### under then `buildHostServer` we have `helloClient` and `helloServer`

```sh
ls buildHostServer/
# CMakeCache.txt  CMakeFiles  cmake_install.cmake  helloClient  helloServer  Makefile
```

### from two separate terminals run:
```sh
./run_server.sh
```

```sh
./run_client.sh
```

---

## Deploy to RPi4

### Transfer binaries & config
```bash
# 1. Copy your application binary and JSON config
scp build-qnx/helloClient Config/vsomeip-client.json root@192.168.50.100:/system/comapiclient/

# 2. Copy the cross-compiled vsomeip and Boost libraries to the QNX lib path
scp libvsomeip3.so* libboost_system.so* libboost_thread.so* root@192.168.50.100:/usr/lib/
scp /home/ehab/qnxCTI/src/stage/usr/lib/libboost_thread.so.1.82.0 root@192.168.50.100:/system/usr/lib/ 
scp /home/ehab/qnxCTI/src/stage/usr/lib/libboost_system.so.1.82.0 root@192.168.50.100:/system/usr/lib/
scp /home/ehab/qnxCTI/src/stage/usr/lib/libboost_thread.so.1.82.0 root@192.168.50.100:/system/usr/lib/  
scp /home/ehab/qnxCTI/src/stage/usr/lib/libboost_filesystem.so.1.82.0 root@QNX_IP:/usr/usr/lib/
scp /home/ehab/qnxCTI/src/stage/usr/lib/libboost_*.so* root@192.168.50.100:/usr/usr/lib/

# Force Multicast Route onto the QNX Interface
sudo route add -net 224.0.0.0 netmask 240.0.0.0 dev eno1
# Without running that command after a reboot, your SOME/IP server attempts to broadcast its Service Discovery OFFER packets out into your home Wi-Fi network 
# instead of sending them down the physical Ethernet cable (eno1) to your QNX board. Running the command manually forces the kernel to redirect that traffic.
```

### RPi4 setup
```bash
# SSH into RPi5
ssh root@192.168.50.100

# Create symlinks for CommonAPI libraries
ln -sf /usr/usr/lib/libCommonAPI.so.3 /usr/usr/lib/libCommonAPI.so.3.2.4
ln -sf /usr/usr/lib/libCommonAPI-SomeIP.so.3 /usr/usr/lib/libCommonAPI-SomeIP.so.3.2.4
ln -sf /system/usr/lib/libCommonAPI.so.3 /system/usr/lib/libCommonAPI.so.3.2.4
ln -sf /system/usr/lib/libCommonAPI-SomeIP.so.3 /system/usr/lib/libCommonAPI-SomeIP.so.3.2.4
ln -sf /system/usr/lib/libboost_thread.so.1.82.0 /system/usr/lib/libboost_thread.so
ln -sf /system/usr/lib/libboost_thread.so.1.82.0 /system/usr/lib/libboost_thread.so.1.82.0
ln -sf /system/usr/lib/libboost_system.so.1.82.0 /system/usr/lib/libboost_system.so
ln -sf /system/usr/lib/libboost_thread.so.1.82.0 /system/usr/lib/libboost_thread.so
ln -sf /usr/usr/lib/libCommonAPI-SomeIP.so /usr/local/lib/commonapi/libCommonAPI-SomeIP.so
ln -sf /usr/usr/lib/libCommonAPI-SomeIP.so.3 /usr/local/lib/commonapi/libCommonAPI-SomeIP.so.3
ln -sf /usr/usr/lib/libvsomeip3.so.3 /usr/lib/libvsomeip3.so.3
ln -sf /usr/usr/lib/libvsomeip3.so /usr/lib/libvsomeip3.so
ln -sf /usr/usr/lib/libvsomeip3-cfg.so.3 /usr/lib/libvsomeip3-cfg.so.3
ln -sf /usr/usr/lib/libvsomeip3-cfg.so /usr/lib/libvsomeip3-cfg.so
ln -sf /usr/usr/lib/libboost_filesystem.so.1.82.0 /usr/lib/libboost_filesystem.so.1.82.0
ln -sf /usr/usr/lib/libboost_*.so* /usr/lib/

# Create vsomeip socket directory
mkdir -p /var/vsomeip
chmod 777 /var/vsomeip

# CommonAPI runtime config
cat > /etc/commonapi.ini << 'EOF'
[default]
binding=someip
libpath=/usr/usr/lib
EOF


```

---

## Running the System

```bash
./run_server.sh                        # On Host
/system/comapiclient/run_client.sh     # On RPi4

# To Kill on QNX
CTRL+Z
slay -s KILL helloClient

# To Kill
CTRL+Z
sudo killall -9 helloServer

# to check if qnx broadcasting, run on host
sudo tcpdump -i eno1 port 30490 -nvv -XX
```

---

## May be needed

```sh
[root@ehabqnx /system/comapiclient]# ls /usr/lib | grep some
libvsomeip3-cfg.so
libvsomeip3-cfg.so.3
libvsomeip3.so
libvsomeip3.so.3
```
```sh
[root@ehabqnx /system/comapiclient]# ls /usr/usr/lib | grep some
libvsomeip3-cfg.so
libvsomeip3-cfg.so.3
libvsomeip3-e2e.so
libvsomeip3-e2e.so.3
libvsomeip3-sd.so
libvsomeip3-sd.so.3
libvsomeip3.so
libvsomeip3.so.3
```
```sh
[root@ehabqnx /system/comapiclient]# ls /system/usr/lib | grep some
libvsomeip3-cfg.so
libvsomeip3-cfg.so.3
libvsomeip3-e2e.so
libvsomeip3-e2e.so.3
libvsomeip3-sd.so
libvsomeip3-sd.so.3
libvsomeip3.so
libvsomeip3.so.3
```
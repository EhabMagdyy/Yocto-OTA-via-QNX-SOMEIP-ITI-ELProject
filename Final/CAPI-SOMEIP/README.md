# CommonAPI SOME/IP on QNX With Host communication

## Build Instructions

### Generate CommonAPI files
```sh
cd fidl
ccgen -sk ota.fidl
csgen ota.fdepl

# note:
# alias csgen='~/Downloads/commonapi_someip_generator/commonapi-someip-generator-linux-x86_64'
# alias ccgen='~/Downloads/commonapi_core_generator/commonapi-core-generator-linux-x86_64'
```

---

### QNX - Cross-Compilation

```bash
cd ..
# Source QNX SDP environment
source ~/qnx800/qnxsdp-env.sh

# Build with QNX toolchain
cmake -B build-qnx -DCMAKE_TOOLCHAIN_FILE=/home/ehab/qnxCTI/src/qnx_aarch64_toolchain.cmake -DCMAKE_BUILD_TYPE=Release -S .
cmake --build build-qnx -j$(nproc)
```

---

### Yocto - Cross-Compilation

```bash
mkdir build-yocto && cd build-yocto

cmake \
    -DCMAKE_TOOLCHAIN_FILE=~/rpi-toolchain.cmake \
    -DCommonAPI_DIR=~/rpi-sysroot/usr/lib/cmake/CommonAPI-3.2.4 \
    -DCommonAPI-SomeIP_DIR=~/rpi-sysroot/usr/lib/cmake/CommonAPI-SomeIP-3.2.4 \
    -Dvsomeip3_DIR=~/rpi-sysroot/usr/lib/cmake/vsomeip3 \
    ..

make -j$(nproc)
```

---

## Setup Instructions

### QNX setup
```bash
# Create vsomeip socket directory
mkdir -p /var/vsomeip
chmod 777 /var/vsomeip

# Copy executable, config and script
scp build-qnx/qnxService     root@192.168.50.100:/system/capi-ota/
scp runQnx.sh                root@192.168.50.100:/system/capi-ota/
scp config/qnx-config.json   root@192.168.50.100:/system/capi-ota/

# CommonAPI runtime config
cat > /etc/commonapi.ini << 'EOF'
[default]
binding=someip
libpath=/system/usr/lib
EOF
```

---

### Yocto setup

```bash
# Deploy binary and config files
scp build-yocto/yoctoService     root@192.168.50.50:/capi-ota/
scp config/yocto-config.json     root@192.168.50.50:/capi-ota/
scp commonapi.ini                root@192.168.50.50:/capi-ota/
scp runYocto.sh                  root@192.168.50.50:/capi-ota/
```

> vsomeip service discovery uses UDP multicast. Add the route once per boot:

```bash
# Check interface name
ip link show

# Add route (replace eth0 with your actual interface if different)
ip route add 224.0.0.0/4 dev eth0
```

---

## Running the System

```bash
/system/capi-ota/runQnx.sh

# To Kill on QNX
CTRL+Z
slay -s KILL runQnx

# To Kill
CTRL+Z
sudo killall -9 runQnx
```

---

### Run Yocto

```bash
/capi-ota/runYocto.sh
```

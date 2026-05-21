# vsomeip + CommonAPI on RPi3B+ with Yocto

## Overview

This guide documents the complete process of fetching, building, and deploying
`vsomeip`, `capicxx-core-runtime`, and `capicxx-someip-runtime` to a Raspberry Pi 3B+
using Yocto devtool, and running a CommonAPI SomeIP client/server between the RPi and a Linux host.

**Setup:**
- RPi3B+ IP: `192.168.50.100` (client)
- Host PC IP: `192.168.50.1` (server)
- Yocto distro: `ros` / Scarthgap
- Target: `raspberrypi3-64` (aarch64 / cortexa53)

---

## Part 1 — Host Machine: Yocto devtool

### 1.1 Source the Build Environment

```bash
cd ~/Documents/ITI_9Months/Yocto
source poky/oe-init-build-env build-rpi
```

### 1.2 Clone Sources Manually

> GitHub fetch via devtool may fail behind firewalls/proxies.
> Clone manually first, then point devtool to the local path.

```bash
git clone https://github.com/COVESA/vsomeip.git sources/vsomeip
git clone https://github.com/COVESA/capicxx-core-runtime.git sources/capicxx-core-runtime
git clone https://github.com/COVESA/capicxx-someip-runtime.git sources/capicxx-someip-runtime
```

### 1.3 Add to Workspace

```bash
devtool add vsomeip ~/Documents/ITI_9Months/Yocto/sources/vsomeip
devtool add capicxx-core-runtime ~/Documents/ITI_9Months/Yocto/sources/capicxx-core-runtime
devtool add capicxx-someip-runtime ~/Documents/ITI_9Months/Yocto/sources/capicxx-someip-runtime
```

### 1.4 Build

```bash
devtool build vsomeip
devtool build capicxx-core-runtime
devtool build capicxx-someip-runtime
```

### 1.5 Deploy to Running RPi

```bash
devtool deploy-target vsomeip root@192.168.50.100
devtool deploy-target capicxx-core-runtime root@192.168.50.100
devtool deploy-target capicxx-someip-runtime root@192.168.50.100
```

### 1.6 Fix Dirty Source Tree (capicxx-core-runtime)

`devtool finish` requires a clean git tree. Commit any changes first:

```bash
cd ~/Documents/ITI_9Months/Yocto/build-rpi/workspace/sources/capicxx-core-runtime

git add include/CommonAPI/Runtime.hpp include/CommonAPI/Types.hpp
git add 0001-add-missing-string-include.patch
git commit -m "fix: add missing string include"

cd ~/Documents/ITI_9Months/Yocto/build-rpi
```

### 1.7 Finish Recipes into Layer

```bash
mkdir -p ~/Documents/ITI_9Months/Yocto/meta-customimage/recipes-connectivity/vsomeip
mkdir -p ~/Documents/ITI_9Months/Yocto/meta-customimage/recipes-connectivity/capicxx-core-runtime
mkdir -p ~/Documents/ITI_9Months/Yocto/meta-customimage/recipes-connectivity/capicxx-someip-runtime

devtool finish vsomeip \
  /home/ehab/Documents/ITI_9Months/Yocto/meta-customimage/recipes-connectivity/vsomeip/

devtool finish capicxx-someip-runtime \
  /home/ehab/Documents/ITI_9Months/Yocto/meta-customimage/recipes-connectivity/capicxx-someip-runtime/

devtool finish capicxx-core-runtime \
  /home/ehab/Documents/ITI_9Months/Yocto/meta-customimage/recipes-connectivity/capicxx-core-runtime/
```

Verify workspace is clean:

```bash
devtool status
# Should only show other recipes (e.g. qt-apps), not vsomeip/capicxx
```

### 1.8 Fix Recipe Parse Error

`devtool finish` may leave a duplicate `SRC_URI:append` line. Remove it:

```bash
sed -i '/SRC_URI:append = " file:\/\/0001-add-missing-string-include.patch"/d' \
  ~/Documents/ITI_9Months/Yocto/meta-customimage/recipes-connectivity/capicxx-core-runtime/capicxx-core-runtime_git.bb

# Verify — should show only one patch entry
grep "patch" \
  ~/Documents/ITI_9Months/Yocto/meta-customimage/recipes-connectivity/capicxx-core-runtime/capicxx-core-runtime_git.bb
```

### 1.9 Build & Deploy Boost (Missing Runtime Dependency)

vsomeip depends on boost at runtime but it wasn't in the image:

```bash
bitbake boost

BOOST_DIR=~/Documents/ITI_9Months/Yocto/shared-build/tmp-glibc/work/cortexa53-ros-linux/boost/1.84.0/packages-split

scp $BOOST_DIR/boost-filesystem/usr/lib/libboost_filesystem.so.1.84.0 root@192.168.50.100:/usr/lib/
scp $BOOST_DIR/boost-system/usr/lib/libboost_system.so.1.84.0       root@192.168.50.100:/usr/lib/
scp $BOOST_DIR/boost-thread/usr/lib/libboost_thread.so.1.84.0       root@192.168.50.100:/usr/lib/
scp $BOOST_DIR/boost-chrono/usr/lib/libboost_chrono.so.1.84.0       root@192.168.50.100:/usr/lib/
scp $BOOST_DIR/boost-atomic/usr/lib/libboost_atomic.so.1.84.0       root@192.168.50.100:/usr/lib/
```

### 1.10 Cross-Compile helloClient for RPi

```bash
cd ~/Documents/ITI_9Months/Courses-Project/Embedded-Linux/Phase-2/Yocto-Comapi
mkdir build-rpi && cd build-rpi

cmake \
    -DCMAKE_TOOLCHAIN_FILE=~/rpi-toolchain.cmake \
    -DCommonAPI_DIR=~/rpi-sysroot/usr/lib/cmake/CommonAPI-3.2.4 \
    -DCommonAPI-SomeIP_DIR=~/rpi-sysroot/usr/lib/cmake/CommonAPI-SomeIP-3.2.4 \
    -Dvsomeip3_DIR=~/rpi-sysroot/usr/lib/cmake/vsomeip3 \
    ..

make -j$(nproc)

# Deploy binary and config files
scp build-rpi/helloClient          root@192.168.50.100:/RPiFiles/
scp Config/vsomeip-client.json     root@192.168.50.100:/RPiFiles/
scp commonapi.ini                  root@192.168.50.100:/RPiFiles/
scp run_client.sh                  root@192.168.50.100:/RPiFiles/
```

### 1.11 Run the Server

```bash
cd ~/Documents/ITI_9Months/Courses-Project/Embedded-Linux/Phase-2/Yocto-Comapi

# Add multicast route (once per boot)
sudo ip route add 224.0.0.0/4 dev eno1

# Clean any stale vsomeip sockets before every run
sudo rm -f /tmp/vsomeip*

./run_server.sh

# Check if packets received from client
sudo tcpdump -i eno1 udp port 30490 -nvv

# if packets received but not for server
sudo ip route add 224.0.0.0/4 dev eno1

# if needed
sudo ufw allow 30490/udp
sudo ufw allow 30500/udp

# Or check if ufw is blocking
sudo ufw status
```

#### run_server.sh (final version)

```bash
#!/bin/bash
rm -f /tmp/vsomeip-0 /tmp/vsomeip-*

export VSOMEIP_CONFIGURATION=$PWD/Config/vsomeip-server.json
export VSOMEIP_APPLICATION_NAME=helloService
export COMMONAPI_CONFIG=$PWD/commonapi.ini
export COMMONAPI_DEFAULT_BINDING=someip

$PWD/buildHostServer/helloServer
```

> **Watch for:** `helloService [info] OFFER(1234): [1234.5678:1.0]`
> This confirms the server is broadcasting via service discovery.

---

## Part 2 — RPi3B+

### 2.1 Fix Script Line Endings & Shebang

Scripts edited on Windows have `\r\n` endings and may use `#!/bin/bash`
which doesn't exist on a minimal Yocto image (BusyBox only has `sh`):

```bash
# Fix CRLF line endings
sed -i 's/\r//' /RPiFiles/run_client.sh

# Fix shebang for BusyBox
sed -i '1s|#!/bin/bash|#!/bin/sh|' /RPiFiles/run_client.sh
```

### 2.2 Fix Library Search Paths

CommonAPI is compiled with `/usr/local/lib/commonapi` hardcoded as the plugin
search path, but devtool installs libs to `/usr/lib`. Create symlinks:

```bash
mkdir -p /usr/local/lib
mkdir -p /usr/local/lib/commonapi

# Symlink all relevant libs into the expected paths
for lib in /usr/lib/libvsomeip3*.so* \
           /usr/lib/libCommonAPI*.so* \
           /usr/lib/libboost*.so*; do
    ln -sf $lib /usr/local/lib/$(basename $lib)
    ln -sf $lib /usr/local/lib/commonapi/$(basename $lib) 2>/dev/null
done
```

### 2.3 Create Boost SONAME Symlinks

The boost `.so.1.84.0` files need their short `.so` symlinks:

```bash
ln -sf /usr/lib/libboost_filesystem.so.1.84.0 /usr/lib/libboost_filesystem.so
ln -sf /usr/lib/libboost_system.so.1.84.0 /usr/lib/libboost_system.so
ln -sf /usr/lib/libboost_thread.so.1.84.0 /usr/lib/libboost_thread.so
ln -sf /usr/lib/libboost_chrono.so.1.84.0 /usr/lib/libboost_chrono.so
ln -sf /usr/lib/libboost_atomic.so.1.84.0 /usr/lib/libboost_atomic.so
```

### 2.4 Add Multicast Route

vsomeip service discovery uses UDP multicast. Add the route once per boot:

```bash
# Check interface name
ip link show

# Add route (replace eth0 with your actual interface if different)
ip route add 224.0.0.0/4 dev eth0
```

### 2.5 Run the Client

```bash
/RPiFiles/run_client.sh
```

#### run_client.sh (final version)

```bash
#!/bin/sh
export LD_LIBRARY_PATH=/usr/lib
export COMMONAPI_CONFIG="/RPiFiles/commonapi.ini"
export VSOMEIP_CONFIGURATION="/RPiFiles/vsomeip-client.json"
export VSOMEIP_APPLICATION_NAME="HelloClient"
export COMMONAPI_DEFAULT_BINDING="someip"
cd /RPiFiles && ./helloClient
```

#### commonapi.ini

```ini
[commonapi]
binding = someip
```

---

## Part 3 — Baking Libraries Into the Image (Permanent Fix)

Instead of manually deploying libs after every image build, add them permanently.

### Option A — Add to `local.conf` (Quick, per-build)

```bitbake
# In build-rpi/conf/local.conf
IMAGE_INSTALL:append = " \
    vsomeip \
    capicxx-core-runtime \
    capicxx-someip-runtime \
    boost-filesystem \
    boost-system \
    boost-thread \
    boost-chrono \
    boost-atomic \
"
```

### Option B — Add to Your Custom Image Recipe (Recommended)

Edit your image `.bb` file in `meta-customimage`:

```bash
# Find your image recipe
find ~/Documents/ITI_9Months/Yocto/meta-customimage -name "*.bb" | grep image
```

Then add inside it:

```bitbake
IMAGE_INSTALL:append = " \
    vsomeip \
    capicxx-core-runtime \
    capicxx-someip-runtime \
    boost-filesystem \
    boost-system \
    boost-thread \
    boost-chrono \
    boost-atomic \
"
```

### Option C — Fix vsomeip RDEPENDS in Recipe (Best Practice)

The root cause is that the `vsomeip_git.bb` recipe doesn't declare boost as a
runtime dependency. Fix it so Yocto automatically pulls boost whenever vsomeip
is installed:

```bash
# Edit the vsomeip recipe
nano ~/Documents/ITI_9Months/Yocto/meta-customimage/recipes-connectivity/vsomeip/vsomeip_git.bb
```

Add or update these variables:

```bitbake
DEPENDS = "boost cmake-native"

RDEPENDS:${PN} = " \
    boost-filesystem \
    boost-system \
    boost-thread \
    boost-chrono \
    boost-atomic \
"
```

### Rebuild the Image

After any of the above changes:

```bash
cd ~/Documents/ITI_9Months/Yocto/build-rpi
bitbake <your-image-name>
# e.g. bitbake core-image-minimal
```

Flash the new image to the RPi. All libraries will be present out of the box —
no manual `devtool deploy-target`, `scp`, or symlink steps needed.

---

## Troubleshooting Reference

| Symptom | Cause | Fix |
|---|---|---|
| `exit code 128` on devtool fetch | Firewall/proxy blocks GitHub | Clone manually, use local path |
| `./runService.sh: not found` | Windows CRLF line endings | `sed -i 's/\r//' script.sh` |
| `not found` after fixing CRLF | `#!/bin/bash` missing on minimal image | Change shebang to `#!/bin/sh` |
| `1 Configuration module could not be loaded!` | Missing `libboost_filesystem.so.1.84.0` | Build and deploy boost |
| `other routing manager present` | Stale `/tmp/vsomeip-0` socket | `rm -f /tmp/vsomeip-*` |
| `Service not available after timeout` | Missing multicast route | `ip route add 224.0.0.0/4 dev <iface>` |
| `Using default binding 'dbus'` | `COMMONAPI_DEFAULT_BINDING` not exported in script | Add export inside the `.sh` file |
| Recipe parse error: patch not found | Duplicate `SRC_URI:append` from devtool | Remove the duplicate line with `sed` |
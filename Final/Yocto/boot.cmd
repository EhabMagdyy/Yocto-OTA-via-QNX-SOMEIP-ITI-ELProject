fdt addr ${fdt_addr} && fdt get value bootargs /chosen bootargs

if test "${active_slot}" = "a"; then
    setenv rootpart 2
else
    setenv rootpart 3
fi

setenv bootargs "${bootargs} dwc_otg.lpm_enable=0 root=/dev/mmcblk0p${rootpart} rootfstype=ext4 rootwait  net.ifnames=0 console=ttyAMA0,115200"

fatload mmc 0:1 ${kernel_addr_r} Image
if test ! -e mmc 0:1 uboot.env; then saveenv; fi
booti ${kernel_addr_r} - ${fdt_addr}

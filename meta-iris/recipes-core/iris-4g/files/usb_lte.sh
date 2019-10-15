#!/bin/sh

# We must block the older Huawei LTE dongle on the IH304 to avoid issues
model=$(cat /tmp/mfg/config/model)
if [ ${model} = "IH304" ] && [ $1 = "12d1" ]; then
    echo "Huawei LTE dongle not supported on this hardware"
    exit 0
fi

# On add, create file with ID, remove file when dongle is removed
case $ACTION in
    add)
	echo $1:$2 > /var/volatile/tmp/lte_dongle
        ;;
    remove)
	rm -f /var/volatile/tmp/lte_dongle
        ;;
esac

exit 0

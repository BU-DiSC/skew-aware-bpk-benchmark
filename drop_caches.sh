cat /proc/sys/vm/drop_caches
echo 3 > /proc/sys/vm/drop_caches
echo -n "Should be 3: "
cat /proc/sys/vm/drop_caches

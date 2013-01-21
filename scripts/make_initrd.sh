rm ../myinitrd.gz
find . | cpio --create --format='newc' > ../myinitrd
gzip ../myinitrd
chmod 777 ../myinitrd.gz

if [ ( ! -d initramfs ) -o  ( ! -d test ) ]; then 
	echo "please, execute scripts from tf201-dev root."; 
	exit;
fi
rm test/myinitrd.gz
cd initramfs
find . | cpio --create --format='newc' > ../test/myinitrd
cd ..
gzip test/myinitrd
chmod 777 test/myinitrd.gz

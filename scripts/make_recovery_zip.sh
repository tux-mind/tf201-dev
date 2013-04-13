script_dir=$(dirname $0)
cd "$script_dir"

cd ../kernel_chooser/edify
cp ../../test/test.blob ./kernel.blob
zip -r ../../test/test.zip *
rm kernel.blob
#adb push ../test/recovery.zip /sdcard/
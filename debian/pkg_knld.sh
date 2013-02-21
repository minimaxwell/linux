#!/bin/bash

#===============================================================
# Note: function to generate knl debian installer. The schedulling of the 
#       dtb file is very important. It determine the schedulling for the storage
#	in the FLASH BOOT
#	 1 => Path and name for the KNL binary file
#	 2 => Board model (MCR3000_1G, MCR3000_2G, ...)
#	 3 => Path and name for the 1st dtb file
# opt	 4 => Path and name for the 2nd dtb file
#	 etc....
#===============================================================
package_knld()
{
	#===== Check parameters
	if [ "$#" -lt "3" ] ; then
		echo "pkg_knl: Error bad number of parameters"
		return 2
	fi

	#===== Construct directory path
	ROOT_PATH=`pwd`
	PATH_PKG=$ROOT_PATH/knld
	PATH_LINUX=$1

	if [ ! -f $PATH_LINUX ] ; then
		echo "pkg_knld: Error file not found [$PATH_LINUX]"
		return 2
	fi
	
	# on travaille sur un repertoire clean	
	rm -rf $PATH_PKG

	# copy LINUX
	mkdir -p $PATH_PKG/tmp/root
	chmod 1777 $PATH_PKG/tmp
	chmod 700 $PATH_PKG/tmp/root
	cp $PATH_LINUX $PATH_PKG/tmp/root
	
	# copy modules
	cp -a lib etc usr $PATH_PKG

	# verifying DTB files exist
	for (( i = 3; i <= $#; i += 1)); do
		echo "dtb: ${!i}"
		if [ ! -f ${!i} ]; then
			echo "pkg_knld: Error ${!i} do not exist"
			return 2
		fi
	done

	# Board type
	board=$2

	# Creating FLASH image for dtb
	case ${board} in
	"MCR3000_1G")
		dd if=/dev/zero of=$PATH_PKG/tmp/root/dtb.bin bs=1 count=64K;;

	"CMPC885")
		dd if=/dev/zero of=$PATH_PKG/tmp/root/dtb.bin bs=1 count=192K;;
	*)
		echo "pkg_knld: Error board type unknown"
		return 2;;
	esac

	# concatenating DTB file
	offset=0
	for (( i = 3; i <= $#; i += 1)); do
		dd if=${!i} of=$PATH_PKG/tmp/root/dtb.bin conv=notrunc bs=1 seek=${offset}K
		offset=$(( offset + 16 ))
	done

	linux_file=`basename ${PATH_LINUX}`
	#===== Making package
	mkdir -p $PATH_PKG/DEBIAN
	cp ./pkg_knld.control 	$PATH_PKG/DEBIAN/control
	cp ./pkg_knld.postinst 	$PATH_PKG/DEBIAN/postinst
	cp ./pkg_knld.preinst 	$PATH_PKG/DEBIAN/preinst

	# replace with the good file name
	sed -i "s#LINUX_FILE_NAME#/tmp/root/${linux_file}#g" $PATH_PKG/DEBIAN/postinst
	sed -i "s#LINUX_FILE_NAME#${linux_file}#g" $PATH_PKG/DEBIAN/preinst
	sed -i "s#LINUX_FILE_NAME#${linux_file}#g" $PATH_PKG/DEBIAN/control

	# get KNL version
	version=`basename ${PATH_LINUX} | sed "s/.lzma//" | cut -d"-" -f2`
	sed -i "s/^Version:/Version: ${version}/" $PATH_PKG/DEBIAN/control

	# replace with the good model
	case ${board} in
	"MCR3000_1G")
		sed -i "s/KNL_MODEL_SUPPORT/MCR3000/g" $PATH_PKG/DEBIAN/preinst;;

	"CMPC885")
		sed -i "s/KNL_MODEL_SUPPORT/MCR3000_2G MIAE CMPC885/g" $PATH_PKG/DEBIAN/preinst;;

	*)
		echo "pkg_knl: Error model of board unknown"
		return 2;;
	esac

	chmod 755 $PATH_PKG/DEBIAN/post*
	chmod 755 $PATH_PKG/DEBIAN/pre*
	dpkg-deb --build knld

	# rename the package
	mv "knld.deb" "KNLD-${board}-${version}.deb"

	return 0
}

usage_pkg_knld()
{
	echo "Usage: $0 KNL_BINARY DTB1 [DTB2 DTB3....]"
	echo "   - Warning: the name of the KNL_BINARY file must be"
	echo "   in the format KNL-BOARD-VERSION-FORMAT. Example:"
	echo "   KNL-CMPC885-3.6.2-uImage"
	echo "   - In the FLASH the DTB files are flashed in the same"
	echo "   order as the parameters DTB1 DTB2 DTB3....."
}


if [ `basename $0` = "pkg_knld.sh" ]; then

	#===== By parameters
	if [ -z "${1}" ]; then
		usage_pkg_knld
		exit 0
	fi
	LINUX_BIN=${1}

	#===== Direct call
	package_knld $LINUX_BIN ${2} ${3} ${4} ${5} ${6} ${7} ${8}

fi

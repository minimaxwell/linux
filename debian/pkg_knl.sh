#!/bin/bash

#===============================================================
# Note: function to generate knl debian installer. The schedulling of the 
#       dtb file is very important. It determine the schedulling for the storage
#	in the FLASH BOOT
#	 1 => Path and name for the KNL binary file
#	 2 => Path and name for the 1st dtb file
# opt	 3 => Path and name for the 2nd dtb file
#	 etc....
#===============================================================
package_knl()
{
	#===== Check parameters
	if [ "$#" -lt "2" ] ; then
		echo "pkg_knl: Error bad number of parameters"
		return 2
	fi

	#===== Construct directory path
	ROOT_PATH=`pwd`
	PATH_PKG=$ROOT_PATH/knl
	PATH_LINUX=$1

	if [ ! -f $PATH_LINUX ] ; then
		echo "pkg_knl: Error file not found [$PATH_LINUX]"
		return 2
	fi
	if [ ! -d $PATH_PKG ] ; then
		mkdir -p $PATH_PKG
	else
		rm -Rf $PATH_PKG/*
	fi

	# copy LINUX
	if [ ! -d $PATH_PKG/tmp ]; then
		mkdir -p $PATH_PKG/tmp
	fi
	cp $PATH_LINUX $PATH_PKG/tmp

	# verifying DTB files exist
	for (( i = 2; i <= $#; i += 1)); do
		echo "dtb: ${!i}"
		if [ ! -f ${!i} ]; then
			echo "pkg_knl: Error ${!i} do not exist"
			return 2
		fi
	done

	# Board type
	board=`basename ${PATH_LINUX} | cut -d"-" -f2`

	# Creating FLASH image for dtb
	case ${board} in
	"MCR3000_1G")
		dd if=/dev/zero of=$PATH_PKG/tmp/dtb.bin bs=1 count=64K;;

	"MCR3000_2G")
		dd if=/dev/zero of=$PATH_PKG/tmp/dtb.bin bs=1 count=192K;;
	*)
		echo "pkg_knl: Error board type unknown"
		return 2;;
	esac

	# concatenating DTB file
	offset=0
	for (( i = 2; i <= $#; i += 1)); do
		dd if=${!i} of=$PATH_PKG/tmp/dtb.bin conv=notrunc bs=1 seek=${offset}K
		offset=$(( offset + 16 ))
	done

	#===== Making package
	mkdir -p $PATH_PKG/DEBIAN
	cp ./pkg_knl.control 	$PATH_PKG/DEBIAN/control
	cp ./pkg_knl.postinst 	$PATH_PKG/DEBIAN/postinst
	cp ./pkg_knl.preinst 	$PATH_PKG/DEBIAN/preinst
	echo /tmp/${linux_file} > $PATH_PKG/conffiles
	echo /tmp/dtb.bin >> $PATH_PKG/conffiles

	# replace with the good file name
	linux_file=`basename ${PATH_LINUX}`
	sed -i "s#LINUX_FILE_NAME#/tmp/${linux_file}#g" $PATH_PKG/DEBIAN/postinst
	sed -i "s#LINUX_FILE_NAME#${linux_file}#g" $PATH_PKG/DEBIAN/preinst
	sed -i "s#LINUX_FILE_NAME#${linux_file}#g" $PATH_PKG/DEBIAN/control

	# get KNL version
	version=`basename ${PATH_LINUX} | sed "s/.lzma//" | cut -d"-" -f3`
	sed -i "s/^Version:/Version: ${version}/" $PATH_PKG/DEBIAN/control

	# replace with the good model
	case ${board} in
	"MCR3000_1G")
		sed -i "s/KNL_MODEL_SUPPORT/MCR3000/g" $PATH_PKG/DEBIAN/preinst;;

	"MCR3000_2G")
		sed -i "s/KNL_MODEL_SUPPORT/MCR3000_2G CMPC885/g" $PATH_PKG/DEBIAN/preinst;;

	*)
		echo "pkg_knl: Error model of board unknown"
		return 2;;
	esac

	chmod 755 $PATH_PKG/DEBIAN/post*
	chmod 755 $PATH_PKG/DEBIAN/pre*
	dpkg-deb --build knl

	# rename the package
	mv "knl.deb" "KNL-${board}-${version}.deb"

	return 0
}

usage_pkg_knl()
{
	echo "Usage: $0 KNL_BINARY DTB1 [DTB2 DTB3....]"
	echo "   - Warning: the name of the KNL_BINARY file must be"
	echo "   in the format KNL-BOARD-VERSION-FORMAT. Example:"
	echo "   KNL-MCR3000_2G-3.6.2-uImage"
	echo "   - In the FLASH the DTB files are flashed in the same"
	echo "   order as the parameters DTB1 DTB2 DTB3....."
}


if [ `basename $0` = "pkg_knl.sh" ]; then

	#===== By parameters
	if [ -z "${1}" ]; then
		usage_pkg_knl
		exit 0
	fi
	LINUX_BIN=${1}

	#===== Direct call
	package_knl $LINUX_BIN ${2} ${3} ${4} ${5} ${6} ${7} ${8}

fi

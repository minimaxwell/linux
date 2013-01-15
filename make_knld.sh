#!/bin/bash

. patch_knl-2.4.incl
. firmware.incl


#===== function make_knld
# parameters: 1 => Version number for KNL (example: 3.6.3)
#             2 => Version number for DRV (example: 3.4.3)
#	      3 => Path to ofl source (absolute)
#	      4 => Path to drv source (absolute)
#	      5 => Delivery path (absolute)
make_knld()
{
	#==== working path
	local knl_path=`pwd`

	local knl_version=$1
	local drv_version=$2

	if [ -z "${3}" ]; then
		echo "make_knld: Error! path to OFL source not defined"
		return 2
	fi
	if [ `expr match "${3}" '/'` -ne 1 ]; then
		echo "make_knld: Error! path to OFL source not absolute"
		return 2
	fi
	local ofl_path=$3
	if [ ! -d $ofl_path ]; then
		echo "make_knld: Error! path to OFL source not found"
		return 2
	fi

	if [ -z "${4}" ]; then
		echo "make_knld: Error! path to DRV source not defined"
		return 2
	fi
	if [ `expr match "${4}" '/'` -ne 1 ]; then
		echo "make_knld: Error! path to DRV source not absolute"
		return 2
	fi
	local drv_path=$4
	if [ ! -d $drv_path ]; then
		echo "make_knld: Error! path to DRV source not found"
		return 2
	fi

	if [ -z "${5}" ]; then
		echo "make_knld: Error! delivery path not defined"
		return 2
	fi
	if [ `expr match "${5}" '/'` -ne 1 ]; then
		echo "make_knld: Error! delivery path not absolute"
		return 2
	fi
	local liv_path=$5
	if [ ! -d $liv_path ]; then
		mkdir -p $liv_path
	fi
	if [ ! -d $liv_path/KNLD-${knl_version}/BINAIRES/ ]; then
		mkdir -p $liv_path/KNLD-${knl_version}/BINAIRES/
	fi
	
	mkdir -p $PATCH_DIRECTORY

	#===== Needed to build the ofl.
	rm -f linux/arch/ppc
	ln -sf powerpc linux/arch/ppc

	#===== Needed to build the knl.
	rm -f linux/drivers/platform/saf3000
	ln -sf $drv_path/drv linux/drivers/platform/saf3000 
	ln -sf $drv_path/drv/include/ldb linux/include

	#===== generating KNL headers for OFL
	pushd $knl_path/linux

	# making knl config
	make cmpc885_defconfig

	rm -rf $ofl_path/knl
	make INSTALL_HDR_PATH=$ofl_path/knl headers_install 

	#===== generating OFL
	pushd $ofl_path
	version_ofl=$knl_version
	. ./gen_main.sh
	if [ "$?" != "0" ]; then exit $?; fi
	rm -rf $knl_path/initramfs
	cp -a livraison/ $knl_path/initramfs
	cp -a initramfs.txt $knl_path/
	popd

	#===== creating a clean lib path
	rm -rf $knl_path/debian/lib/
	mkdir -p $knl_path/debian/lib/

	#===== generating

	# cleaning
	make distclean

	# making knl config
	make cmpc885_defconfig

	#===== generating KNL

	#===== Adding the initramfs source to knl config.
	ln -sf ../initramfs
	sed -i -e "s/^CONFIG_INITRAMFS_SOURCE=.*/CONFIG_INITRAMFS_SOURCE=\"..\/initramfs.txt\"/" $knl_path/linux/.config
	echo "CONFIG_INITRAMFS_ROOT_UID=0" >> $knl_path/linux/.config
	echo "CONFIG_INITRAMFS_ROOT_GID=0" >> $knl_path/linux/.config

	#===== update LOCAL VERSION
	sed -i -e "s/CONFIG_LOCALVERSION=.*/CONFIG_LOCALVERSION=\"-s3k-${knl_version}\"/" $knl_path/linux/.config
	echo "#define DRV_VERSION \"${drv_version}\"" > $knl_path/linux/include/saf3000/drv_version.h

	# making knl 
	rm -rf $knl_path/ofl/*

	make -j 4 uImage
	if [ $? -ne 0 ] ; then
		echo "make_knld: Error! echec make uImage"
		return 2
	fi
		
	make -j 4 mcr3000.dtb cmpc885.dtb mcr3000_2g.dtb miae.dtb
	if [ $? -ne 0 ] ; then
		echo "make_knld: Error! echec make *.dtb"
		return 2
	fi

	./BuildLzma.sh
	mv uImage.lzma ./arch/powerpc/boot
	local bin_list="mcr3000.dtb cmpc885.dtb mcr3000_2g.dtb miae.dtb"
	# cp to delivery
	cp ./arch/powerpc/boot/uImage ${liv_path}/KNLD-${knl_version}/BINAIRES/KNLD-${knl_version}-uImage
	cp ./arch/powerpc/boot/uImage.lzma ${liv_path}/KNLD-${knl_version}/BINAIRES/KNLD-${knl_version}-uImage.lzma
	for file in $bin_list; do
		cp ./arch/powerpc/boot/${file} $liv_path/KNLD-${knl_version}/BINAIRES/
	done

	#===== generating DRV tools
	
	pushd $drv_path/cor
	make
	if [ $? -ne 0 ] ; then
		echo "make_knld: Error! echec make DRV tools"
		return 2
	fi
	popd

	mkdir -p $knl_path/debian/usr/local/bin
	install -m 755 -o root -g root $drv_path/cor/CORLoader $knl_path/debian/usr/local/bin
	ppc-linux-strip $knl_path/debian/usr/local/bin/CORLoader

	cp -af $drv_path/etc $knl_path/debian
	cp -af $drv_path/cfg $knl_path/debian/usr/local
	chown -R 1002:1000 $knl_path/debian/usr/local/cfg/
	find $knl_path/debian/etc -type d -name ".svn" -exec rm -rf {} \;
	find $knl_path/debian/usr -type d -name ".svn" -exec rm -rf {} \;
		
	local firmware_path=`pwd`/debian/lib/firmware

	rm -rf ${firmware_path}
	mkdir -p ${firmware_path}
	cp_firmware MCR3000_1G ${drv_version} ${firmware_path} FIRM_DSP
	if [ $? -ne 0 ]; then return 2; fi
	cp_firmware MCR3000_2G ${drv_version} ${firmware_path} FIRM_DSP
	if [ $? -ne 0 ]; then return 2; fi
	cp_firmware MCR3000_1G ${drv_version} ${firmware_path} FIRM_FPGA
	if [ $? -ne 0 ]; then return 2; fi
	cp_firmware MCR3000_2G ${drv_version} ${firmware_path} FIRM_FPGA
	if [ $? -ne 0 ]; then return 2; fi
	cp_firmware MCR3000_1G ${drv_version} ${firmware_path} FIRM_FPGA_C4E1
	if [ $? -ne 0 ]; then return 2; fi
	cp_firmware MCR3000_2G ${drv_version} ${firmware_path} FIRM_FPGA_C4E1
	if [ $? -ne 0 ]; then return 2; fi
	cp_firmware MCR3000_1G ${drv_version} ${firmware_path} FIRM_FPGA_C4E1T
	if [ $? -ne 0 ]; then return 2; fi
	cp_firmware MCR3000_2G ${drv_version} ${firmware_path} FIRM_FPGA_C4E1T
	if [ $? -ne 0 ]; then return 2; fi
	pushd ${firmware_path}
	ln -s DSP-`get_firmware_version MCR3000_1G ${drv_version} FIRM_DSP`.bin DSP_1G.bin
	ln -s DSP-`get_firmware_version MCR3000_2G ${drv_version} FIRM_DSP`.bin DSP_2G.bin
	ln -s FPGA-`get_firmware_version MCR3000_1G ${drv_version} FIRM_FPGA`.bin FPGA_1G.bin
	ln -s FPGA-`get_firmware_version MCR3000_2G ${drv_version} FIRM_FPGA`.bin FPGA_2G.bin
	ln -s FPGAC4E1-`get_firmware_version MCR3000_1G ${drv_version} FIRM_FPGA_C4E1`.bin FPGAC4E1_1G.bin
	ln -s FPGAC4E1-`get_firmware_version MCR3000_2G ${drv_version} FIRM_FPGA_C4E1`.bin FPGAC4E1_2G.bin
	ln -s FPGAC4E1-`get_firmware_version MCR3000_1G ${drv_version} FIRM_FPGA_C4E1T`.bin FPGAC4E1T_1G.bin
	ln -s FPGAC4E1-`get_firmware_version MCR3000_2G ${drv_version} FIRM_FPGA_C4E1T`.bin FPGAC4E1T_2G.bin
	popd

	#===== generating the headers package (needed to generate LDB).
	rm -f ${liv_path}/KNLD-${knl_version}/HEADERS/KNLD-${knl_version}.tar.gz
	find arch/powerpc/include/ -type f | sed -e "/\/\.svn\//d" | xargs tar -cf ${liv_path}/KNLD-${knl_version}/HEADERS/KNLD-${knl_version}.tar 
	find arch/powerpc/lib/ -type f | sed -e "/\/\.svn\//d" | xargs tar -rf ${liv_path}/KNLD-${knl_version}/HEADERS/KNLD-${knl_version}.tar 
	find arch/powerpc/sysdev/ -type f | sed -e "/\/\.svn\//d" | xargs tar -rf ${liv_path}/KNLD-${knl_version}/HEADERS/KNLD-${knl_version}.tar 
	tar -rf ${liv_path}/KNLD-${knl_version}/HEADERS/KNLD-${knl_version}.tar arch/powerpc/Makefile .config  
	find include/ -type f | sed -e "/\/\.svn\//d" | xargs tar -rf ${liv_path}/KNLD-${knl_version}/HEADERS/KNLD-${knl_version}.tar 
	find include/ldb/ -name "*.h" | sed -e "/\/\.svn\//d" | xargs tar -rhf ${liv_path}/KNLD-${knl_version}/HEADERS/KNLD-${knl_version}.tar 
	tar -rf ${liv_path}/KNLD-${knl_version}/HEADERS/KNLD-${knl_version}.tar Makefile Module.symvers 
	find scripts/ -type f | sed -e "/\/debian\//d" | sed -e "/\/\.svn\//d" | xargs tar -rf ${liv_path}/KNLD-${knl_version}/HEADERS/KNLD-${knl_version}.tar 
	gzip ${liv_path}/KNLD-${knl_version}/HEADERS/KNLD-${knl_version}.tar

	#===== generating for each board
	for my_board in MCR3000_1G MCR3000_2G; do
		# in case of MCR3000 1G we must generate the patch
		if [ "${my_board}" == "MCR3000_1G" ]; then
			rm -rf ../debian/lib/modules/

			sed -i -e "s/.*CONFIG_MCR3000_DRV.*/CONFIG_MCR3000_DRV=m/" .config
			sed -i -e "s/.*CONFIG_CMPC885_DRV.*/# CONFIG_CMPC885_DRV is not set/" .config

			make modules
			if [ $? -ne 0 ] ; then
				echo "make_delivery_knld: Error! echec make modules"
				return 2
			fi
	
			make modules_install INSTALL_MOD_PATH=../debian
			find ../debian/lib/modules -name "*.ko" -exec ppc-linux-strip -S {} \;


			# Don't rebuild the btl (FEV 276).
			# make_btl ${my_board} ${path_btl} $PATCH_DIRECTORY || return 1
			rm -rf $PATCH_DIRECTORY
			mkdir $PATCH_DIRECTORY
			cp $btl_image $PATCH_DIRECTORY/u-boot.bin
			cp ${liv_path}/KNLD-${knl_version}/BINAIRES/KNLD-${knl_version}-uImage.lzma $PATCH_DIRECTORY/uImage.lzma
			cp ${liv_path}/KNLD-${knl_version}/BINAIRES/mcr3000.dtb $PATCH_DIRECTORY
			make_patch_knl ${knl_version} ${btl_version} ${liv_path}/KNLD-${knl_version}/PATCH || return 1
		fi

		if [ "${my_board}" == "MCR3000_2G" ]; then
			rm -rf ../debian/lib/modules/

			sed -i -e "s/.*CONFIG_MCR3000_DRV.*/# CONFIG_MCR3000_DRV is not set/" .config
			sed -i -e "s/.*CONFIG_CMPC885_DRV.*/CONFIG_CMPC885_DRV=m/" .config

			make modules
			if [ $? -ne 0 ] ; then
				echo "make_delivery_knld: Error! echec make modules"
				return 2
			fi
	
			make modules_install INSTALL_MOD_PATH=../debian
			find ../debian/lib/modules -name "*.ko" -exec ppc-linux-strip -S {} \;

		fi

		# debian package 
		pushd $knl_path/debian
		case ${my_board} in
		MCR3000_1G)	cp ${liv_path}/KNLD-${knl_version}/BINAIRES/KNLD-${knl_version}-uImage.lzma ./
				cp ${liv_path}/KNLD-${knl_version}/BINAIRES/mcr3000.dtb ./
				./pkg_knld.sh KNLD-${knl_version}-uImage.lzma MCR3000_1G mcr3000.dtb || return 1
				rm -f KNLD-${knl_version}-uImage.lzma
				rm -f mcr3000.dtb
				mv KNLD-${my_board}-${knl_version}.deb ${liv_path}/KNLD-${knl_version}/DEBIAN/
				;;
		MCR3000_2G)	cp ${liv_path}/KNLD-${knl_version}/BINAIRES/KNLD-${knl_version}-uImage ./
				cp ${liv_path}/KNLD-${knl_version}/BINAIRES/cmpc885.dtb ./
				cp ${liv_path}/KNLD-${knl_version}/BINAIRES/mcr3000_2g.dtb ./
				cp ${liv_path}/KNLD-${knl_version}/BINAIRES/miae.dtb ./
				./pkg_knld.sh KNLD-${knl_version}-uImage MCR3000_2G cmpc885.dtb mcr3000_2g.dtb miae.dtb || return 1
				rm -f KNLD-${knl_version}-uImage
				rm -f cmpc885.dtb
				rm -f mcr3000_2g.dtb
				rm -f miae.dtb
				mv KNLD-${my_board}-${knl_version}.deb ${liv_path}/KNLD-${knl_version}/DEBIAN/
				;;
		*)		echo "make_delivery_ldb: Error! board not supported"
				return 2
				;;
		esac
		popd
	done


	# build KNL manpages rpm package
 	if [ -d $knl_path/man ] && 
           [ -f $knl_path/man/KNL_docs.spec ]; then        
		rm -rf ~/rpmbuild
		mkdir -p ~/rpmbuild/BUILD
		mkdir -p ~/rpmbuild/RPMS
		mkdir -p ~/rpmbuild/RPMS/i386
		mkdir -p ~/rpmbuild/SOURCES
		mkdir -p ~/rpmbuild/SPECS
		mkdir -p ~/rpmbuild/SRPMS
		if [ -f ~/.rpmmacros ]; then 
			mv ~/.rpmmacros ./man/
		fi
		echo "%_topdir      %(echo $HOME)/rpmbuild" > ~/.rpmmacros
		pushd $knl_path/man
		cp KNL_docs.spec ~/rpmbuild/SPECS/
		man_version=$(grep "Version:" KNL_docs.spec | while read NOT_USED VERSION; do echo $VERSION; done)
		tar -cvzf KNL_docs-${man_version}.tar.gz KNL_docs/ --exclude "*\.svn*"
		mv KNL_docs-${man_version}.tar.gz ~/rpmbuild/SOURCES/
		pushd ~/rpmbuild/SPECS/
		rpmbuild -ba KNL_docs.spec
		popd
		popd
		cp ~/rpmbuild/RPMS/noarch/KNL_docs-*.noarch.rpm ${liv_path}/KNLD-${knl_version}/MANPAGES/
		rm -f ~/.rpmmacros
		if [ -f $knl_path/man/.rpmmacros ]; then 
			mv $knl_path/man/.rpmmacros ~/
		fi
	fi


	# md5sum
	pushd ${liv_path}/KNLD-${knl_version}/BINAIRES
	find -type f | xargs md5sum > md5sum.chk
	popd

	popd
		
	return 0
}


usage_make_knld()
{
	echo "Usage: $0 KERNEL_VERSION DRV_VERSION SRC_OFL_DIR SRC_DRV_DIR DELIVERY_DIR"
}

if [ `basename $0` = "make_knld.sh" ]; then
# parameters: 1 => Version number for KNL (example: 3.6.3)
#             2 => Version number for DRV (example: 3.4.3)
#	      3 => Path to ofl source (absolute)
#	      4 => Path to drv source (absolute)
#	      5 => Delivery path (absolute)

	#===== By parameters
	if [ -z "${1}" ]; then
		usage_make_knld
		exit 0
	fi

	if [ -z "${2}" ]; then
		usage_make_knld
		exit 0
	fi

	if [ -z "${3}" ]; then
		usage_make_knld
		exit 0
	fi

	if [ -z "${4}" ]; then
		usage_make_knld
		exit 0
	fi

	if [ -z "${5}" ]; then
		usage_make_knld
		exit 0
	fi

	#===== Direct call
	make_knld $1 $2 $3 $4 $5
fi


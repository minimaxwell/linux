#!/bin/sh

# si LZMA n'est pas présent
if [ ! -f ./tools/p7zip_9.13/CPP/7zip/Compress/LZMA_Alone/lzma ] ; then
	# generation de l'outil
	pushd ./tools/p7zip_9.13/CPP/7zip/Compress/LZMA_Alone/
	make -f makefile
	popd
fi

# supprime vmlinux.bin si présent
if [ -f ./vmlinux.bin ] ; then
	rm -f ./vmlinux.bin
fi

# supprime vmlinux.bin.lzma si présent
if [ -f ./vmlinux.bin.lzma ] ; then
	rm -f ./vmlinux.bin.lzma
fi

# verifie que le fichier uImage est présent
if [ ! -f ./arch/powerpc/boot/uImage ] ; then
	echo "Fichier uImage introuvable!"
	exit 0
fi

# récupére le nom du binaire
HEADER_LIST=`mkimage -l ./arch/powerpc/boot/uImage`
WITHOUT_BEGIN=`echo ${HEADER_LIST#*Name:}`
BINARY_NAME=`echo ${WITHOUT_BEGIN%Created:*}`

# si vmlinux.bin.gz est présent
if [ -f ./vmlinux.bin.gz ] ; then
	# decompresse le binaire
	gzip -c -d ./vmlinux.bin.gz > ./vmlinux.bin
	# compresse en LZMA
	./tools/p7zip_9.13/CPP/7zip/Compress/LZMA_Alone/lzma e ./vmlinux.bin ./vmlinux.bin.lzma -d12
	# creation de l'image U-BOOT
	mkimage -C lzma -A PowerPC -O Linux -T Kernel -a 0 -e 0 -n $BINARY_NAME -d ./vmlinux.bin.lzma ./uImage.lzma
else
	echo "Fichier vmlinux.bin.gz introuvable!"
	exit 0
fi


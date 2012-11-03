#!/bin/sh

# si LZMA n'est pas présent
WHEREIS_LZMA=`whereis -b lzma`
LZMA=`echo ${WHEREIS_LZMA#*lzma:}`
if [ "${#LZMA}" = 0 ] ; then
	echo "l'outil [lzma] est introuvable. Impossible de générer l'image"
	exit 2
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
	exit 2
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
	lzma -z ./vmlinux.bin -c > ./vmlinux.bin.lzma
	if [ $? != 0 ] ; then
		echo "Version de LZMA introuvable ou incorrect. Utilisez <yum install lzma> pour son installation!"
		exit 2
	fi
	# creation de l'image U-BOOT
	mkimage -C lzma -A PowerPC -O Linux -T Kernel -a 0 -e 0 -n $BINARY_NAME -d ./vmlinux.bin.lzma ./uImage.lzma
else
	echo "Fichier vmlinux.bin.gz introuvable!"
	exit 2
fi

# tout est bien qui finie bien
exit 0


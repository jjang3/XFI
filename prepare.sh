#!/bin/sh
git submodule init && git submodule update

GP_PATH=$( cd ../../"$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
P_PATH=$( cd ../"$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
CUR_PATH=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

# Modifying the musl-related files to preapre to build the musl
echo ${CUR_PATH}

NEW_CRT1_PATCH=${CUR_PATH}"/input/scripts/crt1.c"
CRT1_PATH=${CUR_PATH}"/musl/crt/crt1.c"
CRT1_BACKUP_PATH=${CRT1_PATH}.bak

# Backup the original crt1.c
if [ ! -f ${CRT1_BACKUP_PATH} ]; then
    cp ${CRT1_PATH} ${CRT1_BACKUP_PATH}
    echo "Backup of crt1.c created at ${CRT1_BACKUP_PATH}"
else
    echo "Backup of crt1.c already exists at ${CRT1_BACKUP_PATH}"
fi

# Copy the new crt1.c to the original path
cp ${NEW_CRT1_PATCH} ${CRT1_PATH}
echo "XFI's crt1.c copied to ${CRT1_PATH}"

cd $CUR_PATH"/musl"

./configure --prefix=$CUR_PATH/musl_build && make -j4 && make install

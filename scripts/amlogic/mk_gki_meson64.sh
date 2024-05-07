# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#
# Copyright (c) 2019 Amlogic, Inc. All rights reserved.
#

export CROSS_COMPILE=/opt/gcc-arm-8.3-2019.03-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-
#ARCH=arm64 ./scripts/kconfig/merge_config.sh arch/arm64/configs/gki_defconfig arch/arm64/configs/meson64_gki.fragment
ARCH=arm64 ./scripts/kconfig/merge_config.sh arch/arm64/configs/meson64_gki_defconfig arch/arm64/configs/meson64_gki.fragment
make ARCH=arm64 Image -j12
make ARCH=arm64 dtbs -j12
make ARCH=arm64 modules -j12


GKI_EXT_MODULE_CFG=arch/arm64/configs/meson64_gki_module_config

function read_ext_module_config() {
	ALL_LINE=""
	while read LINE
	do
		if [[ $LINE != \#*  &&  $LINE != "" ]]; then
			ALL_LINE="$ALL_LINE"" ""$LINE"
		fi
	done < $1
	echo $ALL_LINE
}

GKI_EXT_MODULE_CONFIG=$(read_ext_module_config $GKI_EXT_MODULE_CFG)

export GKI_EXT_MODULE_CONFIG

function read_ext_module_predefine() {
	PRE_DEFINE=""

	for y_config in `cat $1 | grep "^CONFIG_.*=y" | sed 's/=y//'`;
	do
		PRE_DEFINE="$PRE_DEFINE"" -D"${y_config}
	done

	for m_config in `cat $1 | grep "^CONFIG_.*=m" | sed 's/=m//'`;
	do
		PRE_DEFINE="$PRE_DEFINE"" -D"${m_config}_MODULE
	done

	echo $PRE_DEFINE
}

GKI_EXT_MODULE_PREDEFINE=$(read_ext_module_predefine $GKI_EXT_MODULE_CFG)
echo "GKI_EXT_MODULE_PREDEFINE="$GKI_EXT_MODULE_PREDEFINE

export GKI_EXT_MODULE_PREDEFINE

# ----------- basic common modules build as extern --------------
EXT_MODULES="drivers/amlogic
sound/soc/amlogic
sound/soc/codecs/amlogic
"

if [[ -z "${SKIP_EXT_MODULES}" ]] && [[ -n "${EXT_MODULES}" ]]; then
  echo "========================================================"
  echo " Building external modules"

  for EXT_MOD in ${EXT_MODULES}; do
    set -x
    make -C . M=${EXT_MOD} ARCH=arm64 ${GKI_EXT_MODULE_CONFIG} -j
#make -C . M=${EXT_MOD} ARCH=arm64 ${GKI_EXT_MODULE_CONFIG} modules_install -j
    set +x
  done

fi

echo "pwd = "$PWD
rm -rfv ${INSTALL_MOD_PATH}
mkdir -p ${INSTALL_MOD_PATH}/target/
mkdir -p ${INSTALL_MOD_PATH}/lib/modules/0.0/lib/modules/
for loop in `find -type f | grep ko$`;
do
	echo cp $loop ${INSTALL_MOD_PATH}/lib/modules/0.0/lib/modules/
	cp $loop ${INSTALL_MOD_PATH}/lib/modules/0.0/lib/modules/
done

echo depmod -b ${INSTALL_MOD_PATH}/ 0.0
depmod -b ${INSTALL_MOD_PATH}/ 0.0

cp ${INSTALL_MOD_PATH}/lib/modules/0.0/lib/modules/*.ko  ${INSTALL_MOD_PATH}/target/
cp ${INSTALL_MOD_PATH}/lib/modules/0.0/modules.dep ${INSTALL_MOD_PATH}/target/

############### generate modules_install.sh ################
> ${INSTALL_MOD_PATH}/target/modules_install.sh
sed -i 's#[^ ]*/##g'  ${INSTALL_MOD_PATH}/target/modules.dep

function mod_probe()
{
        local ko=$1
        local loop
        for loop in `grep "$ko:" ${INSTALL_MOD_PATH}/target/modules.dep | sed 's/.*://'`;
        do
                mod_probe $loop
                echo insmod $loop >> ${INSTALL_MOD_PATH}/target/modules_install.sh
        done
}


for loop in `cat ${INSTALL_MOD_PATH}/target/modules.dep | sed 's/:.*//'`;
do
#       echo "############################# "$loop
        mod_probe $loop
        echo insmod $loop >> ${INSTALL_MOD_PATH}/target/modules_install.sh
done

cat ${INSTALL_MOD_PATH}/target/modules_install.sh  | awk '
{
        if (!cnt[$2]) {
                print $0;
                cnt[$2]++;
        }
}' > ${INSTALL_MOD_PATH}/target/modules_install.sh.tmp

mv ${INSTALL_MOD_PATH}/target/modules_install.sh.tmp ${INSTALL_MOD_PATH}/target/modules_install.sh

sed -i '1s/^/#!\/bin\/sh\n&/' ${INSTALL_MOD_PATH}/target/modules_install.sh

###### done #####
echo ""
echo "Done: ${INSTALL_MOD_PATH}/target/"


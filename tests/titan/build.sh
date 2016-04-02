#!/bin/bash
_script_dir=$( dirname $( readlink -f "$0" ) )
SRC_DIR="${_script_dir}/."
BIN_DIR="bin"

mkdir -p ${BIN_DIR}
cd ${BIN_DIR}

for SRC in $(find ${SRC_DIR} -type f)
do
    ln -s ${SRC} .
done

makefilegen -g -f -e minimalistThermostat -o . *
make

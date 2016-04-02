#!/bin/bash
_script_dir=$( dirname $( readlink -f "$0" ) )
BIN_DIR="bin"

cd ${BIN_DIR}
ttcn3_start minimalistThermostat minimalistThermostat.cfg

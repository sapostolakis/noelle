#!/usr/bin/env bash

set -e

name="$1"

# delete the executable name so we are left with executable arguments only.
shift
execution_args="$@"

ir_in="${name}.bc"
instrumented_ir="${name}.via.bc"
bin_name=${name}
artifacts=artifacts

# run VIAInstrument

${oracle_opt_cmd} -load LLVMVIAInstrument.so -via -S ${ir_in} -o ${instrumented_ir}

# Compile Instrumented file


echo running "${oracle_clang_cmd} ${oracle_clang_opts} ${instrumented_ir} -o ${bin_name} -L"${via_install_dir}/lib" -lvia"
echo "VIACONF_SETTING=${VIACONF_SETTING}"

${oracle_clang_cmd} ${oracle_clang_opts} ${instrumented_ir} -o ${bin_name} -L"${via_install_dir}/lib" -lvia


# Execute Instrumented binary

echo "executing ${bin_name} with args '${execution_args}'"

./${bin_name} ${execution_args}

[ -e $artifacts ] && rm $artifacts

echo "$bin_name" >> $artifacts

echo "done"
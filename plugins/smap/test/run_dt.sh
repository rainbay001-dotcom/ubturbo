#!/bin/bash
# ***********************************************************************
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# Description: smap run dt script
# ***********************************************************************

set -euo pipefail

remove_static()
{
    local dir=$1
    find ${dir} -type f -name "*.c" | xargs -i sed -i "s/\bstatic\b//g" {}
}

remove_inline()
{
    local dir=$1
    find ${dir} -type f -name "*.c" | xargs -i sed -i "s/\binline\b//g" {}
}

rename_class()
{
    local dir=$1
    find ${dir} -type f -name "*.c" | xargs -i sed -i "s/\bclass\b/class_stub/g" {}
}

rename_min()
{
    local dir=$1
    find ${dir} -type f -name "*.c" | xargs -i sed -i "s/\bmin\b/min_stub/g" {}
}

replace_string() {
    filename="$1"
    search_string="$2"
    replace_string="$3"
    replace_without_exist="${4:-true}"

    count=$(grep -c "$replace_string" "$filename" || true)
    if [ $count -gt 0 ]; then
        if [[ "$replace_without_exist" == "true" ]]; then
            return 0
        fi
    fi
    
    sed -i "s/\b$search_string\b/$replace_string/g" "$filename"
}

add_extern()
{
    local dir=$1
    local header_files=""
    local lineno=""
    local tmp=""

    (grep -rn -E '^extern "C"' --include="*.h" ${dir} || echo "1") > tmp
    if [ "$(cat tmp)" = "1" ]; then
        echo "none" > skip_files
    else
        cat tmp | cut -f1 -d: --output-delimiter="\n" > skip_files
    fi
    if [ "$(cat skip_files)" = "none" ]; then
        header_files=$(find ${dir} -type f -name "*.h")
    else
        header_files=$(find ${dir} -type f -name "*.h" | grep -vFf skip_files)
    fi
    for f in ${header_files}
    do
        sed -i -r '0,/^\w/s/(^\w.*$)/#ifdef __cplusplus\nextern "C" {\n#endif\n\1/' ${f}
        (grep -n -E "^#endif" ${f} || echo "no") > tmp
        if [ "$(cat tmp)" = "no" ]; then
            continue
        fi
        lineno=$(cat tmp | tail -n 1 | cut -f1 -d:)
        sed -i "${lineno}i\#ifdef __cplusplus\n}\n#endif" ${f}
    done
}

add_atomic()
{
    local filedir=$1
    local filename="${1}/user/smap_env.h"
    sed -i '/stdatomic/i\#ifndef __cplusplus' ${filename}
    sed -i '/stdatomic/a\#else\n#include <atomic>\n#define _Atomic(X) std::atomic<X>\n#endif' ${filename}
}

CURRENT_PATH=$(cd "$(dirname "$0")"; pwd)
echo "${CURRENT_PATH:?}"
cd "${CURRENT_PATH:?}"

DT_SRC_DIR=$CURRENT_PATH/dt_src
if [[ ! -d "$DT_SRC_DIR" ]]
then
  mkdir -p $DT_SRC_DIR
else
    rm -rf ${DT_SRC_DIR}/*
fi
cp -r ${CURRENT_PATH}/../src $DT_SRC_DIR

code_dir=$(cd ${DT_SRC_DIR} && pwd)

rm -f ${CURRENT_PATH}/tiering/test_iomem.cpp

remove_static ${code_dir}/src
remove_inline ${code_dir}/src
rename_class ${code_dir}/src
rename_min ${code_dir}/src
add_atomic ${code_dir}/src

replace_string "${code_dir}/src/drivers/access_mmu.c" "calc_paddr_acidx(" "mock_calc_paddr_acidx(" "true"
replace_string "${code_dir}/src/drivers/access_mmu.c" "get_mm_by_pid(" "mock_get_mm_by_pid(" "true"
replace_string "${code_dir}/src/drivers/access_mmu.c" "walk->private" "walk->private_data" "true"
replace_string "${code_dir}/src/drivers/access_mmu.c" "calc_paddr_acidx_iomem" "drivers_calc_paddr_acidx_iomem" "true"

replace_string "${code_dir}/src/drivers/access_ioctl.c" "ram_changed" "drivers_ram_changed"

replace_string "${code_dir}/src/drivers/access_iomem.h" "remote_ram_list" "drivers_remote_ram_list"
replace_string "${code_dir}/src/drivers/access_iomem.h" "remote_ram_changed" "drivers_remote_ram_changed"
replace_string "${code_dir}/src/drivers/access_iomem.h" "free_remote_ram" "drivers_free_remote_ram"
replace_string "${code_dir}/src/drivers/access_iomem.h" "copy_remote_ram" "drivers_copy_remote_ram"
replace_string "${code_dir}/src/drivers/access_iomem.h" "__print_remote_ram" "drivers__print_remote_ram"
replace_string "${code_dir}/src/drivers/access_iomem.h" "__ram_changed" "drivers__ram_changed"
replace_string "${code_dir}/src/drivers/access_iomem.h" "insert_remote_ram" "drivers_insert_remote_ram"
replace_string "${code_dir}/src/drivers/access_iomem.h" "walk_system_ram_remote_range" "drivers_walk_system_ram_remote_range"
replace_string "${code_dir}/src/drivers/access_iomem.h" "release_remote_ram" "drivers_release_remote_ram"
replace_string "${code_dir}/src/drivers/access_iomem.h" "ram_changed" "drivers_ram_changed"
replace_string "${code_dir}/src/drivers/access_iomem.h" "refresh_remote_ram" "drivers_refresh_remote_ram"
replace_string "${code_dir}/src/drivers/access_iomem.h" "calc_paddr_acidx_iomem" "drivers_calc_paddr_acidx_iomem"


replace_string "${code_dir}/src/drivers/access_iomem.c" "remote_ram_list" "drivers_remote_ram_list"
replace_string "${code_dir}/src/drivers/access_iomem.c" "remote_ram_changed" "drivers_remote_ram_changed"
replace_string "${code_dir}/src/drivers/access_iomem.c" "free_remote_ram" "drivers_free_remote_ram"
replace_string "${code_dir}/src/drivers/access_iomem.c" "copy_remote_ram" "drivers_copy_remote_ram"
replace_string "${code_dir}/src/drivers/access_iomem.c" "__print_remote_ram" "drivers__print_remote_ram"
replace_string "${code_dir}/src/drivers/access_iomem.c" "__ram_changed" "drivers__ram_changed"
replace_string "${code_dir}/src/drivers/access_iomem.c" "insert_remote_ram" "drivers_insert_remote_ram"
replace_string "${code_dir}/src/drivers/access_iomem.c" "walk_system_ram_remote_range" "drivers_walk_system_ram_remote_range"
replace_string "${code_dir}/src/drivers/access_iomem.c" "release_remote_ram" "drivers_release_remote_ram"
replace_string "${code_dir}/src/drivers/access_iomem.c" "ram_changed" "drivers_ram_changed"
replace_string "${code_dir}/src/drivers/access_iomem.c" "refresh_remote_ram" "drivers_refresh_remote_ram"
replace_string "${code_dir}/src/drivers/access_iomem.c" "calc_paddr_acidx_iomem" "drivers_calc_paddr_acidx_iomem"
replace_string "${code_dir}/src/drivers/access_iomem.c" "nr_local_numa" "drivers_nr_local_numa"
replace_string "${code_dir}/src/drivers/access_iomem.c" "smap_scene" "drivers_smap_scene"
replace_string "${code_dir}/src/drivers/access_iomem.c" "fixed_remote_ram" "drivers_fixed_remote_ram"
replace_string "${code_dir}/src/drivers/access_iomem.c" "calc_acidx_paddr_iomem" "drivers_calc_acidx_paddr_iomem"
replace_string "${code_dir}/src/drivers/access_iomem.c" "update_resource" "drivers_update_resource"
replace_string "${code_dir}/src/drivers/memory_notifier.c" "smap_scene" "drivers_smap_scene"

replace_string "${code_dir}/src/drivers/access_acpi_mem.c" "mem_info acpi_mem" "mem_info drivers_acpi_mem"
replace_string "${code_dir}/src/drivers/access_acpi_mem.c" "acpi_mem.mem" "drivers_acpi_mem.mem"
replace_string "${code_dir}/src/drivers/access_acpi_mem.c" "acpi_mem.len" "drivers_acpi_mem.len"
replace_string "${code_dir}/src/drivers/access_acpi_mem.c" "nr_local_numa" "drivers_nr_local_numa"
replace_string "${code_dir}/src/drivers/access_acpi_mem.c" "acpi_parse_memory_affinity" "drivers_acpi_parse_memory_affinity"
replace_string "${code_dir}/src/drivers/access_acpi_mem.c" "init_acpi_mem" "drivers_init_acpi_mem"
replace_string "${code_dir}/src/drivers/access_acpi_mem.c" "reset_acpi_mem" "drivers_reset_acpi_mem"
replace_string "${code_dir}/src/drivers/access_acpi_mem.c" "get_node_actc_len" "drivers_get_node_actc_len"
replace_string "${code_dir}/src/drivers/access_acpi_mem.c" "is_paddr_local" "drivers_is_paddr_local"
replace_string "${code_dir}/src/drivers/access_acpi_mem.c" "acpi_table_build_mem" "drivers_acpi_table_build_mem"

replace_string "${code_dir}/src/drivers/access_acpi_mem.h" "nr_local_numa" "drivers_nr_local_numa"
replace_string "${code_dir}/src/drivers/access_acpi_mem.h" "is_paddr_local" "drivers_is_paddr_local"

replace_string "${code_dir}/src/drivers/access_tracking.c" "remote_ram_list" "drivers_remote_ram_list"
replace_string "${code_dir}/src/drivers/accessed_bit.c" "remote_ram_list" "drivers_remote_ram_list"
replace_string "${code_dir}/src/drivers/accessed_bit.c" "walk->private" "walk->private_data" "true"
replace_string "${code_dir}/src/drivers/accessed_bit.c" "get_mm_by_pid(" "mock_get_mm_by_pid(" "true"
replace_string "${code_dir}/src/drivers/accessed_bit.c" "calc_paddr_acidx_iomem" "drivers_calc_paddr_acidx_iomem" "true"
replace_string "${code_dir}/src/drivers/accessed_bit.h" "remote_ram_list" "drivers_remote_ram_list"

replace_string "${code_dir}/src/drivers/access_tracking_wrapper.c" "__pte_offset_map" "drivers__pte_offset_map"
replace_string "${code_dir}/src/drivers/access_tracking_wrapper.c" "__pte_offset_map_lock" "drivers__pte_offset_map_lock"
replace_string "${code_dir}/src/drivers/access_tracking_wrapper.c" "huge_ptep_get" "drivers_huge_ptep_get"
replace_string "${code_dir}/src/drivers/access_tracking_wrapper.c" "pmdp_get_lockless_start" "drivers_pmdp_get_lockless_start"
replace_string "${code_dir}/src/drivers/access_tracking_wrapper.c" "pmdp_get_lockless_end" "drivers_pmdp_get_lockless_end"
replace_string "${code_dir}/src/drivers/access_tracking_wrapper.c" "smap__pte_offset_map" "drivers_smap__pte_offset_map"
replace_string "${code_dir}/src/drivers/access_tracking_wrapper.c" "smap__pte_offset_map_lock" "drivers_smap__pte_offset_map_lock"
replace_string "${code_dir}/src/drivers/access_tracking_wrapper.h" "smap__pte_offset_map_lock" "drivers_smap__pte_offset_map_lock"
replace_string "${code_dir}/src/drivers/access_tracking_wrapper.c" "get_pageblock_bitmap" "drivers_get_pageblock_bitmap"
replace_string "${code_dir}/src/drivers/access_tracking_wrapper.c" "pfn_to_bitidx" "drivers_pfn_to_bitidx"
replace_string "${code_dir}/src/drivers/access_tracking_wrapper.c" "get_pfnblock_flags_mask" "drivers_get_pfnblock_flags_mask"

replace_string "${code_dir}/src/drivers/access_tracking.c" "init_actc_data" "drivers_init_actc_data"
replace_string "${code_dir}/src/drivers/access_tracking.c" "is_access_hugepage" "drivers_is_access_hugepage"
replace_string "${code_dir}/src/drivers/access_tracking.h" "is_access_hugepage" "drivers_is_access_hugepage"
replace_string "${code_dir}/src/drivers/access_mmu.c" "is_access_hugepage" "drivers_is_access_hugepage"
replace_string "${code_dir}/src/drivers/access_pid.c" "is_access_hugepage" "drivers_is_access_hugepage"
replace_string "${code_dir}/src/drivers/access_mmu.c" "is_paddr_local" "drivers_is_paddr_local"
replace_string "${code_dir}/src/drivers/access_tracking.c" "work_func" "drivers_work_func"
replace_string "${code_dir}/src/drivers/access_tracking_wrapper.c" "lookup_kallsyms_lookup_name" "drivers_lookup_kallsyms_lookup_name"
replace_string "${code_dir}/src/drivers/access_tracking.c" "lookup_kallsyms_lookup_name" "drivers_lookup_kallsyms_lookup_name"
replace_string "${code_dir}/src/drivers/access_tracking.c" "ram_changed" "drivers_ram_changed"
replace_string "${code_dir}/src/drivers/access_tracking.c" "refresh_remote_ram" "drivers_refresh_remote_ram"
replace_string "${code_dir}/src/drivers/access_tracking.c" "smap_scene" "drivers_smap_scene"
replace_string "${code_dir}/src/drivers/access_tracking.h" "smap_scene" "drivers_smap_scene"

replace_string "${code_dir}/src/drivers/hist_tracking.c" "reset_actc_data" "drivers_reset_actc_data"
replace_string "${code_dir}/src/drivers/hist_tracking.c" "actc_buffer_deinit" "drivers_actc_buffer_deinit"
replace_string "${code_dir}/src/drivers/hist_tracking.c" "actc_buffer_reinit" "drivers_actc_buffer_reinit"
replace_string "${code_dir}/src/drivers/hist_tracking.c" "work_func" "hist_work_func"
replace_string "${code_dir}/src/drivers/hist_tracking.c" "get_page_size" "drivers_get_page_size"
replace_string "${code_dir}/src/drivers/hist_tracking.c" "calc_access_len" "drivers_calc_access_len"
replace_string "${code_dir}/src/drivers/hist_tracking.c" "actc_buffer_init" "drivers_actc_buffer_init"
replace_string "${code_dir}/src/drivers/hist_tracking.c" "update_actc_mode_sum" "drivers_update_actc_mode_sum"
replace_string "${code_dir}/src/drivers/hist_tracking.c" "remote_ram_list" "drivers_remote_ram_list"
replace_string "${code_dir}/src/drivers/hist_tracking.c" "refresh_remote_ram" "drivers_refresh_remote_ram"
replace_string "${code_dir}/src/drivers/hist_tracking.c" "smap_scene" "hist_smap_scene"

# hist_ops.c
replace_string "${code_dir}/src/drivers/hist_ops.c" "remote_ram_list" "drivers_remote_ram_list"
replace_string "${code_dir}/src/drivers/hist_ops.c" "nr_local_numa" "drivers_nr_local_numa"
replace_string "${code_dir}/src/drivers/hist_ops.c" "ram_changed" "drivers_ram_changed"

# migration.c
replace_string "${code_dir}/src/user/strategy/migration.c" "InitMigList" "strategy_InitMigList"

replace_string "${code_dir}/src/tiering/coherence_maintain.c" "walk->private" "walk->private_data" "true"
replace_string "${code_dir}/src/tiering/ham_migration.c" "walk->private" "walk->private_data" "true"

# test/depends/include/linux/pagewalk.h
replace_string "${CURRENT_PATH}/depends/include/linux/pagewalk.h" "private;" "private_data;" "true"

add_extern ${code_dir}/src

set -e

BUILD_DIR=$CURRENT_PATH/build
if [[ ! -d "$BUILD_DIR" ]]
then
    mkdir -p $BUILD_DIR
fi

rm -rf $CURRENT_PATH/build/*

cd $BUILD_DIR || {
    echo "Fatal! Cannot enter $BUILD_DIR."
    exit 1;
}

mock_patch_path=$CURRENT_PATH/3rdparty/mockcpp/mockcpp_support_arm64.patch
if [[ ! -e "$mock_patch_path" ]]
then
    dos2unix $CURRENT_PATH/3rdparty/mockcpp_support_arm64.patch
    cp -r $CURRENT_PATH/3rdparty/mockcpp_support_arm64.patch $CURRENT_PATH/3rdparty/mockcpp
    cd $CURRENT_PATH/3rdparty/mockcpp
    dos2unix src/UnixCodeModifier.cpp
    git apply mockcpp_support_arm64.patch
fi
cd $CURRENT_PATH/build
 
N_CPUS=$(grep processor /proc/cpuinfo | wc -l)
echo "$N_CPUS processors detected."
 
CMAKE_CMD="cmake -DCMAKE_BUILD_TYPE=Debug $CURRENT_PATH"
BUILD_CMD="make -j $((N_CPUS-2))"
 
echo $CMAKE_CMD
$CMAKE_CMD || {
    echo "Failed to configure smap_dt build!"
    exit 1
}
echo
echo "Done configuring smap_dt build"
echo
echo $BUILD_CMD
$BUILD_CMD || {
    echo "Failed to build smap_dt"
    exit 1
}
echo
echo Success

./smap_dt

mkdir -p build/gcovr_report
lcov --d ./ --c --output-file test.info --rc lcov_branch_coverage=1
lcov -e test.info "*/dt_src/src/*" -output-file coverage.info --rc lcov_branch_coverage=1
genhtml -o gcovr_report coverage.info --show-details --legend --rc lcov_branch_coverage=1
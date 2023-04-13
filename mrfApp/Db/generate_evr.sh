#!/bin/bash

db_dir="`dirname "$0"`"

. "${db_dir}/generate_lib.sh"

device_type="$1"
mode="$2"

form_factor=""
series=""

case "${device_type}" in
  mtca-evr-300)
    form_factor="mtca"
    series="300"
    ;;
  vme-evr-230|vme-evr-230rf)
    form_factor="vme"
    series="230"
    ;;
  *)
    echo "Unsupported device type: ${device_type}" >&2
    exit 1
    ;;
esac

extension=""
case "${mode}" in
  records)
    extension=db
    ;;
  autosave_request_file)
    extension=req
    ;;
  *)
    echo "Unsupported mode: ${mode}" >&2
    exit 1
    ;;
esac

dbus_bits_to_pulse_gens() {
  local dbus_bit_to_pulse_gen_num_pulse_gen="$1"
  local dbus_bit_to_pulse_gen_base_addr=$( hex_to_decimal 0x0180 )
  for dbus_bit_num in `seq 0 7`; do
    local dbus_bit_to_pulse_gen_addr=$( decimal_to_hex $(( ${dbus_bit_to_pulse_gen_base_addr} + 4 * ${dbus_bit_num} )) 4 )
    if [ "${dbus_bit_to_pulse_gen_num_pulse_gen}" -gt 0 ]; then
      local i
      for i in `seq 0 $(( ${dbus_bit_to_pulse_gen_num_pulse_gen} - 1 ))`; do
        cat "${db_dir}/evr-template-dbus-bit-to-pulse-gen.inc.${extension}" | substitute_template_variables DBUS_BIT_NUM="${dbus_bit_num}" DBUS_BIT_TO_PULSE_GEN_ADDR="${dbus_bit_to_pulse_gen_addr}" PULSE_GEN_NUM="${i}"
      done
    fi
  done
}

fp_input() {
  local fp_input_num="$1"
  local fp_input_can_read="false"
  if [ $# -ge 2 ]; then
    fp_input_can_read="$2"
  fi
  local fp_input_addr=$( decimal_to_hex $(( $( hex_to_decimal 0x0500 ) + 4 * ${fp_input_num} )) 4 )
  cat "${db_dir}/evr-template-fp-input.inc.${extension}" | substitute_template_variables FP_INPUT_NUM="${fp_input_num}" FP_INPUT_ADDR="${fp_input_addr}"
  if [ "$fp_input_can_read" = "true" ]; then
    cat "${db_dir}/evr-template-fp-input-read.inc.${extension}" | substitute_template_variables FP_INPUT_NUM="${fp_input_num}" FP_INPUT_ADDR="${fp_input_addr}"
  fi
  description "\$(P)\$(R)FPIn${fp_input_num}:Description" "Description for front-panel input ${fp_input_num}"
}

fp_output() {
  local fp_out_num="$1"
  local fp_out_addr=$( decimal_to_hex $(( $( hex_to_decimal 0x0400 ) + 2 * ${fp_out_num} )) 4 )
  output "FPOut${fp_out_num}" "Front panel output ${fp_out_num}" ${fp_out_addr}
}

map_ram() {
  local map_ram_num="$1"
  local map_ram_base_addr=$(( $( hex_to_decimal 0x4000 ) + $( hex_to_decimal 0x1000 ) * ${map_ram_num} ))
  local map_ram_int_funcs_addr=$( decimal_to_hex ${map_ram_base_addr} 4 )
  local map_ram_trig_pulse_gens_addr=$( decimal_to_hex $(( ${map_ram_base_addr} + 4 )) 4 )
  local map_ram_set_pulse_gens_addr=$( decimal_to_hex $(( ${map_ram_base_addr} + 8 )) 4 )
  local map_ram_reset_pulse_gens_addr=$( decimal_to_hex $(( ${map_ram_base_addr} + 12 )) 4 )
  cat "${db_dir}/evr-template-map-ram.inc.${extension}" | substitute_template_variables MAP_RAM_NUM="${map_ram_num}" MAP_RAM_INT_FUNCS_ADDR="${map_ram_int_funcs_addr}" MAP_RAM_TRIG_PULSE_GENS_ADDR="${map_ram_trig_pulse_gens_addr}" MAP_RAM_SET_PULSE_GENS_ADDR="${map_ram_set_pulse_gens_addr}" MAP_RAM_RESET_PULSE_GENS_ADDR="${map_ram_reset_pulse_gens_addr}"
}

output() {
  local output_name="$1"
  local output_description="$2"
  local output_addr="$3"
  local output_installed_macro=""
  local output_installed_description=""
  if [ $# -ge 5 ]; then
    output_installed_macro="$4"
    output_installed_description="$5"
  fi
  cat "${db_dir}/evr-template-output.inc.${extension}" | substitute_template_variables OUTPUT_NAME="${output_name}" OUTPUT_DESCRIPTION="${output_description}" OUTPUT_ADDR="${output_addr}"
  if [ -n "${output_installed_macro}" ]; then
    cat "${db_dir}/evr-template-output-installed.inc.${extension}" | substitute_template_variables OUTPUT_NAME="${output_name}" OUTPUT_INSTALLED_MACRO="${output_installed_macro}" OUTPUT_INSTALLED_DESCRIPTION="${output_installed_description}"
  fi
  description "\$(P)\$(R)${output_name}:Description" "Description for ${output_description}"
}

prescaler() {
  local prescaler_num="$1"
  local prescaler_size=16
  local prescaler_to_pulse_gen_num_pulse_gen=0
  if [ $# -ge 2 ]; then
    prescaler_size="$2"
  fi
  if [ $# -ge 3 ]; then
    prescaler_to_pulse_gen_num_pulse_gen="$3"
  fi
  local prescaler_base_addr=$( hex_to_decimal 0x0100 )
  local prescaler_addr=$( decimal_to_hex $(( ${prescaler_base_addr} + 4 * ${prescaler_num} )) 4 )
  local prescaler_to_pulse_gen_base_addr=$( hex_to_decimal 0x0140 )
  local prescaler_to_pulse_gen_addr=$( decimal_to_hex $(( ${prescaler_to_pulse_gen_base_addr} + 4 * ${prescaler_num} )) 4 )
  local -a write_all_pvs
  cat "${db_dir}/evr-template-prescaler-${prescaler_size}-bit.inc.${extension}" | substitute_template_variables PRESCALER_NUM="${prescaler_num}" PRESCALER_ADDR="${prescaler_addr}"
  write_all_pvs+=("\$(P)\$(R)Prescaler${prescaler_num}")
  if [ "${prescaler_to_pulse_gen_num_pulse_gen}" -gt 0 ]; then
    local i
    for i in `seq 0 $(( ${prescaler_to_pulse_gen_num_pulse_gen} - 1 ))`; do
      cat "${db_dir}/evr-template-prescaler-to-pulse-gen.inc.${extension}" | substitute_template_variables PRESCALER_NUM="${prescaler_num}" PRESCALER_TO_PULSE_GEN_ADDR="${prescaler_to_pulse_gen_addr}" PULSE_GEN_NUM="${i}"
      write_all_pvs+=("\$(P)\$(R)Prescaler${prescaler_num}:MapTo:TrigPulseGen${i}")
    done
  fi
  description "\$(P)\$(R)Prescaler${prescaler_num}:Description" "Description for prescaler ${prescaler_num}"
  if [ "${mode}" = "records" ]; then
    fanout "\$(P)\$(R)Intrnl:WriteAll:Prescaler${prescaler_num}" "${write_all_pvs[@]}"
  fi
}

pulse_gens_header() {
  local number_of_pulse_generators="$1"
  cat "${db_dir}/evr-template-pulse-gens-header.inc.${extension}" | substitute_template_variables NUMBER_OF_PULSE_GENERATORS="${number_of_pulse_generators}"
}

pulse_gen() {
  local pulse_gen_num="$1"
  local pulse_gen_prescaler_size="$2"
  local pulse_gen_delay_size="$3"
  local pulse_gen_width_size="$4"
  local pulse_gen_base_addr=$(( $( hex_to_decimal 0x0200 ) + 16 * ${pulse_gen_num} ))
  local pulse_gen_ctrl_addr=$( decimal_to_hex ${pulse_gen_base_addr} 4 )
  local pulse_gen_prescaler_addr=$( decimal_to_hex $(( ${pulse_gen_base_addr} + 4 )) 4 )
  local pulse_gen_delay_addr=$( decimal_to_hex $(( ${pulse_gen_base_addr} + 8 )) 4 )
  local pulse_gen_width_addr=$( decimal_to_hex $(( ${pulse_gen_base_addr} + 12 )) 4 )
  local -a write_all_pvs
  cat "${db_dir}/evr-template-pulse-gen-generic.inc.${extension}" | substitute_template_variables PULSE_GEN_NUM="${pulse_gen_num}" PULSE_GEN_CTRL_ADDR="${pulse_gen_ctrl_addr}"
  write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:PulseGen${pulse_gen_num}:Generic")
  if [ ${pulse_gen_prescaler_size} -ne 0 ]; then
    cat "${db_dir}/evr-template-pulse-gen-prescaler-${pulse_gen_prescaler_size}-bit.inc.${extension}" | substitute_template_variables PULSE_GEN_NUM="${pulse_gen_num}" PULSE_GEN_PRESCALER_ADDR="${pulse_gen_prescaler_addr}"
    write_all_pvs+=("\$(P)\$(R)PulseGen${pulse_gen_num}:Prescaler")
  fi
  cat "${db_dir}/evr-template-pulse-gen-delay-${pulse_gen_delay_size}-bit.inc.${extension}" | substitute_template_variables PULSE_GEN_NUM="${pulse_gen_num}" PULSE_GEN_DELAY_ADDR="${pulse_gen_delay_addr}"
  write_all_pvs+=("\$(P)\$(R)PulseGen${pulse_gen_num}:Delay")
  cat "${db_dir}/evr-template-pulse-gen-width-${pulse_gen_width_size}-bit.inc.${extension}" | substitute_template_variables PULSE_GEN_NUM="${pulse_gen_num}" PULSE_GEN_WIDTH_ADDR="${pulse_gen_width_addr}"
  write_all_pvs+=("\$(P)\$(R)PulseGen${pulse_gen_num}:Width")
  description "\$(P)\$(R)PulseGen${pulse_gen_num}:Description" "Description for pulse gen. ${pulse_gen_num}"
  if [ "${mode}" = "records" ]; then
    fanout "\$(P)\$(R)Intrnl:WriteAll:PulseGen${pulse_gen_num}" "${write_all_pvs[@]}"
  fi
}

tb_output() {
  local param_gen2=""
  if [ "$1" = "--gen2" ]; then
    param_gen2="$1"
    shift
  fi
  local tb_out_num="$1"
  local tb_out_addr=$( decimal_to_hex $(( $( hex_to_decimal 0x0480 ) + 2 * ${tb_out_num} )) 4 )
  local tb_out_module_first_num=$(( ${tb_out_num} / 2 * 2 ))
  local tb_out_module_second_num=$(( ${tb_out_module_first_num} + 1 ))
  output ${param_gen2} "TBOut${tb_out_num}" "TB univ. output ${tb_out_num}" ${tb_out_addr} "TB_UNIV_OUT_${tb_out_module_first_num}_${tb_out_module_second_num}_INSTALLED=\$(TB_UNIV_OUT_INSTALLED=0)" "TB univ. output module ${tb_out_module_first_num}/${tb_out_module_second_num}"
}

univ_output() {
  local param_gen2=""
  if [ "$1" = "--gen2" ]; then
    param_gen2="$1"
    shift
  fi
  local univ_out_num="$1"
  local univ_out_addr=$( decimal_to_hex $(( $( hex_to_decimal 0x0440 ) + 2 * ${univ_out_num} )) 4 )
  local univ_out_module_first_num=$(( ${univ_out_num} / 2 * 2 ))
  local univ_out_module_second_num=$(( ${univ_out_module_first_num} + 1 ))
  output ${param_gen2} "UnivOut${univ_out_num}" "Universal output ${univ_out_num}" ${univ_out_addr} "UNIV_OUT_${univ_out_module_first_num}_${univ_out_module_second_num}_INSTALLED=\$(UNIV_OUT_INSTALLED=0)" "Universal output module ${univ_out_module_first_num}/${univ_out_module_second_num}"
}

declare -a write_all_pvs
cat "${db_dir}/evr-common.inc.${extension}"
write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:Common")

# Only some EVRs support delay compensation, so we only add the respective
# records for those devices.
case "${device_type}" in
  mtca-evr-300)
    cat "${db_dir}/evr-delay-compensation.inc.${extension}"
    write_all_pvs+=("\$(P)\$(R)DelayComp:Enabled")
    write_all_pvs+=("\$(P)\$(R)DelayComp:TargetDelay")
    ;;
esac

for i in `seq 0 1`; do
  map_ram ${i}
  write_all_pvs+=("\$(P)\$(R)Event:MapRAM${i}:WriteAll")
done

if [ "${form_factor}" = "mtca" -a "${series}" = "300" ]; then
  cat "${db_dir}/evr-interrupts-mmap.${extension}"
  write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:IRQ")
  cat "${db_dir}/evr-mtca-300.inc.${extension}"
  for i in `seq 0 7`; do
    prescaler ${i} 32 16
    write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:Prescaler${i}")
  done
  dbus_bits_to_pulse_gens 16
  pulse_gens_header 16
  for i in `seq 0 3`; do
    pulse_gen ${i} 16 32 32
    write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:PulseGen${i}")
  done
  for i in `seq 4 15`; do
    pulse_gen ${i} 0 32 16
    write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:PulseGen${i}")
  done
  for i in `seq 0 3`; do
    fp_output ${i}
    write_all_pvs+=("\$(P)\$(R)FPOut${i}:Map")
  done
  for i in `seq 0 1`; do
    fp_input ${i} true
    write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:"FPIn${i})
  done
fi

if [ "${form_factor}" = "vme" -a "${series}" = "230" ]; then
  cat "${db_dir}/evr-interrupts-udp-ip.${extension}"
  write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:IRQ")
  cat "${db_dir}/evr-vme-230-common.inc.${extension}"
  for i in `seq 0 2`; do
    prescaler ${i} 16
    write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:Prescaler${i}")
  done
  pulse_gens_header 16
  for i in `seq 0 3`; do
    pulse_gen ${i} 8 32 16
    write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:PulseGen${i}")
  done
  for i in `seq 4 15`; do
    pulse_gen ${i} 0 32 16
    write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:PulseGen${i}")
  done
  for i in `seq 0 6`; do
    fp_output ${i}
    write_all_pvs+=("\$(P)\$(R)FPOut${i}:Map")
  done
  for i in `seq 0 3`; do
    univ_output ${i}
    write_all_pvs+=("\$(P)\$(R)UnivOut${i}:Map")
  done
  for i in `seq 0 15`; do
    tb_output ${i}
    write_all_pvs+=("\$(P)\$(R)TBOut${i}:Map")
  done
  for i in `seq 0 1`; do
    fp_input ${i}
    write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:"FPIn${i})
  done
fi

if [ "${device_type}" = "vme-evr-230" ]; then
  # The standard VME-EVR-230 (non RF version) has one additional front-panel
  # output.
  fp_output 7
  write_all_pvs+=("\$(P)\$(R)FPOut7:Map")
elif [ "${device_type}" = "vme-evr-230rf" ]; then
  cat "${db_dir}/evr-vme-230rf.inc.${extension}"
  write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:FPOut:CML")
fi

# EVRs only have a single SFP module, so we use the empty string for the number.
sfp_module "" "0x8200"

if [ "${mode}" = "records" ]; then
  cat <<EOF
# Write all settings to the hardware.

EOF
  fanout "\$(P)\$(R)Intrnl:WriteAll" "${write_all_pvs[@]}"
  cat <<EOF
record(bo, "\$(P)\$(R)WriteAll") {
  field(DESC, "Send all settings to the hardware")
  field(FLNK, "\$(P)\$(R)Intrnl:WriteAll")
  field(ZNAM, "Write all")
  field(ONAM, "Write all")
}

EOF
fi

#!/bin/bash

db_dir="`dirname "$0"`"

. "${db_dir}/generate_lib.sh"

device_type="$1"
mode="$2"

form_factor=""
series=""

case "${device_type}" in
  vme-evg-230)
    form_factor="vme"
    series="230"
    ;;
  *)
    echo "Unsupported device type: ${device_type}" >&2
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

declare -a write_all_pvs;

cat "${db_dir}/evg-common.inc.${extension}"
write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:Common")

if [ "${form_factor}" = "vme" ]; then
  cat "${db_dir}/evg-vme.inc.${extension}"
  write_all_pvs+=("\$(P)\$(R)Intrnl:WriteAll:VME")
fi

# EVGs only have a single SFP module, so we use the empty string for the number.
sfp_module "" "0x1200"

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

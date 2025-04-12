"""
Generate ``.db`` and ``.req`` files for the MRF device support.
"""

import argparse
import pathlib
import sys

import mrf_db_gen


def _main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--description-record-type",
        type=mrf_db_gen.constants.DescriptionRecordType,
        help=(
            "Record type that shall be used for description records (lsi, "
            "stringout, or waveform)"
        ),
    )
    parser.add_argument(
        "file",
        type=pathlib.Path,
        help="Path of the file that shall be generated",
    )
    args = parser.parse_args()
    description_record_type: mrf_db_gen.constants.DescriptionRecordType = (
        args.description_record_type
    )
    output_file: pathlib.Path = args.file

    try:
        device = mrf_db_gen.device_for_name(output_file.stem)
    except KeyError:
        print(
            f"Error: Unknown device: {output_file.stem}",
            file=sys.stderr,
        )
        sys.exit(1)
    try:
        file_type = mrf_db_gen.constants.FileType(output_file.suffix)
    except KeyError:
        print(
            f"Error: Unknown file extension: {output_file.suffix}",
            file=sys.stderr,
        )
        sys.exit(1)

    with open(output_file, "w", encoding="utf-8") as output_file_obj:
        mrf_db_gen.generate_file(
            output_file_obj, device, file_type, description_record_type
        )


if __name__ == "__main__":
    _main()

import argparse
import math
from pathlib import Path

import numpy as np
from netCDF4 import Dataset


def wrap_deg(x: float) -> float:
    return x % 360.0


def shortest_ang_diff_deg(a: float, b: float) -> float:
    d = (a - b + 180.0) % 360.0 - 180.0
    return d


def choose_horizontal_ring(positions: np.ndarray, target_elevation: float) -> np.ndarray:
    """
    positions: shape [M, 3] = [azimuth_deg, elevation_deg, distance_m]
    Returns a boolean mask for the elevation ring nearest to target_elevation.
    """
    elevations = positions[:, 1]
    unique_els = np.unique(np.round(elevations, decimals=6))
    chosen_el = unique_els[np.argmin(np.abs(unique_els - target_elevation))]
    mask = np.isclose(elevations, chosen_el, atol=1e-5)
    print(f"Using elevation ring: {chosen_el:.6f} degrees")
    return mask


def collapse_duplicate_azimuths(azimuths_deg: np.ndarray, irs: np.ndarray):
    """
    azimuths_deg: [K]
    irs: [K, 2, N]
    If multiple measurements land on the same azimuth after rounding, average them.
    """
    groups = {}
    for i, azi in enumerate(azimuths_deg):
        key = round(float(wrap_deg(azi)), 6)
        groups.setdefault(key, []).append(i)

    unique_azis = sorted(groups.keys())
    out_irs = []

    for azi in unique_azis:
        idxs = groups[azi]
        out_irs.append(np.mean(irs[idxs, :, :], axis=0))

    return np.array(unique_azis, dtype=np.float32), np.stack(out_irs, axis=0)


def circular_interp(target_deg: float, measured_azis_deg: np.ndarray, measured_values: np.ndarray) -> np.ndarray:
    """
    target_deg: scalar
    measured_azis_deg: sorted [K] in [0, 360)
    measured_values: [K, ...]
    Returns interpolated value with wraparound.
    """
    target = wrap_deg(float(target_deg))
    az = measured_azis_deg.astype(np.float64)

    az_ext = np.concatenate([az, [az[0] + 360.0]], axis=0)
    val_ext = np.concatenate([measured_values, measured_values[0:1]], axis=0)

    target_ext = target
    if target_ext < az_ext[0]:
        target_ext += 360.0

    idx = np.searchsorted(az_ext, target_ext, side="right") - 1
    idx = max(0, min(idx, len(az_ext) - 2))

    a0 = az_ext[idx]
    a1 = az_ext[idx + 1]

    if abs(a1 - a0) < 1e-12:
        t = 0.0
    else:
        t = (target_ext - a0) / (a1 - a0)

    return (1.0 - t) * val_ext[idx] + t * val_ext[idx + 1]


def format_1d_int_array(name: str, values, indent="    "):
    lines = [f"const int {name}[{len(values)}] =\n{{"]
    row = []
    for i, v in enumerate(values):
        row.append(str(int(v)))
        if len(row) == 12 or i == len(values) - 1:
            lines.append(indent + ", ".join(row) + ("," if i != len(values) - 1 else ""))
            row = []
    lines.append("};\n")
    return "\n".join(lines)


def format_2d_float_array(name: str, array2d: np.ndarray, indent="    "):
    rows, cols = array2d.shape
    lines = [f"const float {name}[{rows}][{cols}] =\n{{"]
    for r in range(rows):
        vals = ", ".join(f"{float(x):.8ff}".replace("ff", "f") for x in array2d[r])
        lines.append(f"{indent}{{ {vals} }}{',' if r != rows - 1 else ''}")
    lines.append("};\n")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Convert horizontal-plane SOFA HRIRs to C++ table files.")
    parser.add_argument("--input", required=True, help="Path to .sofa file")
    parser.add_argument("--output-dir", default=".", help="Where to write .h/.cpp")
    parser.add_argument("--base-name", default="HorizontalHrirData", help="Base output filename")
    parser.add_argument("--elevation", type=float, default=0.0, help="Target elevation in degrees")
    parser.add_argument("--step", type=int, default=5, help="Azimuth step in degrees")
    parser.add_argument("--length", type=int, default=128, help="Crop/pad HRIR length")
    parser.add_argument("--global-normalise", action="store_true", help="Apply one global scale to all HRIRs")
    args = parser.parse_args()

    sofa_path = Path(args.input)
    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    out_h = out_dir / f"{args.base_name}.h"
    out_cpp = out_dir / f"{args.base_name}.cpp"

    with Dataset(str(sofa_path), "r") as ds:
        if "SourcePosition" not in ds.variables:
            raise RuntimeError("SOFA missing SourcePosition")
        if "Data.IR" not in ds.variables:
            raise RuntimeError("SOFA missing Data.IR")

        src_pos_var = ds.variables["SourcePosition"]
        data_ir_var = ds.variables["Data.IR"]

        positions = np.array(src_pos_var[:], dtype=np.float64)
        irs = np.array(data_ir_var[:], dtype=np.float64)

        if positions.ndim != 2 or positions.shape[1] < 3:
            raise RuntimeError(f"Unexpected SourcePosition shape: {positions.shape}")

        if irs.ndim != 3 or irs.shape[1] < 2:
            raise RuntimeError(f"Unexpected Data.IR shape: {irs.shape}. Expected [M, 2, N].")

        coord_type = getattr(src_pos_var, "Type", "spherical")
        coord_units = getattr(src_pos_var, "Units", "degree, degree, metre")

        if "sph" not in coord_type.lower():
            raise RuntimeError(f"Expected spherical SourcePosition, got Type='{coord_type}'")

        if "degree" not in coord_units.lower():
            raise RuntimeError(f"Expected degree-based SourcePosition, got Units='{coord_units}'")

        sr_var = ds.variables.get("Data.SamplingRate", None)
        if sr_var is None:
            raise RuntimeError("SOFA missing Data.SamplingRate")
        source_sample_rate = float(np.array(sr_var[:]).squeeze())

        # Pick nearest horizontal ring
        ring_mask = choose_horizontal_ring(positions, args.elevation)
        ring_positions = positions[ring_mask]
        ring_irs = irs[ring_mask, :, :]

        ring_azis = np.array([wrap_deg(a) for a in ring_positions[:, 0]], dtype=np.float64)
        ring_azis, ring_irs = collapse_duplicate_azimuths(ring_azis, ring_irs)

        sort_idx = np.argsort(ring_azis)
        ring_azis = ring_azis[sort_idx]
        ring_irs = ring_irs[sort_idx]

        print(f"Measurements on chosen ring: {len(ring_azis)}")
        print(f"Source sample rate: {source_sample_rate}")

        azimuths = np.arange(0, 360, args.step, dtype=np.int32)
        num_azimuths = len(azimuths)
        hrir_length = int(args.length)

        left_table = np.zeros((num_azimuths, hrir_length), dtype=np.float32)
        right_table = np.zeros((num_azimuths, hrir_length), dtype=np.float32)

        for i, azi in enumerate(azimuths):
            interp_ir = circular_interp(float(azi), ring_azis, ring_irs)  # [2, N]
            left_full = interp_ir[0]
            right_full = interp_ir[1]

            # crop/pad
            left_crop = np.zeros(hrir_length, dtype=np.float32)
            right_crop = np.zeros(hrir_length, dtype=np.float32)

            n_left = min(hrir_length, left_full.shape[0])
            n_right = min(hrir_length, right_full.shape[0])

            left_crop[:n_left] = left_full[:n_left].astype(np.float32)
            right_crop[:n_right] = right_full[:n_right].astype(np.float32)

            left_table[i] = left_crop
            right_table[i] = right_crop

        global_scale = 1.0
        if args.global_normalise:
            peak = max(float(np.max(np.abs(left_table))), float(np.max(np.abs(right_table))))
            if peak > 0.0:
                global_scale = 0.99 / peak
                left_table *= global_scale
                right_table *= global_scale

        # Write header
        header_text = f"""#pragma once

namespace HrirData
{{
    inline constexpr int kAzimuthStepDeg = {args.step};
    inline constexpr int kNumAzimuths = {num_azimuths};
    inline constexpr int kHrirLength = {hrir_length};
    inline constexpr float kSourceSampleRate = {source_sample_rate:.1f}f;
    inline constexpr float kGlobalScale = {global_scale:.8f}f;

    extern const int kAzimuthsDeg[kNumAzimuths];
    extern const float kLeft[kNumAzimuths][kHrirLength];
    extern const float kRight[kNumAzimuths][kHrirLength];
}}
"""

        # Write cpp
        cpp_lines = [
            f'#include "{args.base_name}.h"\n',
            "namespace HrirData\n{\n",
            format_1d_int_array("kAzimuthsDeg", azimuths),
            format_2d_float_array("kLeft", left_table),
            format_2d_float_array("kRight", right_table),
            "}\n",
        ]
        cpp_text = "\n".join(cpp_lines)

        out_h.write_text(header_text, encoding="utf-8")
        out_cpp.write_text(cpp_text, encoding="utf-8")

    print(f"Wrote: {out_h}")
    print(f"Wrote: {out_cpp}")
    print("Done.")


if __name__ == "__main__":
    main()
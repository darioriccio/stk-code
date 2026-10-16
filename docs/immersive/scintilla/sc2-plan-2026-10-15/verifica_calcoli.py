#!/usr/bin/env python3
"""Oracle matematico del piano SC2; non importa/esegue il codice del gioco.

Uso: python3 -B verifica_calcoli.py [--output calcoli.json]
Solo libreria standard. Nessuna build, rete, app, hardware o cache.
"""
import argparse
from fractions import Fraction
import itertools
import json
import math
from pathlib import Path


def calculate():
    checks = []

    def check(name, condition):
        if not condition:
            raise AssertionError(name)
        checks.append(name)

    fps = 120
    durations = {
        "acquire": (600000, "ceil", 72),
        "breach": (3500000, "floor", 420),
        "cooperation": (1250000, "floor", 150),
        "primary_interval_equivalent_only": (250000, "ceil", 30),
        "solo_gap": (2500000, "floor", 300),
        "solo_total": (8000000, "floor", 960),
        "stagger": (1000000, "ceil", 120),
        "recovery": (4000000, "ceil", 480),
    }
    tick_table = {}
    for name, (microseconds, rounding, expected) in durations.items():
        product = microseconds * fps
        ticks = (product + (999999 if rounding == "ceil" else 0)) // 1000000
        check("ticks_" + name, ticks == expected)
        tick_table[name] = {"microseconds": microseconds, "ticks_120Hz": ticks,
                            "rounding": rounding}
    for frequency in (30, 60, 120, 144, 165):
        for name, (microseconds, _, _) in durations.items():
            exact = Fraction(microseconds * frequency, 1000000)
            lo = (microseconds * frequency) // 1000000
            hi = (microseconds * frequency + 999999) // 1000000
            check("quantization_%s_%s" % (frequency, name),
                  0 <= exact - lo < 1 and 0 <= hi - exact < 1)

    first_shots = [Fraction(i, 4) for i in range(5)]
    check("five_shots_span_1s", first_shots[-1] - first_shots[0] == 1)
    check("half_open_1s_contains_four", sum(0 <= x < 1 for x in first_shots) == 4)
    check("closed_1s_contains_five", sum(0 <= x <= 1 for x in first_shots) == 5)

    def valid_solo_chain(times):
        return (len(times) == 5 and times[-1] - times[0] < 960
                and all(30 <= b - a < 300 for a, b in zip(times, times[1:])))

    check("M02_solo_earliest", valid_solo_chain([0, 30, 60, 90, 120]))
    check("M04_total_boundary_excluded", not valid_solo_chain([0, 240, 480, 720, 960]))
    check("M05_total_inside", valid_solo_chain([0, 239, 478, 717, 956]))
    check("gap_boundary_excluded", not valid_solo_chain([0, 300, 600, 900, 930]))
    check("gap_and_total_not_redundant", Fraction(249, 100) * 4 > 8)
    check("M13_BC_inside", 249 < min(420, 100 + 150))
    check("M13_BC_boundary", not 250 < min(420, 100 + 150))
    check("M14_breach_inside", 419 < min(420, 288 + 150))
    check("M14_breach_boundary", not 420 < min(420, 288 + 150))
    check("M31_recovery_boundary", 120 + 480 == 600)
    identity_sequences = 0
    for sequence in itertools.product("ABC", repeat=3):
        contributors = set(sequence) - {"A"}
        check("identity_" + "".join(sequence),
              (len(contributors) >= 2) == ("B" in sequence and "C" in sequence))
        identity_sequences += 1

    radius = 0.35
    alpha = math.radians(0.75)
    max_distance = radius / math.sin(alpha)
    check("angle_limit_stricter_than_30m", max_distance < 30)
    check("angular_boundary_inverse", math.isclose(math.asin(radius / max_distance), alpha,
                                                    rel_tol=1e-14))
    check("angle_reject_farther", math.asin(radius / (max_distance + 0.001)) < alpha)
    dome_pixels = 2 * alpha * 1536 / math.pi
    eq_pixels = 2 * alpha * 1024 / math.pi
    check("dome_reference_pixels", math.isclose(dome_pixels, 12.8))
    check("equirect_reference_pixels", math.isclose(eq_pixels, 128 / 15))

    def w(u):
        return 16 * u * u * (1 - u) * (1 - u)

    def dw(u):
        return 32 * u * (1 - u) * (1 - 2 * u)

    def ddw(u):
        return 32 * (1 - 6 * u + 6 * u * u)

    height = Fraction(1, 4)
    samples = [Fraction(i, 4) for i in range(5)]
    heights = [height * w(u) for u in samples]
    angles = [8 * w(u) for u in samples]
    check("stagger_exact_heights", heights == [0, Fraction(9, 64), Fraction(1, 4), Fraction(9, 64), 0])
    check("stagger_exact_angles", angles == [0, Fraction(9, 2), 8, Fraction(9, 2), 0])
    check("stagger_endpoint_velocity", dw(0) == dw(1) == 0)
    check("stagger_not_C2_at_stationary_boundary", ddw(0) == ddw(1) == 32)
    critical = (3 - math.sqrt(3)) / 6
    vmax = 16 * math.sqrt(3) / 9 * float(height)
    amax = 32 * float(height)
    check("stagger_velocity_extremum", math.isclose(float(height) * dw(critical), vmax))
    for i in range(10001):
        u = Fraction(i, 10000)
        if not (0 <= w(u) <= 1 and w(u) == w(1-u)):
            raise AssertionError("polynomial_range_or_symmetry")
        if abs(float(height) * float(dw(u))) > vmax + 1e-12:
            raise AssertionError("velocity_bound")
        if abs(float(height) * float(ddw(u))) > amax + 1e-12:
            raise AssertionError("acceleration_bound")
    check("polynomial_10001_exact_rational_samples", True)
    ballistic_g = Fraction(981, 100)
    check("ballistic_reference_height", ballistic_g / 8 == Fraction(981, 800))
    v, a, duration = Fraction(15), Fraction(5), Fraction(1)
    lost_distance = v * duration + v*v/(2*a)
    check("race_cost_illustrative", lost_distance == Fraction(75, 2)
          and lost_distance/v == Fraction(5, 2))

    frame_bytes = 6 * 768**2 * 4
    check("depth_frame_bytes", frame_bytes == 14155776)
    check("depth_12_frames_mib", Fraction(12*frame_bytes, 1024**2) == 162)
    check("depth_slot_cap", min(12, (192*1024**2)//frame_bytes) == 12)
    check("continuous_vs_gpu_slot_peak", 5*8 + 5*8 == 80 and 80 > 64)
    check("service_freshness_not_rounded_to_8ticks", Fraction(8, 120) > Fraction(60, 1000))

    return {
        "kind": "mathematical verification only; not C++/Swift/GLSL/runtime/hardware evidence",
        "status": "pass", "checks": len(checks), "check_names": checks,
        "physics_fps_from_source": fps, "durations": tick_table,
        "identity_sequences_checked": identity_sequences,
        "polynomial_rational_samples": 10001,
        "solo_earliest_shot_times_seconds": [float(x) for x in first_shots],
        "ideal_max_animation_duty_fraction": {"solo": 1/6, "cooperative": 1/5.6},
        "geometry_design_hypothesis": {"radius_m": radius, "min_half_angle_deg": 0.75,
            "effective_max_distance_m": max_distance, "dome_center_diameter_pixels": dome_pixels,
            "equirect_vertical_reference_diameter_pixels": eq_pixels},
        "stagger_design_hypothesis": {"height_m": float(height), "duration_s": 1,
            "pitch_max_rad": math.radians(8), "height_samples_m": [float(x) for x in heights],
            "pitch_samples_deg": [float(x) for x in angles], "max_vertical_speed_m_s": vmax,
            "max_vertical_acceleration_m_s2": amax, "ballistic_1s_height_at_g_9_81_m": float(ballistic_g/8)},
        "race_cost_illustration_not_measurement": {"v_m_s": 15, "a_m_s2": 5,
            "acceleration_seconds": 3, "distance_lost_m": float(lost_distance), "equivalent_delay_seconds": 2.5},
        "capacity_model_not_benchmark": {"depth_frame_bytes": frame_bytes,
            "twelve_depth_frames_mib": 162, "twelve_frame_span_at_120_render_fps_s": 11/120,
            "twelve_frame_span_at_60_render_fps_s": 11/60, "aim_query_rate_worst_no_reuse_s": 1200,
            "mean_query_occupancy_at_10ms": 12, "mean_query_occupancy_at_50ms": 60,
            "simultaneous_5aim_plus_5press_query_slots": 80},
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = calculate()
    text = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
        print("PASS: %d controlli matematici; %s" % (result["checks"], args.output))
    else:
        print(text, end="")

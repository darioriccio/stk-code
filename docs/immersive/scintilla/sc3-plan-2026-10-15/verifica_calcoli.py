#!/usr/bin/env python3
"""Oracle matematico del piano SC3; non esegue il gioco o test C++/Swift/GLSL.

Solo libreria standard. Stampa JSON su stdout. Eseguire con python3 -B.
I risultati verificano il modello analitico, non Bullet, GPU o tempi umani.
"""
import itertools
import json
import math
from fractions import Fraction


def close(a, b, tolerance=1e-10):
    assert abs(a - b) <= tolerance, (a, b)


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def norm(a):
    return math.sqrt(dot(a, a))


def unit(a):
    n = norm(a)
    return tuple(x / n for x in a)


def ceil_ticks(us, frequency):
    return (us * frequency + 999999) // 1000000


def positive_roots(a, b, c, scale_a=1):
    if abs(a) <= 1e-12 * max(scale_a, 1):
        if abs(b) <= 1e-12 * max(abs(b), 1):
            return []
        roots = [-c / b]
    else:
        discriminant = b * b - 4 * a * c
        tolerance = 1e-12 * max(b * b, abs(4 * a * c), 1)
        if discriminant < -tolerance:
            return []
        q = -0.5 * (b + math.copysign(math.sqrt(max(0, discriminant)), b))
        roots = [q / a, c / q] if q else [-b / (2 * a)]
    return sorted(t for t in roots if math.isfinite(t) and t > 0)


def intercept(r, velocity, speed=40):
    if speed <= 0 or not all(math.isfinite(x) for x in (*r, *velocity, speed)):
        return None
    a = dot(velocity, velocity) - speed * speed
    b = 2 * dot(r, velocity)
    c = dot(r, r)
    if c == 0:
        return None
    roots = positive_roots(a, b, c, max(dot(velocity, velocity), speed * speed))
    if not roots:
        return None
    t = roots[0]
    direction = tuple((x + v * t) / (speed * t) for x, v in zip(r, velocity))
    close(norm(direction), 1)
    close(norm(tuple(x + v * t for x, v in zip(r, velocity))), speed * t)
    return {"time_seconds": t, "direction": direction, "profile_admitted": .35 <= t <= .90}


def rear_direction(s, t, aspect=2):
    h = math.tan(math.radians(50))
    return unit((-(2 * s - 1) * h, (1 - 2 * t) * h / aspect, -1))


def inverse_rear(d, aspect=2):
    h = math.tan(math.radians(50))
    return (.5 * (1 - d[0] / (-d[2] * h)), .5 * (1 - d[1] / (-d[2] * h / aspect)))


def angular_distance(degrees, center):
    delta = math.radians(degrees - center)
    return abs(math.degrees(math.atan2(math.sin(delta), math.cos(delta))))


def sphere_contact(p0, p1, b0, b1, radius):
    r = tuple(p - b for p, b in zip(p0, b0))
    w = tuple(pend - pstart - (bend - bstart)
              for pstart, pend, bstart, bend in zip(p0, p1, b0, b1))
    if dot(r, r) <= radius * radius:
        return 0.0
    roots = positive_roots(dot(w, w), 2 * dot(r, w), dot(r, r) - radius * radius)
    return next((u for u in roots if u <= 1), None)


def run():
    checks = []
    result = {"scope": "pure_math_only_not_native_or_hardware_validation"}
    ticks = {}
    for frequency in (60, 120, 144):
        ticks[str(frequency)] = {}
        for us in (3000000, 2500000, 600000, 250000, 8000000, 12000000, 5000000, 500000, 900000):
            minimum = ceil_ticks(us, frequency)
            maximum = us * frequency // 1000000
            assert Fraction(minimum, frequency) >= Fraction(us, 1000000)
            assert Fraction(maximum, frequency) <= Fraction(us, 1000000)
            assert 0 <= Fraction(minimum, frequency) - Fraction(us, 1000000) < Fraction(1, frequency)
            ticks[str(frequency)][str(us)] = {"min": minimum, "max": maximum}
    result["ticks"] = ticks
    checks.append("ceil_minimum_floor_maximum_three_frequencies")

    budgets = []
    for frequency in (60, 120, 144):
        q = 1000 * frequency
        for crew, warning_us in ((1, 3000000), (2, 2500000)):
            warning = ceil_ticks(warning_us, frequency)
            margin = ceil_ticks(250000, frequency)
            hold = ceil_ticks(600000, frequency)
            reserve = 5 * q + 20000 * (warning + margin)
            max_heat = 100 * q - 25000 * (warning + margin)
            energy_after = reserve - 5 * q - 20000 * warning
            heat_after = max_heat + 25000 * warning
            assert energy_after >= 0 and heat_after < 100 * q
            # Integrale razionale indipendente dal fixed point.
            close(reserve / q, float(5 + 20 * Fraction(warning + margin, frequency)))
            close(energy_after / q, float(20 * Fraction(margin, frequency)))
            e_steps = reserve - 5 * q
            for _ in range(warning):
                e_steps -= 20000
            assert e_steps == energy_after
            budgets.append({"frequency": frequency, "crew": crew,
                            "reserve": reserve / q, "max_initial_heat": max_heat / q,
                            "full_defense_cost": (5 * q + 20000 * warning) / q,
                            "minimum_defense_cost": (5 * q + 20000 * hold) / q,
                            "energy_after": energy_after / q, "heat_after": heat_after / q,
                            "energy_recovery_seconds": (reserve - energy_after) / (5 * q),
                            "heat_recovery_seconds": (heat_after - max_heat) / (20 * q)})
    result["budgets"] = budgets
    checks.append("fixed_point_budgets_match_exact_integrals_and_stepwise_consumption")

    assert angular_distance(39, 0) <= 40
    assert angular_distance(44, 0) > 40
    assert angular_distance(49, 0) <= 50
    assert angular_distance(51, 0) > 50
    close(angular_distance(179, -179), 2)
    checks.append("sector_admission_retention_and_wrap")

    for aspect in (2, 369 / 184, 737 / 369):
        for s, t in itertools.product((0, .1, .5, .9, 1), repeat=2):
            ray = rear_direction(s, t, aspect)
            back = inverse_rear(ray, aspect)
            close(back[0], s)
            close(back[1], t)
            close(norm(ray), 1)
    assert rear_direction(.5, .5) == (0.0, 0.0, -1.0)
    assert rear_direction(1, .5)[0] < 0
    result["rear_projection"] = {
        "horizontal_tan": math.tan(math.radians(50)),
        "vertical_fov_degrees_aspect2": math.degrees(2 * math.atan(math.tan(math.radians(50)) / 2)),
        "upper_right_direction": rear_direction(1, 0),
        "rotation_determinant": (-1) * 1 * (-1)}
    checks.append("rear_round_trip_unmirrored_basis_and_rounded_rectangle_aspects")

    mirror_bound = math.hypot(.36, .70)
    node_bound = math.hypot(.6, .62) + .07
    boost_bound = .85 + .06
    assert max(mirror_bound, node_bound, boost_bound) < 1
    assert .62 - .07 > .52  # bars end before nodes
    assert .25 + .055 < .42  # feedback ends before bars
    assert .08 + .055 < .25 - .055  # feedback rows do not overlap
    assert .16 > 2 * .055  # adjacent source badges do not overlap
    assert math.hypot(.2, .85 - .62) > .07 + .06  # boost vs middle nodes
    rectangles = []
    for width, height in ((1024, 1024), (2048, 1024), (2048, 2048)):
        size = min(width, height)
        rect = [width / 2 - .18 * size, height / 2 - .35 * size,
                width / 2 + .18 * size, height / 2 - .17 * size]
        close((rect[2] - rect[0]) / (rect[3] - rect[1]), 2)
        rectangles.append({"dimensions": [width, height], "rectangle_pixels": rect,
                           "pixel_area_fraction": .36 * .18 * size * size / (width * height)})
    result["layout"] = {"mirror_radius_bound": mirror_bound, "node_radius_bound": node_bound,
                        "boost_radius_bound": boost_bound, "rectangles": rectangles}
    checks.append("layout_inside_disc_and_declared_regions_separated_not_optical_stamp_validation")

    velocities = ((0, 0, 0), (0, 0, 10), (10, 0, 0), (0, 0, -10), (0, 0, 40), (0, 0, -40))
    examples = [intercept((0, 0, 20), v) for v in velocities]
    for example, expected in zip(examples[:4], (.5, 2 / 3, 1 / math.sqrt(3.75), .4)):
        close(example["time_seconds"], expected)
        assert example["profile_admitted"]
    assert examples[4] is None
    close(examples[5]["time_seconds"], .25)
    assert not examples[5]["profile_admitted"]
    assert positive_roots(1, -2, 1) == [1, 1]
    assert positive_roots(1, 0, 1) == []
    assert intercept((0, 0, 0), (0, 0, 0)) is None
    assert intercept((0, 0, float("nan")), (0, 0, 0)) is None
    assert intercept((0, 0, 20), (float("inf"), 0, 0)) is None
    result["ballistics"] = [dict(velocity=v, solution=e) for v, e in zip(velocities, examples)]
    checks.append("stable_ballistic_roots_and_profile_rejections")

    static = sphere_contact((-1, 0, 0), (1, 0, 0), (0, 0, 0), (0, 0, 0), .2)
    moving = sphere_contact((-1, 0, 0), (1, 0, 0), (1, 0, 0), (-1, 0, 0), .2)
    close(static, .4)
    close(moving, .45)
    assert 40 / 120 > 2 * .12
    result["sphere_ccd_oracle_only"] = {"static_fraction": static, "moving_fraction": moving}
    checks.append("continuous_sphere_cases_that_endpoint_tests_miss")

    rho, phi, error = 1, math.radians(30), .001
    n = math.ceil(phi / (2 * math.acos(1 - error / rho)))
    actual = rho * (1 - math.cos(phi / (2 * n)))
    assert n == 6 and actual <= error
    assert rho * (1 - math.cos(phi / (2 * (n - 1)))) > error
    result["child_offset_subdivision"] = {"steps": n, "error_m": actual}
    checks.append("compound_child_arc_sagitta_bound")

    u = (0, 1, 0)
    velocity = (10, -2, 20)
    vn = tuple(x * dot(velocity, u) for x in u)
    vt = tuple(x - y for x, y in zip(velocity, vn))
    after = tuple(n + .85 * t for n, t in zip(vn, vt))
    assert after == (8.5, -2.0, 17.0)
    close(dot(after, u), dot(velocity, u))
    result["impact"] = {"before": velocity, "after": after,
                        "tangential_kinetic_energy_fraction": .85 ** 2}
    checks.append("impact_preserves_normal_velocity_and_has_stated_energy_ratio")

    # L <= actual presentation <= H <= authoritative start. Hence start+T-actual >= T.
    for low, high, start in ((100, 120, 121), (0, 400000, 408333)):
        for actual in (low, (low + high) / 2, high):
            assert start >= high and start + 3000000 - actual >= 3000000
    checks.append("warning_minimum_after_interval_upper_bound")
    result["checks"] = checks
    result["passed"] = len(checks)
    return result


if __name__ == "__main__":
    print(json.dumps(run(), indent=2, ensure_ascii=False, allow_nan=False))

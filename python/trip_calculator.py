import argparse
import math
import sys

from relay_curves import IEC_CURVES, IEEE_CURVES, trip_time


CURVE_ALIASES = {
    "si": "standard_inverse",
    "standard": "standard_inverse",
    "standard_inverse": "standard_inverse",
    "vi": "very_inverse",
    "very_inverse": "very_inverse",
    "ei": "extremely_inverse",
    "extremely_inverse": "extremely_inverse",
    "lti": "long_time_inverse",
    "long_time_inverse": "long_time_inverse",
    "moderate_inv": "ieee_moderately_inverse",
    "mod_inv": "ieee_moderately_inverse",
    "ieee_moderately_inverse": "ieee_moderately_inverse",
    "ieee_very_inverse": "ieee_very_inverse",
    "ieee_extremely_inverse": "ieee_extremely_inverse",
}


def normalize_curve(curve):
    key = curve.strip().lower()
    if key in CURVE_ALIASES:
        return CURVE_ALIASES[key]
    if key in IEC_CURVES or key in IEEE_CURVES:
        return key

    valid = sorted(CURVE_ALIASES)
    raise argparse.ArgumentTypeError(
        f"unknown curve '{curve}'. Valid choices include: {', '.join(valid)}"
    )


def positive_float(name):
    def parse(value):
        try:
            number = float(value)
        except ValueError as exc:
            raise argparse.ArgumentTypeError(f"{name} must be a number") from exc
        if number <= 0:
            raise argparse.ArgumentTypeError(f"{name} must be greater than zero")
        return number

    return parse


def prompt_float(label):
    parser = positive_float(label)
    while True:
        value = input(f"{label}: ").strip()
        try:
            return parser(value)
        except argparse.ArgumentTypeError as exc:
            print(f"Error: {exc}")


def prompt_curve():
    print("Curves: SI, VI, EI, LTI, moderate_inv, ieee_very_inverse, ieee_extremely_inverse")
    while True:
        value = input("curve: ").strip()
        try:
            return normalize_curve(value)
        except argparse.ArgumentTypeError as exc:
            print(f"Error: {exc}")


def print_result(tms, curve, ipickup, current):
    multiple = current / ipickup
    t_trip = trip_time(current, ipickup, tms, curve)

    print(f"Curve: {curve}")
    print(f"TMS/TDS: {tms:g}")
    print(f"Ipickup: {ipickup:g} A")
    print(f"I: {current:g} A")
    print(f"M = I / Ipickup = {multiple:.4f}")

    if math.isinf(t_trip):
        print("t_trip: no trip (I <= Ipickup)")
    else:
        print(f"t_trip: {t_trip:.6f} s")


def main():
    if len(sys.argv) == 1:
        print("IDMT theoretical trip-time calculator")
        tms = prompt_float("tms")
        curve = prompt_curve()
        ipickup = prompt_float("ipickup")
        current = prompt_float("i")
        print()
        print_result(tms, curve, ipickup, current)
        return

    parser = argparse.ArgumentParser(
        description="Calculate theoretical IDMT relay trip time."
    )
    parser.add_argument("--tms", required=True, type=positive_float("tms"))
    parser.add_argument("--curve", required=True, type=normalize_curve)
    parser.add_argument("--ipickup", required=True, type=positive_float("ipickup"))
    parser.add_argument("--i", required=True, type=positive_float("i"))

    args = parser.parse_args()
    print_result(args.tms, args.curve, args.ipickup, args.i)


if __name__ == "__main__":
    main()

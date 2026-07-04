import sys
import argparse
import numpy as np
 
 
def main():
    ap = argparse.ArgumentParser(description="Verify a disp_diff bad-pixel map PNG")
    ap.add_argument("badmap", help="path to the bad-pixel map PNG")
    ap.add_argument("--expect-bad", type=float, default=None,
                    help="expected bad%% (e.g. runeval's bad value) to compare against")
    ap.add_argument("--expect-eval", type=float, default=None,
                    help="expected evaluated%% of image to compare against")
    ap.add_argument("--tol", type=float, default=0.5,
                    help="tolerance in percentage points for a PASS (default 0.5)")
    args = ap.parse_args()
 
    try:
        import imageio.v2 as imageio
        img = imageio.imread(args.badmap)
    except Exception as e:
        sys.exit(f"ERROR: could not read {args.badmap}: {e}")
 
    # take first channel if RGB
    if img.ndim == 3:
        img = img[..., 0]
 
    white = int(np.sum(img == 255))   # bad
    black = int(np.sum(img == 0))     # good
    gray  = int(np.sum(img == 64))    # not evaluated
    total = img.size
    other = total - white - black - gray
 
    evaluated = white + black
    if evaluated == 0:
        sys.exit("ERROR: no evaluated (white or black) pixels found - is this a bad-pixel map?")
 
    white_frac = 100.0 * white / evaluated          # == bad%
    eval_frac  = 100.0 * evaluated / total          # == evaluated pixels %
 
    print(f"file             : {args.badmap}")
    print(f"dimensions       : {img.shape[1]} x {img.shape[0]}")
    print(f"white (bad)      : {white}")
    print(f"black (good)     : {black}")
    print(f"gray  (masked)   : {gray}")
    if other:
        print(f"other values     : {other}  (WARNING: unexpected pixel values present)")
    print()
    print(f"bad%   (white / evaluated) : {white_frac:.2f}%")
    print(f"eval%  (evaluated / image) : {eval_frac:.2f}%")
 
    # optional comparisons
    ok = True
    if args.expect_bad is not None:
        diff = abs(white_frac - args.expect_bad)
        status = "PASS" if diff <= args.tol else "FAIL"
        if status == "FAIL":
            ok = False
        print(f"\nexpected bad%  = {args.expect_bad:.2f}  ->  got {white_frac:.2f}  "
              f"(diff {diff:.2f})  [{status}]")
    if args.expect_eval is not None:
        diff = abs(eval_frac - args.expect_eval)
        status = "PASS" if diff <= args.tol else "FAIL"
        if status == "FAIL":
            ok = False
        print(f"expected eval% = {args.expect_eval:.2f}  ->  got {eval_frac:.2f}  "
              f"(diff {diff:.2f})  [{status}]")
 
    # sanity note on 'other'
    if other:
        print("\nNote: found pixels that are not exactly 255/0/64. If you expected a clean "
              "bad-pixel map, check whether the PNG was re-encoded/compressed lossily.")
        ok = False
 
    if args.expect_bad is not None or args.expect_eval is not None:
        print("\nRESULT:", "PASS" if ok else "FAIL")
        sys.exit(0 if ok else 1)
 
 
if __name__ == "__main__":
    main()

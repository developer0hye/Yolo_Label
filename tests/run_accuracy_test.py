#!/usr/bin/env python3
"""Compare C++ ONNX inference against Ultralytics Python inference.

For each supported YOLO model series, this script:
1. Downloads the model and exports to ONNX via Ultralytics
2. Runs Ultralytics Python inference as the reference
3. Runs the C++ test_inference binary on the same ONNX + image
4. Compares detections using IoU matching

Exit code 0 if all models pass, 1 if any fail.
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


# Models to test: (pt_name, display_name)
MODELS = [
    ("yolov5nu.pt", "YOLOv5n"),
    ("yolov8n.pt", "YOLOv8n"),
    ("yolo11n.pt", "YOLO11n"),
    ("yolo26n.pt", "YOLO26n"),
]

# Comparison tolerances
IOU_THRESHOLD = 0.5
CONFIDENCE_TOLERANCE = 0.05
MATCH_RATE_THRESHOLD = 0.90


def compute_iou(a, b):
    """Compute IoU between two boxes (x, y, w, h format, normalized)."""
    ax1, ay1 = a["x"], a["y"]
    ax2, ay2 = ax1 + a["width"], ay1 + a["height"]
    bx1, by1 = b["x"], b["y"]
    bx2, by2 = bx1 + b["width"], by1 + b["height"]

    ix1 = max(ax1, bx1)
    iy1 = max(ay1, by1)
    ix2 = min(ax2, bx2)
    iy2 = min(ay2, by2)

    inter = max(0, ix2 - ix1) * max(0, iy2 - iy1)
    area_a = a["width"] * a["height"]
    area_b = b["width"] * b["height"]
    union = area_a + area_b - inter

    return inter / union if union > 0 else 0.0


def export_model(pt_name, export_dir):
    """Export a model to ONNX. Returns ONNX path or None if model unavailable."""
    try:
        from ultralytics import YOLO
    except ImportError:
        print("ERROR: ultralytics package not installed", file=sys.stderr)
        sys.exit(1)

    try:
        model = YOLO(pt_name)
        onnx_path = model.export(format="onnx", imgsz=640)
        # Move exported file to export_dir for organization
        src = Path(onnx_path)
        dst = export_dir / src.name
        if src != dst:
            src.rename(dst)
        return dst
    except Exception as e:
        print(f"  SKIP: Cannot export {pt_name}: {e}")
        return None


def run_python_inference(onnx_path, image_path, conf):
    """Run Ultralytics Python inference, return list of detections."""
    from ultralytics import YOLO

    model = YOLO(str(onnx_path))
    results = model.predict(str(image_path), conf=conf, verbose=False)

    detections = []
    for result in results:
        img_h, img_w = result.orig_shape
        for box in result.boxes:
            x1, y1, x2, y2 = box.xyxy[0].tolist()
            detections.append({
                "classId": int(box.cls[0].item()),
                "confidence": float(box.conf[0].item()),
                "x": x1 / img_w,
                "y": y1 / img_h,
                "width": (x2 - x1) / img_w,
                "height": (y2 - y1) / img_h,
            })
    return detections


def run_cpp_inference(test_binary, onnx_path, image_path, conf):
    """Run C++ test_inference binary, return parsed JSON dict."""
    cmd = [str(test_binary), str(onnx_path), str(image_path), str(conf)]
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen")
    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=120, env=env
        )
    except subprocess.TimeoutExpired:
        print(f"  ERROR: C++ binary timed out after 120s")
        return None

    if result.returncode != 0:
        print(f"  ERROR: C++ binary failed (exit {result.returncode})")
        print(f"  stderr: {result.stderr.strip()}")
        return None

    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as e:
        print(f"  ERROR: Cannot parse C++ output as JSON: {e}")
        print(f"  stdout: {result.stdout[:500]}")
        return None


def match_detections(py_dets, cpp_dets):
    """Match Python detections to C++ detections by IoU.

    Returns (matched_pairs, unmatched_py_indices, unmatched_cpp_indices).
    Each matched pair is (py_idx, cpp_idx, iou).
    """
    used_cpp = set()
    matched = []
    unmatched_py = []

    for pi, pd in enumerate(py_dets):
        best_iou = 0
        best_ci = -1
        for ci, cd in enumerate(cpp_dets):
            if ci in used_cpp:
                continue
            if cd["classId"] != pd["classId"]:
                continue
            cur_iou = compute_iou(pd, cd)
            if cur_iou > best_iou:
                best_iou = cur_iou
                best_ci = ci

        if best_ci >= 0 and best_iou >= IOU_THRESHOLD:
            matched.append((pi, best_ci, best_iou))
            used_cpp.add(best_ci)
        else:
            unmatched_py.append(pi)

    unmatched_cpp = [i for i in range(len(cpp_dets)) if i not in used_cpp]
    return matched, unmatched_py, unmatched_cpp


def compare_model(model_name, py_dets, cpp_result):
    """Compare Python and C++ detections for one model. Returns True if pass."""
    cpp_dets = cpp_result["detections"]

    print(f"  Python detections: {len(py_dets)}")
    print(f"  C++ detections:    {len(cpp_dets)}")
    print(f"  C++ version:       {cpp_result['version']}")

    if len(py_dets) == 0:
        if len(cpp_dets) == 0:
            print(f"  PASS: Both produced 0 detections")
            return True
        else:
            print(f"  PASS: Python had 0 detections, C++ had {len(cpp_dets)} (no reference to match)")
            return True

    matched, unmatched_py, unmatched_cpp = match_detections(py_dets, cpp_dets)

    match_rate = len(matched) / len(py_dets)
    print(f"  Matched: {len(matched)}/{len(py_dets)} ({match_rate:.1%})")

    # Check confidence tolerance on matched pairs
    conf_failures = []
    for pi, ci, iou_val in matched:
        py_conf = py_dets[pi]["confidence"]
        cpp_conf = cpp_dets[ci]["confidence"]
        if abs(py_conf - cpp_conf) > CONFIDENCE_TOLERANCE:
            conf_failures.append((pi, ci, py_conf, cpp_conf))

    if conf_failures:
        print(f"  Confidence mismatches ({len(conf_failures)}):")
        for pi, ci, py_c, cpp_c in conf_failures[:5]:
            print(f"    py[{pi}]={py_c:.4f} vs cpp[{ci}]={cpp_c:.4f} (diff={abs(py_c-cpp_c):.4f})")

    if unmatched_py:
        print(f"  Unmatched Python detections ({len(unmatched_py)}):")
        for pi in unmatched_py[:5]:
            d = py_dets[pi]
            print(f"    [{pi}] class={d['classId']} conf={d['confidence']:.4f}")

    if unmatched_cpp:
        print(f"  Extra C++ detections ({len(unmatched_cpp)}):")
        for ci in unmatched_cpp[:5]:
            d = cpp_dets[ci]
            print(f"    [{ci}] class={d['classId']} conf={d['confidence']:.4f}")

    passed = match_rate >= MATCH_RATE_THRESHOLD and len(conf_failures) == 0
    return passed


def main():
    parser = argparse.ArgumentParser(description="YOLO C++ vs Python accuracy test")
    parser.add_argument("--test-binary", required=True, help="Path to test_inference binary")
    parser.add_argument("--image", required=True, help="Path to test image")
    parser.add_argument("--conf", type=float, default=0.25, help="Confidence threshold")
    args = parser.parse_args()

    test_binary = Path(args.test_binary).resolve()
    image_path = Path(args.image).resolve()

    if not test_binary.exists():
        print(f"ERROR: Test binary not found: {test_binary}", file=sys.stderr)
        sys.exit(1)
    if not image_path.exists():
        print(f"ERROR: Image not found: {image_path}", file=sys.stderr)
        sys.exit(1)

    with tempfile.TemporaryDirectory() as tmpdir:
        export_dir = Path(tmpdir)
        results = {}
        all_passed = True
        tested_count = 0

        for pt_name, display_name in MODELS:
            print(f"\n{'='*60}")
            print(f"Model: {display_name} ({pt_name})")
            print(f"{'='*60}")

            # Export to ONNX
            print(f"  Exporting to ONNX...")
            onnx_path = export_model(pt_name, export_dir)
            if onnx_path is None:
                results[display_name] = "SKIP"
                continue

            print(f"  ONNX path: {onnx_path}")

            # Python inference
            print(f"  Running Python inference...")
            py_dets = run_python_inference(onnx_path, image_path, args.conf)

            # C++ inference
            print(f"  Running C++ inference...")
            cpp_result = run_cpp_inference(test_binary, onnx_path, image_path, args.conf)
            if cpp_result is None:
                results[display_name] = "FAIL (C++ error)"
                all_passed = False
                continue

            # Compare
            passed = compare_model(display_name, py_dets, cpp_result)
            tested_count += 1

            if passed:
                results[display_name] = "PASS"
                print(f"  >> PASS")
            else:
                results[display_name] = "FAIL"
                all_passed = False
                print(f"  >> FAIL")

        # Summary
        print(f"\n{'='*60}")
        print(f"SUMMARY")
        print(f"{'='*60}")
        for name, status in results.items():
            print(f"  {name}: {status}")

        if tested_count == 0:
            print(f"\nERROR: No models were tested!")
            sys.exit(1)

        if all_passed:
            print(f"\nAll {tested_count} tested models passed.")
            sys.exit(0)
        else:
            print(f"\nSome models failed!")
            sys.exit(1)


if __name__ == "__main__":
    main()

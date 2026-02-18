# SAM (Segment Anything) Pseudo Labeling Analysis

Analysis of SAM-based pseudo labeling approaches for potential integration into YOLO-Label.

## 1. SAM Integration Patterns in Labeling Tools

### Desktop Apps (Most Similar to YOLO-Label)

| Tool | Architecture | SAM Variants | Runtime |
|------|-------------|-------------|---------|
| **AnyLabeling** | Local ONNX inference (encoder + decoder split) | SAM, SAM2, MobileSAM | ONNX Runtime |
| **X-AnyLabeling** | Local ONNX inference | SAM 1/2/3, HQ-SAM, EfficientSAM, EdgeSAM | ONNX Runtime |
| **sam-yolo-labeling-tool** | Local | SAM | PyTorch/ONNX |

### Web-based / Server

| Tool | Architecture | Notes |
|------|-------------|-------|
| **CVAT** | Docker server (Nuclio functions) | Requires GPU server |
| **Label Studio** | Separate ML backend server (port 9090) | REST API communication |
| **Roboflow** | Cloud encoder + browser decoder (ONNX Runtime Web) | Hybrid |
| **Labelbox** | Server encoder + browser decoder | Hybrid |
| **V7 Labs** | Fully cloud | Auto-Annotate engine |

**Key pattern**: Desktop apps all use **ONNX Runtime for local inference** with encoder/decoder split architecture.

---

## 2. SAM Architecture — Encoder/Decoder Split is Key

SAM consists of 3 components, split into **2 ONNX files** for deployment:

```
[encoder.onnx] — Heavy, runs once per image
    Image (1, 3, 1024, 1024) → Image Embeddings (1, 256, 64, 64)

[decoder.onnx] — Lightweight, runs per click/box
    Image Embeddings + Point Coords + Point Labels + Mask Input
        → Masks + IoU Scores + Low-Res Masks
```

**Why split?**
- Encoder (ViT-H: 632M params) runs only once per image (~450ms)
- Decoder (3.87M params) runs per user click (~50ms)
- Caching encoder output enables real-time interactive segmentation

---

## 3. User Interaction Patterns (4 Modes)

### Mode A: Point Click (Most Common)
> AnyLabeling, X-AnyLabeling, CVAT, Label Studio, Labelbox, Roboflow

1. Select SAM tool
2. **Left-click** = positive point (foreground), **Right-click** = negative point (background)
3. Decoder runs per click → mask updates in real-time
4. Additional clicks for refinement
5. Confirm (Enter/F) → mask → convert to polygon/bbox

### Mode B: Bounding Box Prompt
> AnyLabeling, Label Studio, sam-yolo-labeling-tool

1. Draw a rough box
2. SAM segments the object within the box
3. Bounding rectangle of mask = precise YOLO bbox
4. **Best fit for YOLO labeling** — loose box → SAM → tight bbox

### Mode C: Auto-Everything (Full Auto Masks)
> V7, Roboflow, Supervisely

1. SAM samples grid points across the entire image
2. Automatically generates masks for all objects
3. NMS + quality filtering
4. User only assigns classes

### Mode D: Text Prompt (Grounding DINO + SAM)
> Label Studio, Roboflow, X-AnyLabeling

1. Text input ("person", "car")
2. Grounding DINO detects boxes from text
3. Boxes passed as prompts to SAM → mask generation
4. Fully automatic (zero-shot)

---

## 4. SAM Variant Performance Comparison (Desktop App Perspective)

| Model | Encoder Params | ONNX Size | Speed (GPU) | Quality |
|-------|---------------|-----------|-------------|---------|
| SAM ViT-H | 632M | ~2.5GB | ~500ms | Best |
| SAM ViT-B | 91M | ~375MB | ~150ms | High |
| **MobileSAM** | **5.78M** | **~40MB** | **~10ms** | **Good (-2~4 mIoU)** |
| EfficientSAM-S | ~20M | ~100MB | ~21ms | Good (-1.5~3.5 mIoU) |
| FastSAM | 68M (CNN) | YOLO format | ~10ms | Low |
| SAM2 Tiny | ~39M | ~155MB | Fast | Good |

**Optimal choice for desktop apps: MobileSAM**
- 66x smaller model (vs SAM ViT-H)
- Total ~10ms (encoder 8ms + decoder 4ms)
- Full ONNX support (encoder + decoder split)
- Minimal quality degradation (-2~4 mIoU)

---

## 5. SAM to YOLO Conversion Pipeline

```
SAM binary mask
  → contour detection (findContours)
    → polygon (vertex list)
      → simplified polygon (Douglas-Peucker)
        → bounding box (min/max of contour)
          → normalized YOLO format (x_center, y_center, w, h) / img dims
```

**YOLO + SAM combined workflow** (most common):
```
Image → YOLO detection → bounding boxes
Image → SAM Encoder → embeddings (once)
Each box → SAM Decoder → instance mask
Mask → tight bbox / polygon → YOLO format
```

---

## 6. Decoder ONNX Input/Output Tensors

### Inputs

| Name | Shape | Description |
|------|-------|-------------|
| `image_embeddings` | `(1, 256, 64, 64)` | Encoder output |
| `point_coords` | `(1, N, 2)` | (x, y) coordinates in 1024-space |
| `point_labels` | `(1, N)` | 0=background, 1=foreground, 2=box TL, 3=box BR, -1=padding |
| `mask_input` | `(1, 1, 256, 256)` | Previous mask (zeros if none) |
| `has_mask_input` | `(1,)` | 1.0 or 0.0 |
| `orig_im_size` | `(2,)` | Original image (H, W) |

### Outputs

| Name | Shape | Description |
|------|-------|-------------|
| `masks` | `(1, K, H, W)` | K=1 or 3, threshold at 0.0 |
| `iou_predictions` | `(1, K)` | Mask quality scores |
| `low_res_masks` | `(1, K, 256, 256)` | For iterative refinement |

---

## 7. Ultralytics SAM Support Status

- **SAM inference**: Supported (`from ultralytics import SAM`)
- **SAM ONNX export**: Limited/unsupported — official ONNX export requires Meta's `export_onnx_model.py` or community `samexporter`
- **SAM metadata**: Incompatible with Ultralytics YOLO ONNX metadata schema (names, task, stride, etc.). SAM requires a separate inference path
- **FastSAM**: Based on YOLOv8-seg, compatible with existing YOLO ONNX pipeline (but lower mask quality)

---

## 8. Key Considerations for YOLO-Label Integration

1. **Two ONNX files required**: Different from current single-file YOLO design. Needs encoder.onnx + decoder.onnx
2. **No Ultralytics metadata**: SAM ONNX files lack names/task/stride metadata, so the current YoloDetector auto-configuration approach cannot be used
3. **Interactive UI required**: Point clicks (positive/negative) + real-time mask overlay
4. **MobileSAM is optimal**: ~56MB (encoder+decoder), <300ms even on CPU
5. **Combinable with existing YOLO detection**: YOLO boxes → SAM segmentation → tight bbox/polygon

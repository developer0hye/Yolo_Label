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
| EfficientSAM3-EV-S | 0.68M | TBD | ~420ms* | 61.6 mIoU |
| EfficientSAM3-RV-M | 7.77M | TBD | ~413ms* | 65.3 mIoU |
| EfficientSAM3-TV-L | 20.62M | TBD | ~452ms* | 66.3 mIoU |

\* EfficientSAM3 speeds are Stage 1 only (encoder distillation). Full pipeline optimization pending.

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

---

## 9. EfficientSAM3 — Lightweight SAM3 Distillation

[EfficientSAM3](https://github.com/SimonZeng7108/efficientsam3) compresses SAM3 into lightweight, edge-friendly models via progressive knowledge distillation, preserving promptable concept segmentation and video tracking capabilities.

### Three-Stage Progressive Distillation

```
Stage 1: Compact Encoder (Released)
    SAM3 ViT encoder (461.84M) → Student backbones (0.68M ~ 22.4M)
    Trained on 1% SA-1B with prompt-in-the-loop supervision

Stage 2: Temporal Memory (Planned)
    SAM3 dense memory bank → Perceiver-based compact module
    Trained on SA-V dataset for video tracking

Stage 3: End-to-End PCS Fine-Tuning (Planned)
    Joint fine-tuning on SAM3 dataset
    Preserves promptable concept segmentation quality
```

### Model Variants (27 combinations)

**Image Encoder** — 9 variants across 3 backbone families:

| Model | Backbone | Encoder Params | mIoU (COCO) | Compression vs SAM3 |
|-------|----------|---------------|-------------|---------------------|
| ES-EV-S | EfficientViT-B0 | 0.68M | 61.62% | 99.85% smaller |
| ES-EV-M | EfficientViT-B1 | 4.64M | — | 99.0% smaller |
| ES-EV-L | EfficientViT-B2 | 14.98M | — | 96.8% smaller |
| ES-RV-S | RepViT-M0.9 | 4.72M | — | 99.0% smaller |
| ES-RV-M | RepViT-M1.1 | 7.77M | 65.28% | 98.3% smaller |
| ES-RV-L | RepViT-M2.3 | 22.40M | — | 95.2% smaller |
| ES-TV-S | TinyViT-5M | 5.07M | — | 98.9% smaller |
| ES-TV-M | TinyViT-11M | 10.55M | — | 97.7% smaller |
| ES-TV-L | TinyViT-21M | 20.62M | 66.29% | 95.5% smaller |

**Text Encoder** — 3 MobileCLIP variants (distilled from SAM3's 354M text encoder):

| Model | Compression | Notes |
|-------|-------------|-------|
| MobileCLIP-S0 | 87.96% smaller | Smallest |
| MobileCLIP-S1 | — | Balanced |
| MobileCLIP2-L | — | Best quality |

### SAM3 vs SAM1/2 — Key Differences

SAM3 adds capabilities beyond geometry-only segmentation:

| Feature | SAM1 | SAM2 | SAM3 |
|---------|------|------|------|
| Point/box prompts | Yes | Yes | Yes |
| Video tracking | No | Yes | Yes |
| Text prompts | No | No | Yes (built-in) |
| Concept segmentation | No | No | Yes |

EfficientSAM3 preserves all SAM3 capabilities in distilled form, including text-based prompting without needing an external model like Grounding DINO.

### ONNX / Deployment Status (as of Feb 2026)

- **PyTorch weights**: Released (Stage 1 — all 9 image encoders + 3 text encoders)
- **ONNX export**: Under development (not yet available)
- **CoreML export**: Under development
- **Stage 2/3 weights**: Not yet released

### Implications for YOLO-Label

**Advantages over MobileSAM:**
- Built-in text prompting (no separate Grounding DINO needed)
- Video tracking capability (future use for video labeling)
- Potentially smaller encoders (EfficientViT-B0: 0.68M vs MobileSAM: 5.78M)
- Active development with regular updates

**Current limitations:**
- ONNX export not yet available — cannot integrate until export pipeline is ready
- Only Stage 1 (encoder) weights released; full pipeline (Stages 2+3) still pending
- No published ONNX file sizes or CPU inference benchmarks yet
- Decoder compatibility with SAM1-style decoder not confirmed

**Recommendation:** Monitor EfficientSAM3 for ONNX export readiness. Once available, evaluate ES-RV-M (RepViT-M1.1, 7.77M params, 65.28 mIoU) as a potential MobileSAM replacement that adds text prompting support.

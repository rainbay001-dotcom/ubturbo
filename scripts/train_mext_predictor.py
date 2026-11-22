#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Offline Trainer & Weight Exporter for UBTurbo SMAP MEXT Predictive Memory.

Supports two execution backends seamlessly:
  1. PyTorch backend (CPU / CUDA / Ascend NPU via torch_npu) when `torch` is
     available, exporting binary-identical MXTF weights for C++ INT8 inference.
  2. Native C++17 trainer backend (`plugins/smap/tools/mext_train_cli.cpp`),
     compiled on-the-fly with clang++/g++ when PyTorch is not installed.
"""

from __future__ import annotations

import argparse
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
MEXT_WEIGHTS_MAGIC = 0x4D585446  # "MXTF"
MEXT_WEIGHTS_VERSION = 1
TF_D_MODEL = 64
TF_N_HEADS = 2
TF_D_HEAD = 32
TF_D_FF = 128
TF_N_LAYERS = 2
TF_CTX_LEN = 32
MEXT_DELTA_VOCAB = 194
MEXT_VMA_VOCAB = 32
MEXT_DT_VOCAB = 16
MEXT_OUT_DIM = 256


def train_with_cpp_backend(trace: str | None, out_path: Path, epochs: int, lr: float) -> int:
    compiler = shutil.which("clang++") or shutil.which("g++")
    if not compiler:
        raise RuntimeError("Neither PyTorch nor a C++17 compiler (clang++/g++) was found.")

    pred_dir = REPO_ROOT / "plugins/smap/src/user/predictor"
    cli_src = REPO_ROOT / "plugins/smap/tools/mext_train_cli.cpp"
    sources = [
        str(cli_src),
        str(pred_dir / "swap_shadow_map.cpp"),
        str(pred_dir / "tier1_table.cpp"),
        str(pred_dir / "tiny_transformer.cpp"),
        str(pred_dir / "mext_engine.cpp"),
    ]

    with tempfile.TemporaryDirectory() as tmpdir:
        bin_path = Path(tmpdir) / "mext_train_cli"
        compile_cmd = [
            compiler,
            "-std=c++17",
            "-O3",
            f"-I{REPO_ROOT / 'plugins/smap/src/user'}",
            *sources,
            "-lpthread",
            "-o",
            str(bin_path),
        ]
        subprocess.run(compile_cmd, check=True)

        run_cmd = [
            str(bin_path),
            "--out",
            str(out_path),
            "--epochs",
            str(epochs),
            "--lr",
            str(lr),
        ]
        if trace:
            run_cmd.extend(["--trace", trace])
        subprocess.run(run_cmd, check=True)
    return 0


def train_with_pytorch(trace: str | None, out_path: Path, epochs: int, lr: float) -> int:
    import torch
    import torch.nn as nn
    import torch.nn.functional as F

    class RmsNorm(nn.Module):
        def __init__(self, dim: int) -> None:
            super().__init__()
            self.weight = nn.Parameter(torch.ones(dim))

        def forward(self, x: torch.Tensor) -> torch.Tensor:
            rms = torch.sqrt(x.pow(2).mean(dim=-1, keepdim=True) + 1e-5)
            return (x / rms) * self.weight

    class RoPEBlock(nn.Module):
        def __init__(self) -> None:
            super().__init__()
            self.rms_attn = RmsNorm(TF_D_MODEL)
            self.wq = nn.Linear(TF_D_MODEL, TF_D_MODEL, bias=False)
            self.wk = nn.Linear(TF_D_MODEL, TF_D_MODEL, bias=False)
            self.wv = nn.Linear(TF_D_MODEL, TF_D_MODEL, bias=False)
            self.wo = nn.Linear(TF_D_MODEL, TF_D_MODEL, bias=False)
            self.rms_ffn = RmsNorm(TF_D_MODEL)
            self.w1 = nn.Linear(TF_D_MODEL, TF_D_FF, bias=False)
            self.w2 = nn.Linear(TF_D_FF, TF_D_MODEL, bias=False)

        @staticmethod
        def apply_rope(x: torch.Tensor) -> torch.Tensor:
            # x: [B, H, T, D_HEAD]
            t_len = x.shape[2]
            idx = torch.arange(0, TF_D_HEAD // 2, device=x.device, dtype=torch.float32)
            inv_freq = 1.0 / (1000.0 ** (2.0 * idx / TF_D_HEAD))
            pos = torch.arange(0, t_len, device=x.device, dtype=torch.float32)
            angles = torch.outer(pos, inv_freq)  # [T, D_HEAD/2]
            cs = torch.cos(angles)[None, None, :, :]
            sn = torch.sin(angles)[None, None, :, :]
            x0 = x[..., 0::2]
            x1 = x[..., 1::2]
            out = torch.empty_like(x)
            out[..., 0::2] = x0 * cs - x1 * sn
            out[..., 1::2] = x0 * sn + x1 * cs
            return out

        def forward(self, x: torch.Tensor) -> torch.Tensor:
            bsz, seqlen, _ = x.shape
            u = self.rms_attn(x)
            q = self.apply_rope(self.wq(u).view(bsz, seqlen, TF_N_HEADS, TF_D_HEAD).transpose(1, 2))
            k = self.apply_rope(self.wk(u).view(bsz, seqlen, TF_N_HEADS, TF_D_HEAD).transpose(1, 2))
            v = self.wv(u).view(bsz, seqlen, TF_N_HEADS, TF_D_HEAD).transpose(1, 2)
            attn = F.scaled_dot_product_attention(q, k, v, is_causal=True)
            ctx = attn.transpose(1, 2).contiguous().view(bsz, seqlen, TF_D_MODEL)
            x = x + self.wo(ctx)
            f = F.relu(self.w1(self.rms_ffn(x))).pow(2)
            return x + self.w2(f)

    class PyMextTinyTransformer(nn.Module):
        def __init__(self) -> None:
            super().__init__()
            self.emb_dvpn = nn.Embedding(MEXT_DELTA_VOCAB, TF_D_MODEL)
            self.emb_devict = nn.Embedding(MEXT_DELTA_VOCAB, TF_D_MODEL)
            self.emb_vma = nn.Embedding(MEXT_VMA_VOCAB, TF_D_MODEL)
            self.emb_dt = nn.Embedding(MEXT_DT_VOCAB, TF_D_MODEL)
            self.layers = nn.ModuleList([RoPEBlock() for _ in range(TF_N_LAYERS)])
            self.rms_final = RmsNorm(TF_D_MODEL)
            self.head = nn.Linear(TF_D_MODEL, MEXT_OUT_DIM, bias=True)
            nn.init.constant_(self.head.bias, -2.8)

        def forward(self, dvpn: torch.Tensor, devict: torch.Tensor, vma: torch.Tensor, dt: torch.Tensor) -> torch.Tensor:
            x = self.emb_dvpn(dvpn) + self.emb_devict(devict) + self.emb_vma(vma) + self.emb_dt(dt)
            for blk in self.layers:
                x = blk(x)
            return self.head(self.rms_final(x))

    # If a binary trace path was passed, delegate trace parsing to C++ CLI for identical SwapShadowMap parity
    if trace:
        return train_with_cpp_backend(trace, out_path, epochs, lr)

    torch.manual_seed(42)
    model = PyMextTinyTransformer()
    opt = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=1e-4)

    # Synthetic curriculum matching C++ trainer
    bsz = 32
    dvpn = torch.randint(60, 72, (bsz, TF_CTX_LEN))
    devict = torch.randint(64, 68, (bsz, TF_CTX_LEN))
    vma = torch.randint(0, MEXT_VMA_VOCAB, (bsz, TF_CTX_LEN))
    dt = torch.full((bsz, TF_CTX_LEN), 5, dtype=torch.long)
    targets = torch.zeros((bsz, TF_CTX_LEN, MEXT_OUT_DIM))
    for b in range(bsz):
        s = (b % 4) + 1
        dvpn[b] = 64 + s
        devict[b] = 65
        for k in range(1, 5):
            targets[b, :, 64 + s * k - 1] = 1.0
            targets[b, :, 128 + 64 + k - 1] = 1.0

    pos_weight = torch.full((MEXT_OUT_DIM,), 6.0)
    first_loss = 0.0
    last_loss = 0.0
    for ep in range(epochs):
        opt.zero_grad()
        logits = model(dvpn, devict, vma, dt)
        loss = F.binary_cross_entropy_with_logits(logits, targets, pos_weight=pos_weight)
        loss.backward()
        opt.step()
        if ep == 0:
            first_loss = float(loss.item())
        last_loss = float(loss.item())

    # Pack exact C++ TinyTransformerFp32Params layout
    tensors = [
        model.emb_dvpn.weight,
        model.emb_devict.weight,
        model.emb_vma.weight,
        model.emb_dt.weight,
    ]
    for blk in model.layers:
        tensors.extend([
            blk.rms_attn.weight,
            blk.wq.weight,
            blk.wk.weight,
            blk.wv.weight,
            blk.wo.weight,
            blk.rms_ffn.weight,
            blk.w1.weight,
            blk.w2.weight,
        ])
    tensors.extend([model.rms_final.weight, model.head.weight, model.head.bias])

    payload = b"".join(t.detach().cpu().contiguous().to(torch.float32).numpy().tobytes() for t in tensors)
    hdr = struct.pack(
        "<6I",
        MEXT_WEIGHTS_MAGIC,
        MEXT_WEIGHTS_VERSION,
        TF_D_MODEL,
        TF_N_LAYERS,
        MEXT_OUT_DIM,
        len(payload),
    )
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(hdr + payload)
    print(f"[train_mext_predictor:pytorch] epochs={epochs} loss: {first_loss:.4f} -> {last_loss:.4f} saved={out_path}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Train MEXT TinyTransformer page predictor")
    parser.add_argument("--trace", type=str, default=None, help="Optional binary mext_ring_event trace path")
    parser.add_argument("--out", type=Path, default=Path("mext_tiny_tf.bin"), help="Output MXTF weights file")
    parser.add_argument("--epochs", type=int, default=15, help="Training epochs")
    parser.add_argument("--lr", type=float, default=0.01, help="Learning rate")
    parser.add_argument("--backend", choices=["auto", "pytorch", "cpp"], default="auto")
    args = parser.parse_args()

    args.out.parent.mkdir(parents=True, exist_ok=True)
    if args.backend == "cpp":
        return train_with_cpp_backend(args.trace, args.out, args.epochs, args.lr)
    if args.backend == "pytorch":
        return train_with_pytorch(args.trace, args.out, args.epochs, args.lr)

    try:
        import torch  # noqa: F401
        return train_with_pytorch(args.trace, args.out, args.epochs, args.lr)
    except ImportError:
        return train_with_cpp_backend(args.trace, args.out, args.epochs, args.lr)


if __name__ == "__main__":
    sys.exit(main())

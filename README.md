# CEPARCO - SIMD Programming

> **Authors:**
> 
>   - Abdulrahman, Saoud
>   - Lim, Justin Lance
>   - Lopez, Kent Xavier
>   - Nicolas, Francis Medin
> 
> **Section:** S01  

---

## 1. Performance Benchmarks & Execution Time

### 1.1 Execution Time Output (All Kernels)
**Debug build**

<img width="530" height="319" alt="image" src="https://github.com/user-attachments/assets/f752ff67-5013-4a1c-bf74-877b5f5be7d8" />
<img width="533" height="323" alt="image" src="https://github.com/user-attachments/assets/1248b8f4-aa27-4346-9595-aef50688a5cd" />
<img width="531" height="325" alt="image" src="https://github.com/user-attachments/assets/b8d42e2e-7ac5-4db7-9d0a-e89f24341c6c" />

*Figure 1.1a: Program output (Debug, x64) with execution time for all kernels at n = 2^20, 2^26 and 2^29.*

<img width="825" height="502" alt="image" src="https://github.com/user-attachments/assets/21101bbb-a9e3-4f39-91c0-8ea4a16044da" />
<img width="826" height="498" alt="image" src="https://github.com/user-attachments/assets/abbc9ef5-7c10-4ba8-a5ee-e3948b102b4c" />
<img width="832" height="495" alt="image" src="https://github.com/user-attachments/assets/a7ad12ab-38f5-4233-83cc-1c5c3a4f4a04" />

*Figure 1.1b: Program output (Release, x64) with execution time for all kernels at n = 2^20, 2^26 and 2^29.*

### 1.2 Comparative Performance Table
Average of 30 runs, in milliseconds. Speedup = baseline time / kernel time (below 1.00x means slower than the baseline).

**n = 2^20 (1,048,576 elements)**

| Implementation / Kernel | Register Size | Debug (ms) | Release (ms) | vs C (Debug) | vs C (Release) | vs x86-64 (Debug) | vs x86-64 (Release) | Correct |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C (baseline)** | Scalar | 1.4094 | 0.2288 | 1.00x | 1.00x | 0.30x | 1.84x | Yes |
| **x86-64 Assembly** | 64-bit GPR / scalar SSE | 0.4225 | 0.4212 | 3.34x | 0.54x | 1.00x | 1.00x | Yes |
| **SIMD (XMM)** | 128-bit | 0.1659 | 0.2374 | 8.50x | 0.96x | 2.55x | 1.77x | Yes |
| **SIMD (YMM)** | 256-bit | 0.1430 | 0.1930 | 9.85x | 1.19x | 2.95x | 2.18x | Yes |

**n = 2^26 (67,108,864 elements)**

| Implementation / Kernel | Register Size | Debug (ms) | Release (ms) | vs C (Debug) | vs C (Release) | vs x86-64 (Debug) | vs x86-64 (Release) | Correct |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C (baseline)** | Scalar | 93.3782 | 32.2542 | 1.00x | 1.00x | 0.41x | 1.10x | Yes |
| **x86-64 Assembly** | 64-bit GPR / scalar SSE | 38.6684 | 35.4736 | 2.41x | 0.91x | 1.00x | 1.00x | Yes |
| **SIMD (XMM)** | 128-bit | 32.9507 | 30.7271 | 2.83x | 1.05x | 1.17x | 1.15x | Yes |
| **SIMD (YMM)** | 256-bit | 36.0716 | 33.9873 | 2.59x | 0.95x | 1.07x | 1.04x | Yes |

**n = 2^29 (536,870,912 elements)**

| Implementation / Kernel | Register Size | Debug (ms) | Release (ms) | vs C (Debug) | vs C (Release) | vs x86-64 (Debug) | vs x86-64 (Release) | Correct |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C (baseline)** | Scalar | 738.8029 | 245.5390 | 1.00x | 1.00x | 0.41x | 1.08x | Yes |
| **x86-64 Assembly** | 64-bit GPR / scalar SSE | 304.9005 | 264.0704 | 2.42x | 0.93x | 1.00x | 1.00x | Yes |
| **SIMD (XMM)** | 128-bit | 245.8114 | 242.5343 | 3.01x | 1.01x | 1.24x | 1.09x | Yes |
| **SIMD (YMM)** | 256-bit | 267.4229 | 241.2624 | 2.76x | 1.02x | 1.14x | 1.09x | Yes |


### 1.3 Kernel Performance Analysis
* **x86-64 Assembly vs. C Baseline:** In Debug, the C kernel is unoptimized, so the scalar asm is 3.3x (2^20) and 2.4x (2^26, 2^29) faster. In Release, the compiler optimizes the C loop (it likely vectorizes it), and the scalar asm, which handles one float per iteration, becomes 7%-46% slower than C (0.54x at 2^20, 0.91x at 2^26, 0.93x at 2^29). This means that coding a scalar assembly is no longer faster than compiler output.

* **SIMD XMM (128-bit) vs. Scalar:** XMM does 4 floats per instruction, but the gain is far from 4x: 2.55x / 1.17x / 1.24x in Debug and 1.77x / 1.15x / 1.09x in Release (2^20 / 2^26 / 2^29). The gain is largest when the data is small enough to be served from cache, and shrinks at large n.

* **SIMD YMM (256-bit) vs. SIMD XMM (128-bit):** Doubling the width helps only at 2^20 (1.16x Debug, 1.23x Release). At 2^26 and 2^29, YMM is not faster than XMM and is sometimes about more like 10% slower (0.91x / 0.92x in Debug, 0.90x / 1.01x in Release), which is about the size of the run-to-run variation we saw, because the kernel is limited by memory bandwidth, not by how many floats are processed per instruction.

---

## 2. Correctness & Boundary Verification

### 2.1 C Baseline Kernel Correctness

**Debug build**

<img width="532" height="102" alt="image" src="https://github.com/user-attachments/assets/85939366-1113-497b-a93a-c4b27fe598d9" />
<img width="541" height="99" alt="image" src="https://github.com/user-attachments/assets/9d813069-3b71-43bd-9506-3f77d62a6020" />
<img width="534" height="95" alt="image" src="https://github.com/user-attachments/assets/adcb4aa2-2d28-4894-8b98-99a45ab1305e" />

*Figure 2.1a: Debug Build Verification of output correctness against golden reference values (C implementation).*

**Release build**

<img width="645" height="118" alt="image" src="https://github.com/user-attachments/assets/3318b45c-7cb3-4a8c-8019-229b5bd34d2d" />
<img width="626" height="113" alt="image" src="https://github.com/user-attachments/assets/4d667a2d-900e-458d-b0ac-d8ffa69a52ff" />
<img width="621" height="114" alt="image" src="https://github.com/user-attachments/assets/467fd1fe-2b73-4959-a659-73b5b8e7f7b0" />

*Figure 2.1b: Release Build Verification of output correctness against golden reference values (C implementation).*

### 2.2 x86-64 Assembly Kernel Correctness

**Debug build**

<img width="312" height="48" alt="image" src="https://github.com/user-attachments/assets/2314a2aa-03d4-4fcb-ab10-77b130b8eff6" />
<img width="312" height="47" alt="image" src="https://github.com/user-attachments/assets/5301ab01-eeec-44b3-bbb6-391049b338d1" />
<img width="368" height="45" alt="image" src="https://github.com/user-attachments/assets/9b9eb7e7-1ad0-4198-8df0-eea0cd46e577" />

*Figure 2.2a: Debug Build Verification of output correctness for the x86-64 assembly implementation.*

**Release build**

<img width="390" height="56" alt="image" src="https://github.com/user-attachments/assets/9015b67d-88fb-4662-b477-6ff8b4499a47" />
<img width="380" height="54" alt="image" src="https://github.com/user-attachments/assets/dd98f59c-ce9a-4e5a-a468-922dc9678d77" />
<img width="370" height="50" alt="image" src="https://github.com/user-attachments/assets/caafaa7f-426e-4b8f-b029-21af241b4ff3" />

*Figure 2.2b: Release Build Verification of output correctness for the x86-64 assembly implementation.*

### 2.3 SIMD XMM (128-bit) Kernel Correctness

**Debug build**

<img width="326" height="49" alt="image" src="https://github.com/user-attachments/assets/64fd1fcf-bba3-4712-8924-3000040bc92a" />
<img width="326" height="41" alt="image" src="https://github.com/user-attachments/assets/653dd40f-f728-4760-afb6-09e65c141306" />
<img width="379" height="42" alt="image" src="https://github.com/user-attachments/assets/b8df8fba-359f-4fb8-a169-ec66be25fc52" />

*Figure 2.3a: Debug Build Verification of output correctness for the 128-bit SIMD (XMM) implementation.*

**Release build**

<img width="380" height="58" alt="image" src="https://github.com/user-attachments/assets/2092e6bf-e045-49df-816d-43257a262982" />
<img width="360" height="44" alt="image" src="https://github.com/user-attachments/assets/e51e4fb1-ecdc-432b-a696-be1306608e83" />
<img width="373" height="46" alt="image" src="https://github.com/user-attachments/assets/d0adfd55-c277-4ce1-913b-4b72911e7e2f" />

*Figure 2.3b: Release Build Verification of output correctness for the 128-bit SIMD (XMM) implementation.*

### 2.4 SIMD YMM (256-bit) Kernel Correctness

**Debug build**

<img width="330" height="52" alt="image" src="https://github.com/user-attachments/assets/2ef077dc-9339-4bd9-a627-fa06bd9c1986" />
<img width="322" height="43" alt="image" src="https://github.com/user-attachments/assets/514b680e-c708-44ab-a08f-e039994cfd59" />
<img width="392" height="49" alt="image" src="https://github.com/user-attachments/assets/f2e3717f-870f-4d04-aeb9-9082d5d72f81" />

*Figure 2.4a: Debug Build Verification of output correctness for the 256-bit SIMD (YMM) implementation.*

**Release build**

<img width="372" height="46" alt="image" src="https://github.com/user-attachments/assets/d7a0d028-9fdd-4f1b-b24a-669ef1253af8" />
<img width="365" height="47" alt="image" src="https://github.com/user-attachments/assets/1d0c9c3f-c3bd-40af-b7ac-956567041426" />
<img width="365" height="45" alt="image" src="https://github.com/user-attachments/assets/01476768-5d10-4989-956e-79e8ea5531da" />

*Figure 2.4b: Release Build Verification of output correctness for the 256-bit SIMD (YMM) implementation.*

### 2.5 Boundary & Alignment Checks (SIMD XMM & YMM)

<img width="497" height="987" alt="image" src="https://github.com/user-attachments/assets/003ab653-e44d-43aa-89e7-96011aea4cb8" />

*Figure 2.5a: Debug Build Correctness verification under non-vector-aligned array sizes (e.g., N % 4 != 0 or N % 8 != 0) demonstrating proper tail/remainder loop handling.*

<img width="564" height="1157" alt="image" src="https://github.com/user-attachments/assets/757ab33c-1ce1-4ed0-8476-56c4fe2f5092" />

*Figure 2.5b: Debug Build Correctness verification under non-vector-aligned array sizes (e.g., N % 4 != 0 or N % 8 != 0) demonstrating proper tail/remainder loop handling.*
---

## 3. All-Platform Analysis of the Results

### 3.1 Overall Speed Evaluation & Microarchitectural Causes
* **Fastest Platform:** `[State the fastest implementation, e.g., SIMD YMM Register Implementation].`
* **Primary Drivers of Performance:**
  1. **Data Level Parallelism (DLP):** Packaged execution allowed processing `[e.g., 8 elements per instruction using AVX2 YMM registers]`.
  2. **Register Allocation & Memory Access:** Reduced stack frame overhead and minimized instruction cache misses.
  3. **Instruction-Level Parallelism (ILP) & Pipelining:** Reduced data dependencies in SIMD operations allowed execution units to stay saturated.

### 3.2 Theoretical vs. Real-World Speedup
* **Theoretical Maximum Speedup:** 
  * 128-bit SIMD (XMM): Up to **4x** (for 32-bit floats) / **2x** (for 64-bit doubles).
  * 256-bit SIMD (YMM): Up to **8x** (for 32-bit floats) / **4x** (for 64-bit doubles).
* **Observed Speedup:** `[X.XXx for XMM, Y.YYx for YMM]`
* **Why Real-World Performance Deviates from Theoretical Limits:**
  * **Memory Bandwidth Bottleneck:** SIMD compute capability outpaces RAM/L3 cache bandwidth. Data transfers restrict total throughput (memory-bound kernel).
  * **Scalar Tail Loops / Alignment Overhead:** Processing non-aligned array tails with scalar instructions adds execution overhead.
  * **AVX Frequency Scaling / Thermal Throttling:** Executing wide YMM instructions may trigger CPU core frequency step-downs.
  * **Cache Line Misses & Memory Alignment:** Unaligned memory accesses (`vmovdqu` vs `vmovdqa`) introduce clock cycle penalties.

---

## 4. Engineering Reflections, Methodology & Insights

### 4.1 Problems Encountered & Solutions

| Issue / Bug | Root Cause | Solution Implemented |
| :--- | :--- | :--- |
| **Segmentation Fault on YMM Load** | Unaligned memory access using aligned load instructions (`vmovdqa`). | Switched to unaligned load instructions (`vmovdqu`) or aligned memory using `posix_memalign` / `_mm_malloc`. |
| **Incorrect Results on Arbitrary Vector Sizes** | SIMD loop overshot array bounds when total size N was not a multiple of vector width. | Implemented a dedicated scalar cleanup/tail loop to process remaining N % vector_width elements. |
| **Unexpected Speedup Drop on Large Datasets** | L1/L2 cache capacity exceeded; main memory latency dominated execution. | Integrated cache-blocking / tiling techniques to improve temporal data locality. |

### 4.2 Unique Methodology Used
* `[Describe any special profiling tools used (e.g., Intel VTune, perf, Valgrind), assembly unrolling tricks, or automated benchmarking harness scripts].`

### 4.3 Key Insights & "AHA!" Moments
* **Insight 1:** Vectorization isn't just about compute power; **memory alignment and bandwidth limits** dominate real-world performance gains.
* **Insight 2:** Hand-written x86-6

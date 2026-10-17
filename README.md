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
![Program Output - Execution Times](docs/screenshots/execution_times_all.png)
*Figure 1.1: Complete program output displaying execution time across all kernel implementations.*

### 1.2 Comparative Performance Table

| Implementation / Kernel | Register Size | Execution Time (ms) | Speedup vs. C Baseline | Speedup vs. x86-64 | Correctness Verified |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **C Language (Baseline)** | Scalar | `[X.XX ms]` | `1.00x` | `—` | Yes |
| **x86-64 Assembly** | 64-bit GPR | `[X.XX ms]` | `[X.XXx]` | `1.00x` | Yes |
| **SIMD (XMM Register)** | 128-bit | `[X.XX ms]` | `[X.XXx]` | `[X.XXx]` | Yes |
| **SIMD (YMM Register)** | 256-bit | `[X.XX ms]` | `[X.XXx]` | `[X.XXx]` | Yes |

### 1.3 Kernel Performance Analysis
* **x86-64 Assembly vs. C Baseline:** `[Describe speedup, e.g., manual register allocation and loop unrolling reduced instruction count by X% compared to gcc compiler optimizations].`
* **SIMD XMM (128-bit) vs. Scalar:** `[Explain vectorization gains, e.g., processes 4 single-precision floats simultaneously, yielding a ~X.Xx speedup].`
* **SIMD YMM (256-bit) vs. SIMD XMM (128-bit):** `[Explain wide vector gains and bottlenecks, e.g., double the vector width yields a X.Xx speedup].`

---

## 2. Correctness & Boundary Verification

### 2.1 C Baseline Kernel Correctness
![C Kernel Output](docs/screenshots/correctness_c.png)
*Figure 2.1: Verification of output correctness against golden reference values (C implementation).*

### 2.2 x86-64 Assembly Kernel Correctness
![x86-64 Assembly Output](docs/screenshots/correctness_x86_64.png)
*Figure 2.2: Verification of output correctness for the x86-64 assembly implementation.*

### 2.3 SIMD XMM (128-bit) Kernel Correctness
![SIMD XMM Output](docs/screenshots/correctness_simd_xmm.png)
*Figure 2.3: Verification of output correctness for the 128-bit SIMD (XMM) implementation.*

### 2.4 SIMD YMM (256-bit) Kernel Correctness
![SIMD YMM Output](docs/screenshots/correctness_simd_ymm.png)
*Figure 2.4: Verification of output correctness for the 256-bit SIMD (YMM) implementation.*

### 2.5 Boundary & Alignment Checks (SIMD XMM & YMM)
![Boundary Check SIMD XMM and YMM](docs/screenshots/boundary_check_simd.png)
*Figure 2.5: Correctness verification under non-vector-aligned array sizes (e.g., N % 4 != 0 or N % 8 != 0) demonstrating proper tail/remainder loop handling.*

---

## 3. Cross-Platform & Microarchitectural Analysis

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

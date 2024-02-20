## AMD Prefetch Attacks through Power and Time

This repository contains several experiments and proof-of-concepts for the *AMD Prefetch Attacks through Power and  Time* paper. For more technical information, please refer to the paper:

* **[AMD Prefetch Attacks through Power and Time](https://www.usenix.org/conference/usenixsecurity22/presentation/lipp)** by Moritz Lipp, Daniel Gruss, Michael Schwarz

### Prerequisites

The individual proof-of-concept implementations are self-contained and come with a Makefile and an individual description that explains how to build, run and interpret the proof-of-concept.

In order to run the proof-of-concepts, the following prerequisites need to be fulfilled:

* Linux installation
  * Build tools (gcc, make)
  * [AMD energy driver](driver/amd-energy) (optional)
  * [PTEditor](https://github.com/misc0110/PTEditor/)
* AMD CPU

Throughout our experiments, we successfully evaluated our implementations on the following CPUs. However, most of the implementation should work on CPUs with the same microarchitecture.

| CPU                          | Microcode   | Microarchitecture |
| ---------------------------- | ----------- | ----------------- |
| AMD Ryzen 5 2500 U           | `0x810100b` | Zen               |
| AMD Ryzen Threadripper 1920X | `0x8001137` | Zen               |
| AMD Ryzen 5 3600             | `0x8701021` | Zen 2             |
| AMD Ryzen 7 3700X            | `0x8701021` | Zen 2             |
| AMD A10-7870K                | `0x6003106` | Steamroller       |
| AMD EPYC 7402P               | `0x830104d` | Zen               |
| AMD EPYC 7571                | `0x800126c` | Zen               |

### Proof-of-Concepts

The follow tables give an overview of all artifacts provided in this repository. Each folder contains an additional description explaining how to build, run and interpret the artifact.

#### Leakage Analysis Primitives

| Name                                                         | Description |
| ------------------------------------------------------------ | ----------- |
| [Page Table Level](leakage-analysis-primitives/mapping-level) |             |
| [TLB State](leakage-analysis-primitives/tlb-state)           |             |
| [Stalling](leakage-analysis-primitives/stalling)           |             |
| [Retirement](leakage-analysis-primitives/load-vs-prefetch)   |             |

#### Case Studies

| Name                                                         | Description                                                  |
| ------------------------------------------------------------ | ------------------------------------------------------------ |
| [KASLR Break](case-studies/kaslr-break)                      | Kernel Address Space Derandomization using Energy Consumption or the Execution Time of the prefetch instruction |
| [Leaking Kernel Memory with Spectre](case-studies/kernel-spectre)   | Combination of TLB-Evict+Prefetch and a Spectre Gadget to leak kernel memory |


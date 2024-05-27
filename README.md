# ChengOS-UEFI 

## 介绍

该项目为ChengOS的EFI源代码，负责在BDS阶段做一些预备工作为内核的运行提供一些必要的环境以及收集机器的信息以供在内核初始化的阶段时使用。该项目提供BOOTX64.EFI文件以及ccldr文件的源代码，您需要编译它们并放在EFI引导分区的指定位置，下面展示了一个最基本的EFI分区的目录结构:
```
$ tree         
.
├── ccldr
├── EFI
│   └── BOOT
│       └── BOOTX64.EFI
└── krnl

3 directories, 3 files
```

- `BOOTX64.EFI` 负责收集硬件的信息，接着加载krnl和ccldr的镜像文件，并把控制权传给ccldr。
- `ccldr` 负责根据UEFI阶段收集的内存信息来进行内核临时页表映射，初始化处理器，并把控制权移交给内核。

**为什么要先构建一个临时的页表再把控制权移交给内核?**

临时页表需要将物理内存中预留的内核空间内存映射到虚拟地址空间的高地址部分(以0xfffff00000000000为基地址)。对于`ccldr`，它最多可以映射 2 GBytes，加载器程序默认当前机器的物理内存的总大小至少为 1 GByte，内核空间的大小一般设置为 **总空闲物理内存的大小( 1 GByte 对齐)的四分之一**。分页机制从`UefiMain()`函数开始之前就已经启用，假如不使用临时页表机制而直接将控制权移交给内核，那么再执行内核镜像的代码期间将很难再将自身的镜像部分映射到虚拟地址空间的高地址部分。所以临时页表至少要将内核镜像提前映射到虚拟地址空间的高地址部分，并预留一部分空间供内核使用。UEFI程序为`ccldr`程序预留了一部分内存空间供`ccldr`来存储临时页表(这部分的地址范围为`0x100000~0x800000`，共占用 7 MBytes，虽然 7 MBytes的空间如果全部用来存储临时页表它能映射的地址范围远超 2 GBytes，但对于该系统，2 GBytes 的临时空间完全足够内核去使用了)。



## 构建

有关EKDⅡ的构建不再赘述，该项目已经提供好了`inf`文件。

在构建内核加载器`ccldr`之前，您需要确保已经安装好了`nasm`汇编器。下面提供了Debian系和Arch系的Linux发行版的安装教程，对于Windows操作系统，您需要去官网上自行下载官方提供的汇编器程序。

Debian:
```bash
sudo apt update && sudo apt install nasm -y
```

Arch:
```bash
pacman -S nasm
```

在确保您已经安装好`nasm`之后，请使用以下命令来编译ccldr:
```bash
git clone https://github.com/shichengv/ChengOS-UEFI
cd ChengOS-UEFI
nams -f bin -o ccldr ccldr.asm
```

接着，您需要把编译得到的`BOOTX64.EFI`和`ccldr`二进制文件复制到EFI分区的指定位置。

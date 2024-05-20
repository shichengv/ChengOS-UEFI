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
- `ccldr` 负责根据UEFI阶段收集的内存信息来进行内核临时页表映射，初始化处理器，并把控制权交给内核。

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

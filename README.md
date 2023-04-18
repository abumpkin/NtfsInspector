# Ntfs Inspector

查看 ntfs 文件系统原始数据的工具. 编写的初衷是方便自己研究 ntfs 文件系统的具体结构.

目前已实现的功能有:

* 通过 **扇区号(Sector Number)** 查看一个扇区数据, 以 16 进制输出.
* 通过 **簇号(Cluster Number)** 查看一个簇的数据, 以 16 进制输出.
* 查询 Ntfs 分卷的 **主文件表(MFT)**, 以摘要形式或 16 进制输出.
  * 其中 **摘要形式** 对以下属性进行解析:
    * `$STANDARD_INFORMATION`
    * `$ATTRIBUTE_LIST`
    * `$FILE_NAME`
    * `$INDEX_ROOT`
    * `$NTFS_DATA`
* 通过 **文件记录号(FRN)** 列举 **文件夹** 内的所有文件.
* 解析 Data Runs.
* 查询 `$UsnJrnl` 最新的 n 条日志记录.

## 平台要求

Windows

## 命令说明

### 打印分卷的信息

```txt
p info
```

### 打印指定扇区 16 进制

```txt
p sec <n>
```

* `n` 为 **扇区号**, 比如 `p sec 0` 打印分卷的第 0 个扇区.

### 打印指定簇号的 16 进制

```txt
p clu <n>
```

### 打印指定 **文件记录** 的摘要信息

```txt
p frn <n>
```

* `n` 为 **文件记录号**, 比如 `p frn 5` 打印分卷根目录(`.`) 文件记录的信息.

### 打印指定 **文件记录** 的 16 进制数据

```txt
p hex frn <n>
```

或

```txt
p frn <n> hex
```

* `p` 后的参数顺序可随意, 但 **参数值** 必须跟在指定参数后面.

### 单独打印 **文件记录** 中的某个属性信息

```txt
p frn <n> attrid <id>
```

* `n` 是 **文件记录号**
* `id` 是此 **文件记录** 中的 **属性 id**.

### 单独打印 **文件记录** 中的某个属性信息的 16 进制

```txt
p frn <n> attrid <id> hex
```

* `n` 是 **文件记录号**
* `id` 是此 **文件记录** 中的 **属性 id**.

### 对指定 **文件记录** 中的指定 **非驻留属性** 的 Data Runs 进行解析

```txt
p frn <n> attrid <id> info
```

* `n` 是 **文件记录号**
* `id` 是此 **文件记录** 中的 **属性 id**.

### 根据 **文件记录** 枚举文件夹内所有文件

```txt
p frn <n> dir
```

* `n` 是 **文件记录号**

### 打印 `$UsnJrnl:$J` 日志最新的 n 条

```txt
p logj [n]
```

* `n` 是打印即日志记录数量, 如果省略则默认打印 10 条.

## 总结

目前程序所实现的功能还十分有限, 仅对 Ntfs 文件系统的主体结构进行了解析, 想要对 Ntfs 文件系统进行比较全面的解析所需的工作量很大. 欢迎各位大佬对代码进行改进!

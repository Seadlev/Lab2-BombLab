# Lab 2 — BombLab 逆向分析实验报告

## 一、实验目的与环境

本实验重点是利用 `objdump` 和 GDB 观察 Bomb 程序的汇编指令、寄存器与内存，逐步还原各关卡的验证条件。实验环境是 WSL Linux x86-64。

## 二、关键汇编与逆向分析

### Phase 1：调用约定与计算函数

主函数中最重要的指令为：

```asm
mov rdi,r12
call 402050 <p1_compute>
cmp DWORD PTR [rbp-0x14],eax
sete al
```

程序将 `challenge` 传入 `p1_compute`，把返回值和输入的一个整数比较。`p1_compute` 又依次调用 `p1_step1`、`p1_step0` 和 `p1_step2`。辅助函数的关键汇编如下：

```asm
; p1_step0
lea eax,[rdi+rdi*4]
add esi,0x6d95
xor eax,esi

; p1_step1
lea eax,[rsi+0x651a]
xor eax,edi

; p1_step2
sub edi,esi
lea eax,[rdi+rdi*4+0xf59a]
```

`lea` 在此用于整数运算，如 `rdi+rdi*4` 表示五倍参数。设 `L`、`M`、`H` 分别是 `challenge` 的低 32 位、右移 16 位后的低 32 位、右移 32 位后的低 32 位，可得中间结果 `t1=(M+0x651a) XOR H`、`t0=(5L) XOR (H+0x6d95)`，最终答案为 `5(t0−t1)+0xf59a`。

### Phase 2：循环与数组递推

程序调用 `valid_u32_fields` 时将第二参数设置为 6，说明输入需要六个整数。首项由以下指令计算：

```asm
shr rax,0x29
and eax,0x3f
add eax,0x2
```

因此首项为 `((challenge >> 41) & 63) + 2`。其余五项由循环检查：

```asm
lea ecx,[rdx+0x2]
shr rax,cl
and eax,0xf
add eax,edx
xor eax,esi
mov esi,DWORD PTR [rbp+rdx*4-0x40]
lea eax,[rax+rax*4]
add rdx,0x1
cmp rdx,0x6
jne 402173
```

`rdx` 为数组下标，从 1 增加到 5；`[rbp+rdx*4-0x40]` 访问第 `rdx` 个 32 位整数。每一项等于“前一项 XOR（从 challenge 抽取的 4 位数再加下标）”的五倍。这说明该关是一个依赖前项的递推序列，而不是六个独立答案。

### Phase 3：switch 与跳转表

本关需要两个整数。第一个整数为 `challenge & 7`；第二个由 `p3_dispatch` 计算。其关键指令是：

```asm
cmp eax,0x7
ja 4022b5
notrack jmp QWORD PTR [rax*8+0x405820]
```

这表明程序通过索引访问跳转表，而非顺序执行一组条件判断。使用 GDB 命令 `x/8gx 0x405820` 读取跳转表，得到索引 0～7 分别指向 `p3_case_0、p3_case_4、p3_case_6、p3_case_7、p3_case_3、p3_case_1、p3_case_5、p3_case_2`。

设 `x` 为 `challenge` 右移 32 位与右移 3 位后的低 32 位异或，逐一分析各 case 中的 `shr`、`xor` 和 `lea`，可得计算规则。

**跳转表中的实际顺序不能仅凭 case 函数的名称判断，必须检查表中的地址。**

### Phase 4：二分递归

`phase_04` 取 `challenge` 的低 7 位作为目标值，并生成附加参数 `salt`，随后调用 `p4_walk(target,0,127,salt)`。递归函数的关键汇编为：

```asm
sub edi,esi
shr edi,1
add edi,esi
cmp edi,eax
je 4023a0
...
call 402340 <p4_walk>
lea eax,[rax+rax*2]
xor eax,ebx
xor eax,0x1a
```

前三条指令计算区间中点，即 `low + (high−low)/2`。找到目标时，程序返回 `(mid+0x9c) XOR salt`；否则以中点为界向左或向右递归。递归返回值会乘以 3，再与 `mid+salt` 以及分支对应的常数异或：左分支使用 `0x9`，右分支使用 `0x1a`。

### Phase 5：结构体与链表遍历

`p5_traverse` 先以 `challenge & 7` 从 `0x405b60` 的指针表中选取链表头。以下三条汇编揭示了节点结构：

```asm
mov esi,DWORD PTR [rdx+0x4]
xor eax,DWORD PTR [rdx]
mov rdx,QWORD PTR [rdx+0x8]
```

即节点偏移 0 处为 32 位 `key`，偏移 4 处为 32 位 `value`，偏移 8 处为指向下一节点的 64 位指针。GDB 的 `x/8gx 0x405b60` 用来确认八个链表起点；`x/64gx 0x405bd0` 和 `x/6gx 0x405ba0` 用来恢复各节点数据及 `next` 关系。

遍历次数为 `(challenge >> 3) & 3`。程序先将 challenge 的两个位段异或，再与起始节点的 `key` 和常数 `0xec77` 异或、加上 `value`。每访问一个后继节点，又将累计值与节点 `key` 异或并加上其 `value`。最终验证两个整数：累计结果以及最后访问节点的 `value`。

### Phase 6：函数指针与间接调用

`p6_dispatch` 中有两处间接调用：

```asm
call QWORD PTR [rax*8+0x405e00]
xor eax,0xc8a2
mov edi,eax
call QWORD PTR [rbx+0x405e00]
add eax,0xad5c
```

函数指针表起始地址为 `0x405e00`。用 GDB 命令 `x/8gx 0x405e00` 读取，确认索引 0～7 对应的运算函数依次是 `op_1、op_0、op_1、op_4、op_3、op_5、op_2、op_0`。

第一次选取索引使用 `challenge` 的低 3 位，第二次使用第 3～5 位。初始参数为 `challenge` 右移 6 位与右移 32 位后的低 32 位异或。第一次调用返回后异或 `0xc8a2`，再作为第二次函数的输入；最终结果增加 `0xad5c`。

### Honor：栈内存布局与隐藏校验

`secret_gate` 先调用 `strlen` 检查输入长度，并通过 `secret_copy` 逐字节复制输入。

```asm
lea r12,[rbp-0xd0]
call 402720 <secret_copy>
cmp DWORD PTR [rbp-0xb0],0x4e45504f
lea rdi,[rbp-0xac]
call 402660 <secret_phase>
```

复制缓冲区起始于 `rbp-0xd0`，标记校验位置为 `rbp-0xb0`，两者相距 32 字节。由于采用小端序，常量 `0x4e45504f` 对应字符 `OPEN`。随后 `secret_phase` 从该标记后的地址解析三个无符号整数。由此确定输入布局为 **32 字节填充 + `OPEN` + 三个整数**。

`secret_mix` 的汇编为：

```asm
lea eax,[rdi*8+0x0]
sub eax,edi
add eax,0x8e53
xor eax,0x2227
```

可还原为“参数乘以 7，加 `0x8e53`，再与 `0x2227` 异或”。`secret_phase` 进一步混合 `challenge`、`context` 的高低 32 位以及位移结果，依次校验三个整数；第一个整数为 `context & 3`，后两个根据 `secret_mix` 和后续 `xor`、`shr`、`lea` 指令推导。最终依据这些校验条件生成完整第七行。

## 三、测试与结果

`make test` 全部通过。

![](/ics/Lab2-BombLab/images/image1.png)
![](/ics/Lab2-BombLab/images/image2.png)

公开列表之外 `seed = 43` 时全部通过。

![](/ics/Lab2-BombLab/images/image3.png)

## 四、参考资料与工具使用说明

使用 ChatGPT 协助解释汇编指令并梳理推导；关键汇编与数据来自个人 Bomb 的反汇编及 GDB 输出，最终结果通过本地测试核验。

未使用 Coding Agent。
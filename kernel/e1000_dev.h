//
// E1000 hardware definitions: registers and DMA ring format.
// from the Intel 82540EP/EM &c manual.
//

/* Registers 这些全都是 E1000 网卡 (Intel 8254x 系列) 的 内存映射寄存器 (MMIO registers)，驱动通过读写它们来控制网卡的工作。*/
/*接收 (RX) 相关寄存器 E1000 使用 接收描述符环 (RX Descriptor Ring)，DMA 把收到的数据直接写入内存的 buffer。*/
#define E1000_CTL      (0x00000/4)  /* Device Control Register - RW 网卡的总控开关. 比如：启用/禁用网卡,设置速度/全双工,复位网卡*/
#define E1000_ICR      (0x000C0/4)  /* Interrupt Cause Read - R 中断状态寄存器。每个 bit 代表某种中断事件（接收包、发送完成、链路状态变化等）*/
#define E1000_IMS      (0x000D0/4)  /* Interrupt Mask Set - RW 中断屏蔽寄存器。用来打开/关闭某些中断源。*/
#define E1000_RCTL     (0x00100/4)  /* RX Control - RW 接收功能控制。比如启用接收、是否丢弃错误包、是否接受广播包、多播过滤等。*/
#define E1000_TCTL     (0x00400/4)  /* TX Control - RW 发送控制，比如开启发送、冲突重传、填充最小包长等。*/
#define E1000_TIPG     (0x00410/4)  /* TX Inter-packet gap -RW 以太网帧之间的最小间隔时间*/
#define E1000_RDBAL    (0x02800/4)  /* RX Descriptor Base Address Low - RW 接收描述符环的物理内存地址（低 32 位）*/
#define E1000_RDTR     (0x02820/4)  /* RX Delay Timer */
#define E1000_RADV     (0x0282C/4)  /* RX Interrupt Absolute Delay Timer */
#define E1000_RDH      (0x02810/4)  /* RX Descriptor Head - RW 硬件正在处理的描述符索引（由网卡更新）*/
#define E1000_RDT      (0x02818/4)  /* RX Descriptor Tail - RW 软件设置的尾指针。驱动写入新的尾索引告诉网卡还有空余 buffer 可以填。*/
#define E1000_RDLEN    (0x02808/4)  /* RX Descriptor Length - RW 描述符环的大小（字节数，必须是 16 的倍数）*/
#define E1000_RSRPD    (0x02C00/4)  /* RX Small Packet Detect Interrupt 用于优化小包接收时的中断触发。*/
#define E1000_TDBAL    (0x03800/4)  /* TX Descriptor Base Address Low - RW 发送描述符环的物理内存地址（低 32 位）*/
#define E1000_TDLEN    (0x03808/4)  /* TX Descriptor Length - RW 发送描述符环大小*/
#define E1000_TDH      (0x03810/4)  /* TX Descriptor Head - RW 硬件更新的当前发送位置。*/
#define E1000_TDT      (0x03818/4)  /* TX Descripotr Tail - RW 驱动写入的尾索引，告诉网卡可以从哪些 buffer 里取数据发出去。*/
#define E1000_MTA      (0x05200/4)  /* Multicast Table Array - RW Array 用来存放多播地址过滤表*/
#define E1000_RA       (0x05400/4)  /* Receive Address - RW Array 接收 MAC 地址（通常存放网卡的唯一硬件 MAC 地址）。
网卡会检查收到的包的目的 MAC，如果匹配这里的值才会接收。*/

/* Device Control E1000_CTL_SLU、E1000_CTL_RST 这些：表示 在 E1000_CTL 寄存器中的某一位/某几位。数值常量，用来和寄存器的值按位与/或操作*/
#define E1000_CTL_SLU     0x00000040    /* set link up */
#define E1000_CTL_FRCSPD  0x00000800    /* force speed */
#define E1000_CTL_FRCDPLX 0x00001000    /* force duplex */
#define E1000_CTL_RST     0x00400000    /* full reset */

/* Transmit Control */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_COLD_SHIFT 12
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */

/* Receive Control */
#define E1000_RCTL_RST            0x00000001    /* Software reset */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_SBP            0x00000004    /* store bad packet */
#define E1000_RCTL_UPE            0x00000008    /* unicast promiscuous enable */
#define E1000_RCTL_MPE            0x00000010    /* multicast promiscuous enab */
#define E1000_RCTL_LPE            0x00000020    /* long packet enable */
#define E1000_RCTL_LBM_NO         0x00000000    /* no loopback mode */
#define E1000_RCTL_LBM_MAC        0x00000040    /* MAC loopback mode */
#define E1000_RCTL_LBM_SLP        0x00000080    /* serial link loopback mode */
#define E1000_RCTL_LBM_TCVR       0x000000C0    /* tcvr loopback mode */
#define E1000_RCTL_DTYP_MASK      0x00000C00    /* Descriptor type mask */
#define E1000_RCTL_DTYP_PS        0x00000400    /* Packet Split descriptor */
#define E1000_RCTL_RDMTS_HALF     0x00000000    /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_QUAT     0x00000100    /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_EIGTH    0x00000200    /* rx desc min threshold size */
#define E1000_RCTL_MO_SHIFT       12            /* multicast offset shift */
#define E1000_RCTL_MO_0           0x00000000    /* multicast offset 11:0 */
#define E1000_RCTL_MO_1           0x00001000    /* multicast offset 12:1 */
#define E1000_RCTL_MO_2           0x00002000    /* multicast offset 13:2 */
#define E1000_RCTL_MO_3           0x00003000    /* multicast offset 15:4 */
#define E1000_RCTL_MDR            0x00004000    /* multicast desc ring 0 */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SZ_1024        0x00010000    /* rx buffer size 1024 */
#define E1000_RCTL_SZ_512         0x00020000    /* rx buffer size 512 */
#define E1000_RCTL_SZ_256         0x00030000    /* rx buffer size 256 */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 1 */
#define E1000_RCTL_SZ_16384       0x00010000    /* rx buffer size 16384 */
#define E1000_RCTL_SZ_8192        0x00020000    /* rx buffer size 8192 */
#define E1000_RCTL_SZ_4096        0x00030000    /* rx buffer size 4096 */
#define E1000_RCTL_VFE            0x00040000    /* vlan filter enable */
#define E1000_RCTL_CFIEN          0x00080000    /* canonical form enable */
#define E1000_RCTL_CFI            0x00100000    /* canonical form indicator */
#define E1000_RCTL_DPF            0x00400000    /* discard pause frames */
#define E1000_RCTL_PMCF           0x00800000    /* pass MAC control frames */
#define E1000_RCTL_BSEX           0x02000000    /* Buffer size extension */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */
#define E1000_RCTL_FLXBUF_MASK    0x78000000    /* Flexible buffer size */
#define E1000_RCTL_FLXBUF_SHIFT   27            /* Flexible buffer shift */

#define DATA_MAX 1518

/* Transmit Descriptor command definitions [E1000 3.3.3.1] */
#define E1000_TXD_CMD_EOP    0x01 /* End of Packet 表示这个描述符是一个数据包的 结束（End of Packet）。因为一个包可能很大，需要多个描述符才能存放（比如分片），所以最后一个描述符需要设置这个标志。*/
#define E1000_TXD_CMD_RS     0x08 /* Report Status 表示发送完成后，网卡需要 报告状态，即更新 status 字段。（不设置的话，可能发送了但驱动不知道结果）*/

/* Transmit Descriptor status definitions [E1000 3.3.3.2] */
#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done 表示发送描述符已经被网卡处理完成（Descriptor Done）。驱动通过轮询或中断方式检查这个位，确认数据已发出。*/

// [E1000 3.3.3] 发送描述符 (Transmit Descriptor)
struct tx_desc
{
  uint64 addr;// 数据缓冲区的物理地址
  uint16 length;// 数据长度
  uint8 cso;// Checksum offset（一般不用，和硬件计算校验和相关）
  uint8 cmd;// 命令 (比如 EOP, RS)
  uint8 status;// 状态 (比如 DD)
  uint8 css;// Checksum start（一般不用）
  uint16 special;// 其他特殊用途（VLAN 等）
};

/* Receive Descriptor bit definitions [E1000 3.2.3.1] */
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done 网卡写入数据到缓冲区后，设置这个位告诉驱动“这个描述符有效了”*/
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet 表示这个描述符包含了一个完整数据包的 最后一部分*/

// [E1000 3.2.3]
struct rx_desc
{
  uint64 addr;       /* Address of the descriptor's data buffer */
  uint16 length;     /* Length of data DMAed into data buffer */
  uint16 csum;       /* Packet checksum */
  uint8 status;      /* Descriptor status  状态 (DD/EOP 等)*/
  uint8 errors;      /* Descriptor Errors 错误标志*/
  uint16 special;
};


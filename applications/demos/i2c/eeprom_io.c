#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "eeprom_io.h"

#define DEV_PATH "/dev/i2c-0"  //设备文件路径
#define I2C_M_WR          0    //定义写标志
#define MAX_MSG_NR        2    //时序要两次，所以设为2
#define EEPROM_BLOCK_SIZE 256  //每个block容量256 bytes
#define EEPROM_PAGE_SIZE  16   //AT24C08(16)页大小为16字节
#define I2C_MSG_SIZE (sizeof(struct i2c_msg))


/* I2C总线ioctl方法所使用的结构体 */
typedef struct i2c_rdwr_ioctl_data ioctl_st;

/* 自定义eeprom参数结构体 */
typedef struct eeprom_data
{
    u8 slave_addr;
    u8 byte_addr;
    u8 len;
    u8 *buf;
} eeprom_st;

/* 对设备进行初始化设置, 设置超时时间及重发次数, 参数为设备文件描述符 */
static int eeprom_init(int fd)
{
    ioctl(fd, I2C_TIMEOUT, 4);
    ioctl(fd, I2C_RETRIES, 2);
    return 0;
}

/* 初始化一个ioctl_st结构, 并预分配空间 */
static ioctl_st *ioctl_st_init(void)
{
    ioctl_st *iocs;

    iocs = (ioctl_st *)malloc(sizeof(ioctl_st));
    if (!(iocs))
    {
        perror("malloc iocs:");
        return NULL;
    }

    iocs->msgs = (struct i2c_msg *)malloc(I2C_MSG_SIZE * MAX_MSG_NR);
    if (!(iocs->msgs))
    {
        perror("malloc iocs->msgs");
        return NULL;
    }

    iocs->msgs[0].buf = (unsigned char *)malloc(EEPROM_PAGE_SIZE + 1);
    if (!(iocs->msgs[0].buf))
    {
        perror("malloc iocs->msgs[0].buf");
        return NULL;
    }

    iocs->msgs[1].buf = (unsigned char *)malloc(EEPROM_PAGE_SIZE + 1);
    if (!(iocs->msgs[1].buf))
    {
        perror("malloc iocs->msgs[1].buf");
        return NULL;
    }

    return iocs;
}

/* 销毁ioctl_st结构, 释放空间 */
static void ioctl_st_release(ioctl_st *iocs)
{
    free(iocs->msgs[0].buf);
    free(iocs->msgs[1].buf);
    free(iocs->msgs);
    free(iocs);
}

/* 根据eeprom_st结构生成page_read时序所需的ioctl_st结构 */
static void page_read_st_gen(ioctl_st *iocs, eeprom_st *eeps)
{
    int size = eeps->len;

    iocs->nmsgs = 2;  //page_read需2次start信号

    /* 第1次信号 */
    iocs->msgs[0].addr = eeps->slave_addr;  //填入slave_addr
    iocs->msgs[0].flags = I2C_M_WR;         //write标志
    iocs->msgs[0].len = 1;                  //信号长度1字节
    iocs->msgs[0].buf[0] = eeps->byte_addr; //填入byte_addr

    /* 第2次信号 */
    iocs->msgs[1].addr = eeps->slave_addr;  //填入slave_addr
    iocs->msgs[1].flags = I2C_M_RD;         //read标志
    iocs->msgs[1].len = size;               //信号长度: 待读数据长度
    memset(iocs->msgs[1].buf, 0, size);     //先清零, 待读数据将自动存放于此
}

/* 用ioctl方法从eeprom中读取数据 */
static int page_read(int fd, ioctl_st *iocs, eeprom_st *eeps)
{
    int ret;
    int size = eeps->len;

    page_read_st_gen(iocs, eeps);
    ret = ioctl(fd, I2C_RDWR, (u32)iocs);
    if (ret == -1)
    {
        perror("ioctl");
        return ret;
    }

    /* 将读取的数据从ioctl结构中复制到用户buf */
    memcpy(eeps->buf, iocs->msgs[1].buf, size);
    printf("read byte ioctl ret = %d\n", ret);

    return ret;
}

/* 从eeprom的pos位置开始读取size长度数据
   @pos: eeprom的byte_address, 取值范围为0 ~ 255
   @data: 接收数据的缓冲区
   @size: 待读取的数据长度, 取值范围为1 ~ 16 */
int eeprom_page_read(int addr, u8 pos, u8 *data, int size)
{
    int fd;
    u8 *buf = data;
    eeprom_st eeps;
    ioctl_st *iocs = ioctl_st_init();

    fd = open(DEV_PATH, O_RDONLY);
    if (fd < 0)
    {
        perror("open eeprom");
        return 0;
    }

    /* 判断要读取数据的长度size有效性 */
    if (size > 16)
        size = 16;
    else if (size < 1)
        return 0;

    if (size > (EEPROM_BLOCK_SIZE - pos))
        size = EEPROM_BLOCK_SIZE - pos;

    eeprom_init(fd);
    eeps.slave_addr = addr;
    eeps.byte_addr = pos;
    eeps.len = size;
    eeps.buf = buf;

    page_read(fd, iocs, &eeps);
    ioctl_st_release(iocs);
    close(fd);

    return size;
}

/* 根据eeprom_st结构生成page_write时序所需的ioctl_st结构 */
static void page_write_st_gen(ioctl_st *iocs, eeprom_st *eeps)
{
    int size = eeps->len;

    iocs->nmsgs = 1;  //page_write只需1次start信号

    iocs->msgs[0].addr = eeps->slave_addr;  //填入slave_addr
    iocs->msgs[0].flags = I2C_M_WR;         //write标志
    iocs->msgs[0].len = size + 1; //信号长度: 待写入数据长度 + byte_addr长度
    iocs->msgs[0].buf[0] = eeps->byte_addr; //第1字节为byte_addr
    memcpy((iocs->msgs[0].buf + 1), eeps->buf, size); //copy待写数据
}

/* 用ioctl方法向eeprom中写入数据 */
static int page_write(int fd, ioctl_st *iocs, eeprom_st *eeps)
{
    int ret;

    page_write_st_gen(iocs, eeps);
    ret = ioctl(fd, I2C_RDWR, (u32)iocs);
    if (ret == -1)
    {
        perror("ioctl");
        return ret;
    }

    printf("write byte ioctl ret = %d\n", ret);

    return ret;
}

/* 自eeprom的pos位置开始写入数据
   @pos: eeprom的byte_address, 取值范围为0 ~ 255
   @data: 待写入的数据
   @size: 待写入数据的长度, 取值范围为1 ~ 16 */
int eeprom_page_write(int addr, u8 pos, u8 *data, int size)
{
    int fd;
    u8 *buf = data;
    eeprom_st eeps;
    ioctl_st *iocs = ioctl_st_init();

    fd = open(DEV_PATH, O_WRONLY);
    if (fd < 0)
    {
        perror("open eeprom");
        return 0;
    }

    /* 判断要读取数据的长度size有效性 */
    if (size > 16)
        size = 16;
    else if (size < 1)
        return 0;

    if (size > (EEPROM_BLOCK_SIZE - pos))
        size = EEPROM_BLOCK_SIZE - pos;

    eeprom_init(fd);
    eeps.slave_addr = addr;
    eeps.byte_addr = pos;
    eeps.len = size;
    eeps.buf = buf;

    page_write(fd, iocs, &eeps);
    ioctl_st_release(iocs);
    close(fd);

    return size;
}

/* 从eeprom的pos位置处读取1个字节
   @pos: eeprom的byte_address, 取值范围为0 ~ 255
   返回值为读取的字节数据 */
int eeprom_byte_read(int addr, u8 pos)
{
    u8 buf;
    eeprom_page_read(addr, pos, &buf, 1);

    return buf;
}

/* 将1个字节数据写入eeprom的pos位置
   @pos: eeprom的byte_address, 取值范围为0 ~ 255
   @data: 待写入的字节数据 */
int eeprom_byte_write(int addr, u8 pos, u8 data)
{
    int ret;
    u8 buf = data;
    ret = eeprom_page_write(addr, pos, &buf, 1);

    return ret;
}


int eeprom_test(void)
{
    u8 pos = 3;
    u8 data[5] = {32, 15, 99, 250, 6};

    eeprom_byte_write(ee_addr7, pos, data[0]); //单字节写操作
    usleep(10 * 1000);
    data[0] = eeprom_byte_read(ee_addr7, pos); //单字节读操作
    printf("%x\n", data[0]);
    usleep(10 * 1000);
    eeprom_page_write(ee_addr7, pos, data, 5); //多字节写操作
    memset(data, 0, 5);
    usleep(10 * 1000);
    eeprom_page_read(ee_addr7, pos, data, 5);  //多字节读操作

    printf("%x %x %x %x %x\n", data[0], data[1], data[2], data[3], data[4]);

    return 0;
}
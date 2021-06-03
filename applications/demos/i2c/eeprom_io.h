/****************************************************************************************
Copyright (C), 2015-2016, JiuZhou Tech. Co., Ltd.
     Author:  Alan
    Version:  1.00
       Date:  2015.11.20
*****************************************************************************************/

#ifndef _EEPROM_IO_H
#define _EEPROM_IO_H

typedef unsigned char u8;
typedef unsigned int u32;

/* eeprom的slave_address, 对于AT24C08(24C16), 容量为1024(2048)bytes, 由于byte_address
   仅有8位, 只能表示0 ~ 255 bytes, 故此需在slave_address中分出2(3)位来配合
   byte_address一起寻址, 而slave_address可以取值0x50, 0x51, 0x52, 0x53,(0x54, 0x55, 0x56, 0x57)
   配合byte_address刚好可以实现0 ~ 1023(2047) bytes的寻址, 另外byte_address
   取值范围从0 ~ 255, 若大于255则按其值对256取余计算 */
enum ee_addr
{
    ee_addr0 = 0x50,
    ee_addr1 = 0x51,
    ee_addr2 = 0x52,
    ee_addr3 = 0x53,
    ee_addr4 = 0x54,
    ee_addr5 = 0x55,
    ee_addr6 = 0x56,
    ee_addr7 = 0x57,
};

/* 从eeprom的pos位置处读取1个字节
   @pos: eeprom的byte_address, 取值范围为0 ~ 255 */
int eeprom_byte_read(int ee_addr, u8 pos);

/* 将1个字节数据写入eeprom的pos位置
   @pos: eeprom的byte_address, 取值范围为0 ~ 255
   @data: 待写入的字节数据 */
int eeprom_byte_write(int ee_addr, u8 pos, u8 data);

/* 从eeprom的pos位置开始读取size长度数据
   @pos: eeprom的byte_address, 取值范围为0 ~ 255
   @data: 接收数据的缓冲区
   @size: 待读取的数据长度, 取值范围为1 ~ 16 */
int eeprom_page_read(int ee_addr, u8 pos, u8 *data, int size);

/* 自eeprom的pos位置开始写入数据
   @pos: eeprom的byte_address, 取值范围为0 ~ 255
   @data: 待写入的数据
   @size: 待写入数据的长度, 取值范围为1 ~ 16 */
int eeprom_page_write(int ee_addr, u8 pos, u8 *data, int size);


#endif
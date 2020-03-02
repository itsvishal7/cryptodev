#include <crypter.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <sys/mman.h>

#define SET_KEY___ _IOW(248,101,uint32_t*)
#define SET_CONFIG _IOW(248,102,uint8_t*)
#define SET_ENCRYP _IOW(248,103,uint8_t*)
#define SET_MMAP__ _IOW(248,104,uint8_t*)

#define BUFFERSIZE 32768

/*Function template to create handle for the CryptoCard device.
On success it returns the device handle as an integer*/
DEV_HANDLE create_handle()
{
	DEV_HANDLE cdev = open("/dev/cryptocard_mod",O_RDWR);
	if (cdev < 0)
		return ERROR;
	return cdev;
}

/*Function template to close device handle.
Takes an already opened device handle as an arguments*/
void close_handle(DEV_HANDLE cdev)
{
	if (close(cdev)) {
		printf("close_handle: error while closing\n");
		return;
	}
}

/*Function template to encrypt a message using MMIO/DMA/Memory-mapped.
Takes four arguments
  cdev: opened device handle
  addr: data address on which encryption has to be performed
  length: size of data to be encrypt
  isMapped: TRUE if addr is memory-mapped address otherwise FALSE
*/
int encrypt(DEV_HANDLE cdev, ADDR_PTR addr, uint64_t length, uint8_t isMapped)
{
	uint8_t value;
	value = 50;
	if (ioctl(cdev, SET_ENCRYP, &value))
                return ERROR;
	if (isMapped) {
		value = 70;
	}else {
		value = 80;
	}
	if (ioctl(cdev, SET_MMAP__, &value))
		return ERROR;
	ADDR_PTR copy = addr;
	uint64_t len = length;
	uint64_t count = 0;
	do {
		count =  length > BUFFERSIZE ? BUFFERSIZE : length;
		length = length > BUFFERSIZE ? length-BUFFERSIZE: 0;
		if (write(cdev, addr, count)) {
			return ERROR;
		}
		addr += count;
	} while(length>0);
	return length;
}

/*Function template to decrypt a message using MMIO/DMA/Memory-mapped.
Takes four arguments
  cdev: opened device handle
  addr: data address on which decryption has to be performed
  length: size of data to be decrypt
  isMapped: TRUE if addr is memory-mapped address otherwise FALSE
*/
int decrypt(DEV_HANDLE cdev, ADDR_PTR addr, uint64_t length, uint8_t isMapped)
{
	uint8_t value;
        value = 60;
        if (ioctl(cdev, SET_ENCRYP, &value))
                return ERROR;
	if (isMapped) { 
                value = 70;
        }else {
                value = 80;
        }
        if (ioctl(cdev, SET_MMAP__, &value))
                return ERROR;

	uint64_t count = 0;
        do {    
                count =  length > BUFFERSIZE ? BUFFERSIZE : length;
                length = length > BUFFERSIZE ? length-BUFFERSIZE: 0;
                if (write(cdev, addr, count)) {
                        return ERROR;
                }
                addr += count;
        } while(length>0);
        return length;
}

/*Function template to set the key pair.
Takes three arguments
  cdev: opened device handle
  a: value of key component a
  b: value of key component b
Return 0 in case of key is set successfully*/
int set_key(DEV_HANDLE cdev, KEY_COMP a, KEY_COMP b)
{
	uint32_t key = a;
	key = key<<8;
	key = key | b;
	if (ioctl(cdev, SET_KEY___, &key))
		return ERROR;
	return 0;
}

/*Function template to set configuration of the device to operate.
Takes three arguments
  cdev: opened device handle
  type: type of configuration, i.e. set/unset DMA operation, interrupt
  value: SET/UNSET to enable or disable configuration as described in type
Return 0 in case of key is set successfully*/
int set_config(DEV_HANDLE cdev, config_t type, uint8_t value)
{	
	if (type == DMA && value == SET) {
		value = 40;
	}else if (type == DMA && value == UNSET) {
		value = 30;
	}else if (type ==  INTERRUPT && value == SET) {
		value = 10;
	}else {
		value = 20;
	}
	if (ioctl(cdev, SET_CONFIG, &value))
		return ERROR;
  return 0;
}

/*Function template to device input/output memory into user space.
Takes three arguments
  cdev: opened device handle
  size: amount of memory-mapped into user-space (not more than 1MB strict check)
Return virtual address of the mapped memory*/
ADDR_PTR map_card(DEV_HANDLE cdev, uint64_t size)
{	
	if (size > 1024*1024) {
		printf("map_card: size exceeds capacity\n");
		return NULL;
	}
	ADDR_PTR addr;
	addr = mmap(NULL, 1024*1024, PROT_READ | PROT_WRITE, MAP_SHARED, cdev, 0);
	if (!addr) {
		printf("map_card: mmap failed!!!!\n");
                return NULL;
	}
  return addr + 0xA8;
}

/*Function template to device input/output memory into user space.
Takes three arguments
  cdev: opened device handle
  addr: memory-mapped address to unmap from user-space*/
void unmap_card(DEV_HANDLE cdev, ADDR_PTR addr)
{
	if (munmap(addr - 0xA8, 1024*1024)) {
		printf("unmap_card: munmap failed!!!!\n");
	}	
}

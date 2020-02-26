#include <linux/module.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>

#define INTERRUPT 10
#define NOINTERRUPT 20
#define MMIO 30
#define DMA 40
#define ENCRYPT 50
#define DECRYPT 60

#define SET_KEY___ _IOW(248,101,uint32_t*)
#define SET_CONFIG _IOW(248,102,uint8_t*)
#define SET_ENCRYP _IOW(248,103,uint8_t*)

#define ui unsigned int
#define ul unsigned long

#define MAXDEVICE 1

#define PCI_CryptoCard_DRIVER "cryptocard_mod"
#define PCI_CryptoCard_VENDOR 0x1234
#define PCI_CryptoCard_DEVICE 0xdeba

struct job {
	bool isE;          // isEncrypt or Decrypt
	bool isM;          // isMMIO or DMA
	bool isI;          // isInterruptable or Not
	ui   len;          // length of the message
	u32  keyab;
	char *msg;
};

struct mem_region {
	u32 start;
	u32 end;
};

struct config_mem {
	void __iomem *start;
	u8 id;                 /* read only */
	u8 live;
	u8 keys;               /* write only */
	// mmio //
	u8 len_msg;
	struct mem_region r5;
	//// registers //
	u8 status;
	u8 interrupt_status;   /* read only */
	struct mem_region r4;
	u8 interrupt_raise;    /* write only */
	u8 interrupt_ack;      /* write only */
	struct mem_region r3;
	//// registers end //
	u8 data_addr;
	//// mmio end//
	struct mem_region r2;
	// dma //
	u8 addr_op;
	u8 dlen_msg;
	//// registers //
	u8 cmd;
	//// registers end //
	// dma end //
	struct mem_region r1;
	ul size;
};

struct cryptodev_data {
	struct cdev *cdev;
	struct config_mem *configspace;
};


static dev_t devno;
struct cryptodev_data mycryptodev;

static bool is_not_live(void __iomem *live, u32 value)
{
	iowrite32(value, live);
        if (ioread32(live) & value) {
                return true;
        }
	return false;
}

static irqreturn_t pci_irq_handle(int irq, void *devid) 
{
	u32 status;
	struct config_mem *space;
	pr_alert("pci_irq_handle: called\n");
	if (!mycryptodev.configspace) {
		return -EBUSY;
	}
	space = mycryptodev.configspace;
	status = ioread32(space->start+space->interrupt_status);
	if (status == 0x001) {
		pr_alert("pci_irq_handle: MMIO\n");
	} else if (status == 0x100) {
		pr_alert("pci_irq_handle: DMA\n");
	}else {
		iowrite32(status,  space->start+space->interrupt_ack);
		return -EINVAL;
	}
	iowrite32(status,  space->start+space->interrupt_ack);
	return IRQ_HANDLED;
}

static long pci_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{	
	struct job *myjob;
	uint8_t myval;
	pr_alert("pci_unlocked_ioctl: called!!!\n");

	if (!file->private_data)
		return -ENOMEM;  /* struct job is not present*/
	myjob = (struct job *) file->private_data;

	switch (cmd) {
	case SET_KEY___:
		pr_alert("set_key: called!!!\n");
		if (copy_from_user(&myjob->keyab, (void *)arg, 4)) {
			return -EAGAIN;
		}
		pr_alert("set_key: success!!!\n");
		break;
	case SET_CONFIG:
		pr_alert("set_config: called!!!\n");
		if (copy_from_user(&myval, (void *)arg, 1)) {
                        return -EAGAIN;
                }	
		if (myval == DMA) { 
			myjob->isM = false;
		} else if (myval == MMIO) {
			myjob->isM = true;
		} else if (myval == INTERRUPT) {
                        myjob->isI = true;
                } else if (myval == NOINTERRUPT) {
                        myjob->isI = false;
                }else {
			return -EINVAL;
		}
		pr_alert("set_config: success!!!\n");
		break;
	case SET_ENCRYP:
		pr_alert("set_encrypt: called!!!\n");
                if (copy_from_user(&myval, (void *)arg, 1)) {
                        return -EAGAIN;
                }
                if (myval == ENCRYPT) {
                        myjob->isE = true;
                } else if (myval == DECRYPT) {
                        myjob->isE = false;
                }else {
                        return -EINVAL;
                }
                pr_alert("set_encrypt: success!!!\n");
                break;
	default:
		return -EINVAL; 
	}
	pr_alert("pci_unlocked_ioctl: success!!!\n");
        return 0;
}

static ssize_t pci_write(struct file *file, const char __user *ptr, size_t len, loff_t *offset)
{
        struct job *myjob;
	struct config_mem *space;

	if (!file->private_data || !mycryptodev.configspace)
                return -ENOMEM;  /* struct job is not present*/
        myjob = (struct job *) file->private_data;
	space = mycryptodev.configspace;
	
	pr_alert("pci_write: write called!!!\n");
	if (is_not_live(space->start+space->live, 0x0fffffff))
		return -ENODEV;
        pr_alert("pci_write: liveliness check passed!!!\n");
	
	myjob->msg = (char *) kzalloc(len, GFP_KERNEL);	
	if (!myjob->msg)
		return -ENOMEM;
	myjob->len = len;
	if (copy_from_user(myjob->msg, (void *)ptr, myjob->len+1)) //remove 1
		goto out_free;
	if (myjob->isM) {
		pr_alert("pci_write: MMIO operation!!!\n");
		if (myjob->isI) {
			pr_alert("pci_write: With Interrupt!!!\n");
			if (myjob->isE) {
                                pr_alert("pci_write: Encrypt!!!\n");
                                iowrite32(0x80 | 0x00, space->start+space->status);
                        } else {
                                pr_alert("pci_write: Decrypt!!!\n");
                                iowrite32(0x80 | 0x02, space->start+space->status);
                        }
			iowrite32(myjob->keyab, space->start+space->keys);
                        iowrite32(myjob->len, space->start+space->len_msg);
			memcpy_toio(space->start+space->r1.start, (void *) myjob->msg, myjob->len);
			pr_alert("pci_write: Driver should put data address register and go to sleep!!\n");
		/*tmp*/ pr_alert("pci_write: status: %x\n", ioread32(space->start+space->status));
                /*tmp*/ pr_alert("pci_write: len_msg: %x\n", ioread32(space->start+space->len_msg));
                        iowrite32(space->r1.start, space->start+space->data_addr);
		} else {
			pr_alert("pci_write: Without Interrupt!!!\n");
			if (myjob->isE) {
				pr_alert("pci_write: Encrypt!!!\n");
				iowrite32(0x00 | 0x00, space->start+space->status);
			} else {
				pr_alert("pci_write: Decrypt!!!\n");
				iowrite32(0x00 | 0x02, space->start+space->status);
			}
			iowrite32(myjob->keyab, space->start+space->keys);
                        iowrite32(myjob->len, space->start+space->len_msg);
                        memcpy_toio(space->start+space->r1.start, (void *) myjob->msg, myjob->len);
                /*tmp*/ pr_alert("pci_write: status: %x\n", ioread32(space->start+space->status));
                /*tmp*/ pr_alert("pci_write: len_msg: %x\n", ioread32(space->start+space->len_msg));
                        iowrite32(space->r1.start, space->start+space->data_addr);
                        while(ioread8(space->start+space->status) & 0x01) {
                                pr_alert("pci_write: looping!!!\n");
                        }
                        memcpy_fromio((void *)myjob->msg, space->start+space->r1.start, myjob->len);
                        if (copy_to_user((void *)ptr, myjob->msg, myjob->len))
                                goto out_free;
		}
	}else {
		pr_alert("pci_write: DMA operation!!!\n");
	}
	kfree(myjob->msg);
	myjob->msg = NULL;
	return 0;
out_free:
	kfree(myjob->msg);
	myjob->msg = NULL;
	return -EAGAIN;
}

static int pci_open(struct inode *inode, struct file *file)
{	
	struct job *myjob;
	pr_alert("pci_open: called!!!\n");
	myjob = kzalloc(sizeof(struct job), GFP_KERNEL);
	if (!myjob) { 
		return -ENOMEM;
	}
	myjob->isE = true;
	myjob->isM = true;
	myjob->isI = false;
	myjob->keyab = 0;
	file->private_data = myjob;
	pr_alert("pci_open: success!!!\n");
	return 0;
}

static int pci_release (struct inode *inode, struct file *file)
{	
	pr_info("pci_release: called!!!\n");
	kfree(file->private_data);
	file->private_data = NULL;
	pr_info("pci_release: success!!!\n");
        return 0;
}

static void setup_configspace(struct config_mem *configspace, ul size) {
	configspace->id = 0x0;
	configspace->live = 0x4;
	configspace->keys = 0x8;
	configspace->len_msg = 0xC;
	configspace->status = 0x20;
	configspace->interrupt_status = 0x24;
	configspace->interrupt_raise = 0x60;
        configspace->interrupt_ack = 0x64;
	configspace->data_addr = 0x80;
	configspace->addr_op = 0x90;
	configspace->dlen_msg = 0x98;
	configspace->cmd = 0xA0;
	configspace->size = size;

	configspace->r5.start = 0x10;
	configspace->r5.end   = 0x1F;
	configspace->r4.start = 0x28;
        configspace->r4.end   = 0x5F;
	configspace->r3.start = 0x68;
        configspace->r3.end   = 0x7F;
	configspace->r2.start = 0x88;
        configspace->r2.end   = 0x8F;
	configspace->r1.start = 0xA8;
        configspace->r1.end   = 0xFFFFF;
}

const struct file_operations fops = {
	.owner          = THIS_MODULE,
	.open           = pci_open,
//	.read           = pci_read,
	.unlocked_ioctl = pci_unlocked_ioctl,
	.write          = pci_write,
	.release        = pci_release 
};

static int cryptocard_probe(struct pci_dev *dev, const struct pci_device_id *id)
{	
	struct config_mem *space;
	struct cdev *cdev;
	pr_alert("cryptocard_probe: probe method called!!!\n");
	
	space = kzalloc(sizeof(struct config_mem), GFP_KERNEL);
	if (!space)
		return -ENOMEM;	
	cdev = kzalloc(sizeof(struct cdev), GFP_KERNEL);
        if (!cdev) {
		kfree(space);
                return -ENOMEM;
	}

	if (pci_enable_device(dev))
        	goto out_err;
	pr_alert("cryptocard_probe: enabled device!!!\n");
	if (pci_request_regions(dev, PCI_CryptoCard_DRIVER))
		goto out_disable;
	pr_alert("cryptocard_probe: requested regions!!!\n");
	space->start = pci_ioremap_bar(dev, 0);
	if (!space->start)
                goto out_unrequest;
	setup_configspace(space, pci_resource_len(dev, 0));
	pr_alert("cryptocard_probe: configspace start: %lx, size: %lx\n", (ul)space->start, space->size);
	if (ioread32(space->start+space->id) != 0x010C5730) {
		pr_alert("cryptocard_probe: id match failed!!!\n"); 
		goto out_iounmap;
	}
	pr_alert("cryptocard_probe: id matched!!!\n");

	if (is_not_live(space->start+space->live, 0x0fffffff)) {
		pr_alert("cryptocard_probe: liveliness check failed!!!\n");                              
                goto out_iounmap;
	}
	pr_alert("cryptocard_probe: liveliness check passed!!!\n");

	if (request_irq(dev->irq, pci_irq_handle, IRQF_SHARED, PCI_CryptoCard_DRIVER, dev)) {
		pr_alert("cryptocard_probe: request irq failed!!!\n");
		goto out_iounmap;
	}
	pr_alert("cryptocard_probe: irq#%u\n", dev->irq);

	if (alloc_chrdev_region(&devno, 0, MAXDEVICE, PCI_CryptoCard_DRIVER)) {
		pr_alert("cryptocard_probe: char device allocation failed!!!\n");
                goto out_irqfree;
	}
        pr_alert("cryptocard_probe: char device region allocated, major number: %d \n", MAJOR(devno));
	
	mycryptodev.cdev = cdev;
        mycryptodev.configspace = space;	

        cdev_init(cdev, &fops);
        if (cdev_add(cdev, devno, 1))
		goto out_chrdev_unreg_region;
	pr_alert("cryptocard_probe: char device added!!!\n");

	return 0;

out_chrdev_unreg_region:
	pr_alert("cryptocard_probe: out_chrdev_unreg_region: cdev_add failed!!!\n");
	unregister_chrdev_region(devno, MAXDEVICE);
out_irqfree:
	pr_alert("cryptocard_probe: out_irqfree: something failed after request irq!!!\n");
	free_irq(dev->irq, dev);
out_iounmap:
	pr_alert("cryptocard_probe: out_iounmap: something failed after iomapping !!!\n");
	iounmap(space->start);
out_unrequest:
	pr_alert("cryptocard_probe: out_unrequest: pci_ioremap_bar failed!!!\n");
	pci_release_regions(dev);
out_disable:
	pr_alert("cryptocard_probe: out_disable: pci_request_regions failed!!!\n");
	pci_disable_device(dev);
out_err:
	pr_alert("cryptocard_probe: out_err: char device alloc failed!!!\n");
	kfree(cdev);
	kfree(space);
	return -ENODEV;
}

static void cryptocard_remove(struct pci_dev *dev)
{
	pr_alert("cryptocard_remove: remove method called!!!\n");
	cdev_del(mycryptodev.cdev);
        unregister_chrdev_region(devno, MAXDEVICE);
	free_irq(dev->irq, dev);
	iounmap(mycryptodev.configspace->start);
	pci_release_regions(dev);
	pci_disable_device(dev);
	kfree(mycryptodev.cdev);
        kfree(mycryptodev.configspace);
}

/**
 * PCI_DEVICE(vend, dev) - macro used to describe a specific PCI device
 * @vend: the 16 bit PCI Vendor ID
 * @dev: the 16 bit PCI Device ID
 *
 * This macro is used to create a struct pci_device_id that matches a
 * specific device.  The subvendor and subdevice fields will be set to
 * PCI_ANY_ID.
 */
static struct pci_device_id cryptocard_ids[] = {
	{ PCI_DEVICE(PCI_CryptoCard_VENDOR, PCI_CryptoCard_DEVICE) },
	{ 0, },
};

static struct pci_driver cryptocard_mod = {
	.name     = PCI_CryptoCard_DRIVER,
	.id_table = cryptocard_ids,
	.probe    = cryptocard_probe,
	.remove   = cryptocard_remove 
};

/* Register our driver in kernel pci framework */
/**
 * module_pci_driver(__pci_driver) - Helper macro for registering a PCI driver
 * @__pci_driver: pci_driver struct
 *
 * Helper macro for PCI drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
module_pci_driver(cryptocard_mod);

/**
* MODULE_DEVICE_TABLE(pci,...) - Helper macro for exporting table of device iD's
* @pci: type
*/
MODULE_DEVICE_TABLE(pci, cryptocard_ids);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CryptoCard Device Driver module");
MODULE_AUTHOR("Vishal Chourasia");
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>

#define INTERRUPT 10

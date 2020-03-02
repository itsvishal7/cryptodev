#include <linux/module.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>

#define INTERRUPT 10
#define NOINTERRUPT 20
#define MMIO 30
#define DMA 40
#define ENCRYPT 50
#define DECRYPT 60
#define MMAP 70
#define UNMMAP 80

#define BUFFERSIZE 32768

#define SET_KEY___ _IOW(248,101,uint32_t*)
#define SET_CONFIG _IOW(248,102,uint8_t*)
#define SET_ENCRYP _IOW(248,103,uint8_t*)
#define SET_MMAP__ _IOW(248,104,uint8_t*)

#define ui unsigned int
#define ul unsigned long

#define MAXDEVICE 1

#define PCI_CryptoCard_DRIVER "cryptocard_mod"
#define PCI_CryptoCard_VENDOR 0x1234
#define PCI_CryptoCard_DEVICE 0xdeba
#define PCI_CryptoCard_IDFIER 0x010C5730

struct job {
	bool isE;          // isEncrypt or Decrypt
	bool isM;          // isMMIO or DMA
	bool isI;          // isInterruptable or Not
	bool isNotMM;      // isMemoryMapped or Not
	ui   len;          // length of the buffer
	void *msg;	   // buffer
	u32  keyab;
};

struct mem_region {
	void __iomem *start;
	void __iomem *end;
};

struct config_mem {
	void __iomem *start;
	void __iomem *id;                 /* read only */
	void __iomem *live;
	void __iomem *keys;               /* write only */
	// mmio //
	void __iomem *len_msg;
	struct mem_region r5;
	//// registers //
	void __iomem *status;
	void __iomem *interrupt_status;   /* read only */
	struct mem_region r4;
	void __iomem *interrupt_raise;    /* write only */
	void __iomem *interrupt_ack;      /* write only */
	struct mem_region r3;
	//// registers end //
	void __iomem *data_addr;
	//// mmio end//
	struct mem_region r2;
	// dma //
	void __iomem *addr_op;
	void __iomem *dlen_msg;
	//// registers //
	void __iomem *cmd;
	//// registers end //
	// dma end //
	struct mem_region r1;
	ul size;
};

struct cryptodev_data {
	struct cdev *cdev;
	struct pci_dev *pdev;
	struct config_mem *configspace;
	struct semaphore wsem;
	wait_queue_head_t wq;
};


/** Device specfic data */
static dev_t devno;
struct cryptodev_data mycryptodev;

/** setup functions */
static bool is_not_live(void __iomem *live, u32 value);
static void setup_configspace(struct config_mem *configspace, ul size);

/** char dev file operations function */ 
static int pci_mmap (struct file *file, struct vm_area_struct *vmas);
static irqreturn_t pci_irq_handle(int irq, void *devid);
static long pci_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t pci_write(struct file *file, const char __user *ptr, size_t len, loff_t *offset);
static int pci_open(struct inode *inode, struct file *file);
static int pci_release (struct inode *inode, struct file *file);

const struct file_operations fops = {
	.owner          = THIS_MODULE,
	.open           = pci_open,
	.unlocked_ioctl = pci_unlocked_ioctl,
	.write          = pci_write,
	.release        = pci_release,
	.mmap		= pci_mmap
};

static int cryptocard_probe(struct pci_dev *dev, const struct pci_device_id *id)
{	
	struct config_mem *space;
	struct cdev *cdev;

	//pr_alert("cryptocard_probe: 1) Set up device...started\n");
	
	space = kzalloc(sizeof(struct config_mem), GFP_KERNEL);
	if (unlikely(!space))
		return -ENOMEM;	
	cdev = kzalloc(sizeof(struct cdev), GFP_KERNEL);
        if (unlikely(!cdev)) {
		kfree(space);
                return -ENOMEM;
	}

	if (unlikely(pci_enable_device(dev)))
        	goto out_err;
	//pr_alert("cryptocard_probe: 2) Enabled device\n");
	if (unlikely(pci_request_regions(dev, PCI_CryptoCard_DRIVER)))
		goto out_disable;
	//pr_alert("cryptocard_probe: 3) Requested regions\n");
	space->start = pci_ioremap_bar(dev, 0);
	if (unlikely(!space->start))
                goto out_unrequest;
	setup_configspace(space, pci_resource_len(dev, 0));
	if (unlikely(ioread32(space->id) != PCI_CryptoCard_IDFIER)) {
		//pr_alert("cryptocard_probe: error: id match failed\n"); 
		goto out_iounmap;
	}
	//pr_alert("cryptocard_probe: 4) ID matched!!!\n");

	if (unlikely(is_not_live(space->live, 0x0fffffff))) {
		//pr_alert("cryptocard_probe: error: device not live\n");                              
                goto out_iounmap;
	}
	//pr_alert("cryptocard_probe: 5) Device is live.\n");
	
	if (!dma_set_mask(&dev->dev, DMA_BIT_MASK(32))) {
		//pr_alert("cryptocard_probe: 6) dma_set_mask successfull!!!\n");
	}else {
		//pr_alert("cryptocard_probe: @error: dma_set_mask failed!!!\n");
	}

	if (unlikely(request_irq(dev->irq, pci_irq_handle, IRQF_SHARED, PCI_CryptoCard_DRIVER, dev))) {
		//pr_alert("cryptocard_probe: error: request irq failed!!!\n");
		goto out_iounmap;
	}
	//pr_alert("cryptocard_probe: 7) IRQ at: %u\n", dev->irq);

	if (unlikely(alloc_chrdev_region(&devno, 0, MAXDEVICE, PCI_CryptoCard_DRIVER))) {
		//pr_alert("cryptocard_probe: error: char device allocation failed!!!\n");
                goto out_irqfree;
	}
        //pr_alert("cryptocard_probe: 8) char device region allocated, major number: %d, minor #: %d \n", MAJOR(devno), MINOR(devno));
	
	mycryptodev.cdev = cdev;
        mycryptodev.configspace = space;	
	mycryptodev.pdev = dev;	

	init_waitqueue_head(&mycryptodev.wq);
	sema_init(&mycryptodev.wsem, 1);

        cdev_init(cdev, &fops);
        if (unlikely(cdev_add(cdev, devno, 1)))
		goto out_chrdev_unreg_region;
	//pr_alert("cryptocard_probe: 9) Char device added!!!\n");

	return 0;

out_chrdev_unreg_region:
	//pr_alert("cryptocard_probe: out_chrdev_unreg_region: cdev_add failed!!!\n");
	mycryptodev.cdev = NULL;
        mycryptodev.configspace = NULL;
	mycryptodev.pdev = NULL;
	unregister_chrdev_region(devno, MAXDEVICE);
out_irqfree:
	//pr_alert("cryptocard_probe: out_irqfree: something failed after request irq!!!\n");
	free_irq(dev->irq, dev);
out_iounmap:
	//pr_alert("cryptocard_probe: out_iounmap: something failed after iomapping !!!\n");
	iounmap(space->start);
out_unrequest:
	//pr_alert("cryptocard_probe: out_unrequest: pci_ioremap_bar failed!!!\n");
	pci_release_regions(dev);
out_disable:
	//pr_alert("cryptocard_probe: out_disable: pci_request_regions failed!!!\n");
	pci_disable_device(dev);
out_err:
	//pr_alert("cryptocard_probe: out_err: char device alloc failed!!!\n");
	kfree(cdev);
	kfree(space);
	return -ENODEV;
}

static void cryptocard_remove(struct pci_dev *dev)
{
        //pr_alert("cryptocard_remove: called!!!\n");
        cdev_del(mycryptodev.cdev);
        unregister_chrdev_region(devno, MAXDEVICE);
        free_irq(dev->irq, dev);
        iounmap(mycryptodev.configspace->start);
        pci_release_regions(dev);
        pci_disable_device(dev);
        kfree(mycryptodev.cdev);
        kfree(mycryptodev.configspace);
	mycryptodev.configspace = NULL;
        mycryptodev.cdev = NULL;
        mycryptodev.pdev = NULL;
	//pr_alert("cryptocard_remove: success!!!\n");
}

static int pci_mmap (struct file *file, struct vm_area_struct *vmas)
{
	if (!mycryptodev.pdev) 
		return -EBUSY;
	vmas->vm_page_prot = pgprot_device(vmas->vm_page_prot);
	vmas->vm_pgoff += (pci_resource_start(mycryptodev.pdev, 0) >> PAGE_SHIFT);
	if (unlikely(io_remap_pfn_range(vmas, vmas->vm_start, vmas->vm_pgoff, vmas->vm_end - vmas->vm_start, vmas->vm_page_prot))) {
		//pr_alert("pci_mmap: remap_pfn_range failed!!!\n");
		return -EAGAIN;
	}
	//pr_alert("pci_mmap: remap_pfn_range mapped!!!\n");
	return 0;
}

static irqreturn_t pci_irq_handle(int irq, void *devid) 
{
	u32 status;
	struct config_mem *space;
	struct pci_dev *dev = (struct pci_dev *) devid;
	//pr_alert("pci_irq_handle: called\n");
	if (unlikely(!dev || dev->device != PCI_CryptoCard_DEVICE || !mycryptodev.configspace)) {
		return -EBUSY;
	}
	space = mycryptodev.configspace;
	status = ioread32(space->interrupt_status);
	if (status == 0x001) {
		//pr_alert("pci_irq_handle: MMIO\n");
		wake_up_interruptible(&mycryptodev.wq);
	} else if (status == 0x100) {
		//pr_alert("pci_irq_handle: DMA\n");
		wake_up_interruptible(&mycryptodev.wq);
	} else {
		iowrite32(status,  space->interrupt_ack);
		return -EINVAL;
	}
	iowrite32(status,  space->interrupt_ack);
	//pr_alert("pci_irq_handle: handled!!\n");
	return IRQ_HANDLED;
}

static long pci_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{	
	struct job *myjob;
	uint8_t myval;

	if (unlikely(!file->private_data))
		return -ENOMEM;  /* struct job is not present*/
	myjob = (struct job *) file->private_data;

	switch (cmd) {
	case SET_KEY___:
		//pr_alert("set_key: called!!!\n");
		if (unlikely(copy_from_user(&myjob->keyab, (void *)arg, 4))) {
			return -EAGAIN;
		}
		//pr_alert("set_key: success!!!\n");
		break;
	case SET_CONFIG:
		//pr_alert("set_config: called!!!\n");
		if (unlikely(copy_from_user(&myval, (void *)arg, 1))) {
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
		//pr_alert("set_config: success!!!\n");
		break;
	case SET_ENCRYP:
		//pr_alert("set_encrypt: called!!!\n");
                if (unlikely(copy_from_user(&myval, (void *)arg, 1))) {
                        return -EAGAIN;
                }
                if (myval == ENCRYPT) {
                        myjob->isE = true;
                } else if (myval == DECRYPT) {
                        myjob->isE = false;
                }else {
                        return -EINVAL;
                }
                //pr_alert("set_encrypt: success!!!\n");
                break;
	case SET_MMAP__:
		//pr_alert("set_mmap__: called!!!\n");
                if (unlikely(copy_from_user(&myval, (void *)arg, 1))) {
                        return -EAGAIN;
                }
                if (myval == MMAP) {
                        myjob->isNotMM = false;
                } else if (myval == UNMMAP) {
                        myjob->isNotMM = true;
                }else {
                        return -EINVAL;
                }
                //pr_alert("set_mmap__: success!!!\n");
                break;
	default:
		return -EINVAL; 
	}
        return 0;
}

static ssize_t pci_write(struct file *file, const char __user *ptr, size_t len, loff_t *offset)
{
        struct job *myjob;
	struct config_mem *space;
	ssize_t err;
	dma_addr_t dma_handle;
	err = -1;

	if (unlikely(!file->private_data || !mycryptodev.configspace))
                return -ENOMEM;  /* struct job is not present*/
        myjob = (struct job *) file->private_data;
	space = mycryptodev.configspace;
	
	if (unlikely(!myjob->isNotMM && !myjob->isM)) {
		//pr_alert("pci_write: Trying to DMA with MMAPED pointer. NOT ALLOWED!!\n");
		return -EINVAL;
	}
	
	//pr_alert("pci_write: write called!!!\n");
	if (unlikely(is_not_live(space->live, 0x0fffffff))) {
		//pr_alert("pci_write: device is not live\n");
		return -ENODEV;
	}
        //pr_alert("pci_write: device is live!!!\n");
	
	myjob->len = len;
	if (myjob->isNotMM) {
		//pr_alert("pci_wrtie: isNotMM: True\n");
		myjob->msg = vmalloc(len);                                                             /* Only if isNotMM is true */
		if (unlikely(!myjob->msg))
                	return -ENOMEM;	

		if (unlikely(copy_from_user(myjob->msg, (void *)ptr, myjob->len))) {
			err = -EAGAIN;
			goto out_free;
		}
	}

	if (down_interruptible(&mycryptodev.wsem)) {
		err = -ERESTARTSYS;
		goto out_free;
	}

	iowrite32(myjob->keyab, space->keys);								/* setting keys */
	if (myjob->isM) {										/* MMIO */
		//pr_alert("pci_write: MMIO operation!!!\n");
		iowrite32(myjob->len, space->len_msg);
		if (myjob->isNotMM) {
			memcpy_toio(space->r1.start, myjob->msg, myjob->len);
		}
		if (myjob->isI) {					
			//pr_alert("pci_write: With___ Interrupt!!!\n");
			if (myjob->isE) {
                                //pr_alert("pci_write: Encrypt!!!\n");
                                iowrite32(0x80 | 0x00, space->status);
                        } else {
                                //pr_alert("pci_write: Decrypt!!!\n");
                                iowrite32(0x80 | 0x02, space->status);
                        }
			writeq(space->r1.start - space->start, space->data_addr);
		        //pr_alert("pci_write: wait_event_interruptible: Just BEfore\n");
			if (unlikely(wait_event_interruptible(mycryptodev.wq, !(ioread8(space->status) & 0x01)))) {
				err = -ERESTARTSYS;
				goto out_up;
			}
			//pr_alert("pci_write: wait_event_interruptible: Just After\n");
		} else {
			//pr_alert("pci_write: Without Interrupt!!!\n");
			if (myjob->isE) {
				//pr_alert("pci_write: Encrypt!!!\n");
				iowrite32(0x00 | 0x00, space->status);
			} else {
				//pr_alert("pci_write: Decrypt!!!\n");
				iowrite32(0x00 | 0x02, space->status);
			}
                        writeq(space->r1.start - space->start, space->data_addr);
                        while(ioread8(space->status) & 0x01); 							/* busy waiting */
                	//pr_alert("pci_write: loop ended!!!\n");
		}
		if (myjob->isNotMM) {
			memcpy_fromio(myjob->msg, space->r1.start, myjob->len);
                	if (unlikely(copy_to_user((void *)ptr, myjob->msg, myjob->len))) {
				err = -EAGAIN;
                		goto out_up;
			}
		}
	}else {
		//pr_alert("pci_write: DMA operation!!!\n");
		
		dma_handle = dma_map_single(&mycryptodev.pdev->dev, myjob->msg, myjob->len, DMA_BIDIRECTIONAL);
		if (unlikely(dma_mapping_error(&mycryptodev.pdev->dev, dma_handle))) {
			//pr_alert("pci_write: dma_map_single failed!!!\n");
			goto out_up;
		}
		//pr_alert("pci_write: dma_handle: %lx\n", (unsigned long) dma_handle);
	
		writeq(dma_handle, space->addr_op);
		//pr_alert("pci_write: dma_data_address: %lx (check)\n", (unsigned long)readq(space->addr_op));
		
		writeq(myjob->len, space->dlen_msg);
                //pr_alert("pci_write: DMA length: %llu\n", readq(space->dlen_msg));
		
		if (myjob->isI) {
			//pr_alert("pci_write: With___ Interrupt!!!\n");
			if (myjob->isE) {
                                //pr_alert("pci_write: Encrypt!!!\n");
                                writeq(0x4 | 0x0 | 0x1, space->cmd);
                        } else {
                                //pr_alert("pci_write: Decrypt!!!\n");
                                writeq(0x4 | 0x2 | 0x1, space->cmd);
                        }
			//pr_alert("pci_write: wait_event_interruptible: Just BEfore\n");
			if (unlikely(wait_event_interruptible(mycryptodev.wq, !(readq(space->cmd) & 0x1)))) {
                                err = -ERESTARTSYS;
                                goto out_up;
                        }
                	//pr_alert("pci_write: wait_event_interruptible: Just After\n");
		} else {
			//pr_alert("pci_write: Without Interrupt!!!\n");	
			if (myjob->isE) {
				//pr_alert("pci_write: Encrypt!!!\n");	
				writeq(0x0 | 0x0 | 0x1, space->cmd);
			} else {
				//pr_alert("pci_write: Decrypt!!!\n");
				writeq(0x0 | 0x2 | 0x1, space->cmd);
			}
			//pr_alert("pci_write: DMA cmd: %llx(check)\n", readq(space->cmd));
                	while(readq(space->cmd) & 0x1);              			/* busy waiting */
			//pr_alert("pci_write: dma looping completes \n");
		}
		dma_unmap_single(&mycryptodev.pdev->dev, dma_handle, myjob->len, DMA_BIDIRECTIONAL);
		if (unlikely(copy_to_user((void *)ptr, myjob->msg, myjob->len))) {
                	err = -EAGAIN;
                	goto out_up;
                }
	}
	up(&mycryptodev.wsem);
	if (myjob->isNotMM) {
                vfree(myjob->msg);
        }
	myjob->msg = NULL;
	return 0;
out_up:
	up(&mycryptodev.wsem);
out_free:
	if (myjob->isNotMM) {
		vfree(myjob->msg);
	}
	myjob->msg = NULL;
	return err;
}

static int pci_open(struct inode *inode, struct file *file)
{	
	struct job *myjob;
	//pr_alert("pci_open: called!!!\n");
	myjob = kzalloc(sizeof(struct job), GFP_KERNEL);
	if (unlikely(!myjob)) { 
		return -ENOMEM;
	}
	myjob->isE = true;
	myjob->isM = true;
	myjob->isI = false;
	myjob->keyab = 0;
	file->private_data = myjob;
	//pr_alert("pci_open: success!!!\n");
	return 0;
}

static int pci_release (struct inode *inode, struct file *file)
{	
	//pr_info("pci_release: called!!!\n");
	kfree(file->private_data);
	file->private_data = NULL;
	//pr_info("pci_release: success!!!\n");
        return 0;
}

static bool is_not_live(void __iomem *live, u32 value)
{       
        iowrite32(value, live);
        if (ioread32(live) & value) {
                return true;
        }
        return false;
}

static void setup_configspace(struct config_mem *configspace, ul size) {
	void __iomem *start = configspace->start;
        configspace->id               = start + 0x0;
        configspace->live             = start + 0x4;
        configspace->keys             = start + 0x8;
        configspace->len_msg          = start + 0xC;
        configspace->status           = start + 0x20;
        configspace->interrupt_status = start + 0x24;
        configspace->interrupt_raise  = start + 0x60;
        configspace->interrupt_ack    = start + 0x64;
        configspace->data_addr        = start + 0x80;
        configspace->addr_op          = start + 0x90;
        configspace->dlen_msg         = start + 0x98;
        configspace->cmd              = start + 0xA0;
        configspace->size             = size;

        configspace->r5.start = start + 0x10;
        configspace->r5.end   = start + 0x1F;
        configspace->r4.start = start + 0x28;
        configspace->r4.end   = start + 0x5F;
        configspace->r3.start = start + 0x68;
        configspace->r3.end   = start + 0x7F;
        configspace->r2.start = start + 0x88;
        configspace->r2.end   = start + 0x8F;
        configspace->r1.start = start + 0xA8;
        configspace->r1.end   = start + 0xFFFFF;
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

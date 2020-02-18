#include <linux/module.h>
#include <linux/pci.h>
#include <linux/cdev.h>

#define ui unsigned int
#define ul unsigned long

#define MAXDEVICE 1

#define PCI_CryptoCard_DRIVER "cryptocard_mod"
#define PCI_CryptoCard_VENDOR 0x1234
#define PCI_CryptoCard_DEVICE 0xdeba

struct config_mem {
	void __iomem *id;
	void __iomem *live;
	void __iomem *keya;
	void __iomem *keyb;
	// mmio //
	void __iomem *len_msg;
	//// registers //
	void __iomem *status;
	void __iomem *interrupt_status;
	void __iomem *interrupt_raise;
	void __iomem *interrupt_ack;
	//// registers end //
	void __iomem *data_addr;
	//// mmio end//
	// dma //
	void __iomem *addr_op;
	void __iomem *dlen_msg;
	//// registers //
	void __iomem *cmd;
	//// registers end //
	// dma end //
	ul size;
};

struct cryptodev_data {
	struct cdev cdev;
};


static dev_t devno;
static struct config_mem *configspace;

struct cryptodev_data mycryptodev;


static int pci_open(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE))
		return -EAGAIN;
	
	pr_info("pci_open: device open success!!!\n");
	return 0;
}

static int pci_release (struct inode *inode, struct file *file)
{
	module_put(THIS_MODULE);
	pr_info("pci_release: device closed success!!\n");
        return 0;
}

static void setup_configspace(struct config_mem *configspace, void __iomem *start, ul size) {
	configspace->id = start;
	configspace->live = start + 4;
	configspace->keya = start + 10;
	configspace->keyb = start + 11;
	configspace->len_msg = start + 0xC;
	configspace->status = start + 0x20;
	configspace->interrupt_status = start + 0x24;
	configspace->interrupt_raise = start + 0x60;
        configspace->interrupt_ack = start + 0x64;
	configspace->data_addr = start + 0x80;
	configspace->addr_op = start + 0x90;
	configspace->dlen_msg = start + 0x98;
	configspace->cmd = start + 0xA0;
	configspace->size = size;
}

const struct file_operations fops = {
	.owner   = THIS_MODULE,
	.open    = pci_open,
//	.close   = NULL//pci_close;
//	.write   = NULL//pci_write;
	.release = pci_release 
};

static int cryptocard_probe(struct pci_dev *dev, const struct pci_device_id *id)
{	
	void __iomem *start;
	pr_alert("cryptocard_probe: probe method called!!!\n");
	
	configspace = (struct config_mem *)kzalloc(sizeof(struct config_mem), GFP_KERNEL);
	if (!configspace)
		return -ENOMEM;	

	if (pci_enable_device(dev))
        	goto out_err;
	pr_alert("cryptocard_probe: enabled device!!!\n");
	if (pci_request_regions(dev, PCI_CryptoCard_DRIVER))
		goto out_disable;
	pr_alert("cryptocard_probe: requested regions!!!\n");
	start = pci_ioremap_bar(dev, 0);
	if (!start)
                goto out_unrequest;
	setup_configspace(configspace, start, pci_resource_len(dev, 0));
	pr_alert("cryptocard_probe: config space start: %lx and size: %lx\n", (ul)configspace->id, configspace->size);
	if (ioread32(configspace->id) != 0x010C5730) {
		pr_alert("cryptocard_probe: id match failed!!!\n"); 
		goto out_iounmap;
	}
	pr_alert("cryptocard_probe: id matched!!!\n");

	iowrite32(0x0fffffff, configspace->live);
	if (ioread32(configspace->live) & 0x0fffffff) {
		pr_alert("cryptocard_probe: liveliness check failed!!!\n");                              
                goto out_iounmap;
	}
	pr_alert("cryptocard_probe: liveliness check passed!!!\n");

	if (alloc_chrdev_region(&devno, 0, MAXDEVICE, PCI_CryptoCard_DRIVER)) {
		pr_alert("cryptocard_probe: char device allocation failed!!!\n");
                goto out_iounmap;
	}
        pr_alert("cryptocard_probe: char device region allocated, major number: %d \n", MAJOR(devno));

        cdev_init(&mycryptodev.cdev, &fops);
        if (cdev_add(&mycryptodev.cdev, devno, 1))
		goto out_chrdev_unreg_region;
	pr_alert("cryptocard_probe: char device added!!!\n");
	
	return 0;

out_chrdev_unreg_region:
	pr_alert("cryptocard_probe: out_chrdev_unreg_region: cdev_add failed!!!\n");
	unregister_chrdev_region(devno, MAXDEVICE);
out_iounmap:
	pr_alert("cryptocard_probe: out_iounmap: something failed after iomapping !!!\n");
	iounmap(configspace->id);
out_unrequest:
	pr_alert("cryptocard_probe: out_unrequest: pci_ioremap_bar failed!!!\n");
	pci_release_regions(dev);
out_disable:
	pr_alert("cryptocard_probe: out_disable: pci_request_regions failed!!!\n");
	pci_disable_device(dev);
out_err:
	pr_alert("cryptocard_probe: out_err: char device alloc failed!!!\n");
	kfree(configspace);
	return -ENODEV;
}

static void cryptocard_remove(struct pci_dev *dev)
{
	pr_alert("cryptocard_remove: remove method called!!!\n");
	cdev_del(&mycryptodev.cdev);
        unregister_chrdev_region(devno, MAXDEVICE);
	iounmap(configspace->id);
	pci_release_regions(dev);
	pci_disable_device(dev);
	kfree(configspace);
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

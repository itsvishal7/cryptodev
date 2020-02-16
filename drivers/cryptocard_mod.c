#include <linux/module.h>
#include <linux/pci.h>

#define PCI_CryptoCard_DRIVER "cryptocard_mod"
#define PCI_CryptoCard_VENDOR 0x1234
#define PCI_CryptoCard_DEVICE 0xdeba

static int cryptocard_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	pr_alert("cryptocard_probe: probe method called!!!\n");
	
	if (pci_enable_device(dev))
        	goto out_err;
	pr_alert("cryptocard_probe: enabled device!!!\n");
	if (pci_request_regions(dev, PCI_CryptoCard_DRIVER))
		goto out_disable;
	pr_alert("cryptocard_probe: requested regions!!!\n");
	
	return 0;
out_disable:
	pr_alert("cryptocard_probe: out_disable: pci_request_regions failed!!!\n");
	pci_disable_device(dev);
out_err:
	pr_alert("cryptocard_probe: out_err: pci_enable_device failed!!!\n");
	return -ENODEV;
}

static void cryptocard_remove(struct pci_dev *dev)
{
	pr_alert("cryptocard_remove: remove method called!!!\n");
	pci_release_regions(dev);
	pci_disable_device(dev);
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

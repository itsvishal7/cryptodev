

#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "hw/hw.h"
#include "qemu/event_notifier.h"
#include <time.h>
#include "hw/qdev-properties.h"
#include "hw/pci/msi.h"

typedef struct PCIHelloDevState {
    PCIDevice parent_obj;

    /* for PIO */
    MemoryRegion io;
    /* for MMIO */
    MemoryRegion mmio;
    /* irq used */
    qemu_irq irq;
    /* dma buf size */
    unsigned int dma_size;
    /* buffer copied with the dma operation on RAM */
    char *dma_buf;
    /* did we throw an interrupt ? */
    int threw_irq;
    /* id of the device, writable */
    int id;
    int val;


} PCIHelloDevState;

#define TYPE_PCI_HELLO_DEV "pci-hellodev"
#define PCI_HELLO_DEV(obj)     OBJECT_CHECK(PCIHelloDevState, (obj), TYPE_PCI_HELLO_DEV)
/* sizes must be power of 2 in PCI */
#define HELLO_IO_SIZE 1<<4
#define HELLO_MMIO_SIZE 1<<6


static void hello_iowrite(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    PCIHelloDevState *d = (PCIHelloDevState *) opaque;
    PCIDevice *pci_dev = (PCIDevice *) opaque;

    printf("Write Ordered, addr=%x, value=%lu, size=%d\n", (unsigned) addr, value, size);

    switch (addr) {
        case 0:
            if (value) {
                /* throw an interrupt */
                printf("irq assert\n");
                d->threw_irq = 1;
                pci_irq_assert(pci_dev);

            } else {
                /*  ack interrupt */
                printf("irq deassert\n");
                pci_irq_deassert(pci_dev);
                d->threw_irq = 0;
            }
            break;
        default:
            printf("Io not used\n");
            
    }

}

static uint64_t hello_ioread(void *opaque, hwaddr addr, unsigned size)
{
    PCIHelloDevState *d = (PCIHelloDevState *) opaque;
    printf("Read Ordered, addr =%x, size=%d\n", (unsigned) addr, size);

    switch (addr) {
        case 0:
            /* irq status */
            return d->threw_irq;
            break;
        default:
            printf("Io not used\n");
            return 0x0;
            
    }
}

static uint64_t hello_mmioread(void *opaque, hwaddr addr, unsigned size)
{
    PCIHelloDevState *d = (PCIHelloDevState *) opaque;
    printf("MMIO Read Ordered, addr =%x, size=%d\n",(unsigned)  addr, size);

    switch (addr) {
        case 0:
            /* also irq status */   
            printf("irq_status %d\n", d->threw_irq);
            return d->threw_irq;
            break;
        case 4:
            /* Id of the device */
            printf("id %x sampvalue = %x\n", d->id, d->val);
            return d->id;
            break;
        default:
            printf("MMIO not used\n");
            return 0x0;
            
    }
}

static void hello_mmiowrite(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    PCIHelloDevState *d = (PCIHelloDevState *) opaque;
    printf("MMIO write Ordered, addr=%x, value=%lu, size=%d\n",(unsigned)  addr, value, size);

    switch (addr) {
        case 4:
            /* change the id */
            d->val = value;
            break;
        default:
            printf("MMIO not writable or not used\n");
            
    }
}



/*
 * Callbacks called when the Memory Region
 * representing the MMIO space is
 * accessed.
 */ 
static const MemoryRegionOps hello_mmio_ops = {
    .read = hello_mmioread,
    .write = hello_mmiowrite,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

/*
 * Callbacks called when the Memory Region
 * representing the PIO space is
 * accessed. 
 */ 
static const MemoryRegionOps hello_io_ops = {
    .read = hello_ioread,
    .write = hello_iowrite,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

/* Callbacks for MMIO and PIO regions are registered here */
static void hello_io_setup(PCIHelloDevState *d) 
{
    memory_region_init_io(&d->mmio, OBJECT(d), &hello_mmio_ops, d, "hello_mmio", HELLO_MMIO_SIZE);
    memory_region_init_io(&d->io, OBJECT(d), &hello_io_ops, d, "hello_io", HELLO_IO_SIZE);
}

/* When device is loaded */
static void pci_hellodev_realize(PCIDevice *pci_dev, Error **errp)
{
    /* init the internal state of the device */
    PCIHelloDevState *d = PCI_HELLO_DEV(pci_dev);
    printf("d=%lu\n", (unsigned long) &d);
    d->dma_size = 4096 * sizeof(char);
    d->dma_buf = malloc(d->dma_size);
    d->id = 0x1337;
    d->threw_irq = 0;

    uint8_t *pci_conf = pci_dev->config;

    pci_config_set_interrupt_pin(pci_conf, 1);

    if (msi_init(pci_dev, 0, 1, true, false, errp)) {
        return;
    }

    /* create the memory region representing the MMIO and PIO 
     * of the device
     */
    hello_io_setup(d);
    /*
     * See linux device driver (Edition 3) for the definition of a bar
     * in the PCI bus.
     */
    pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_IO, &d->io);
    pci_register_bar(pci_dev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY, &d->mmio);

    printf("Hello World loaded\n");
}

/* When device is unloaded 
 * Can be useful for hot(un)plugging
 */
static void pci_hellodev_uninit(PCIDevice *dev)
{
    PCIHelloDevState *d = (PCIHelloDevState *) dev;
    free(d->dma_buf);
    msi_uninit(dev);
    printf("Good bye World unloaded\n");
   
}
/*
static void qdev_pci_hellodev_reset(DeviceState *dev)
{
    printf("Reset World\n");
}
*/

static void pci_hellodev_class_init(ObjectClass *class, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(class);

    k->realize = pci_hellodev_realize;
    k->exit = pci_hellodev_uninit;
//    k->vendor_id = PCI_VENDOR_ID_QEMU;
    k->vendor_id = 0x1337;
    k->device_id = 0x0001;
    k->class_id = PCI_CLASS_OTHERS;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
    k->revision = 0x10;
    dc->desc = "PCI Hello World";    
}


static void pci_hellodev_register_types(void)
{
    static InterfaceInfo interfaces[] = {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    };
    static const TypeInfo hellodev_info = {
        .name          = TYPE_PCI_HELLO_DEV,
        .parent        = TYPE_PCI_DEVICE,
        .instance_size = sizeof(PCIHelloDevState),
        .class_init    = pci_hellodev_class_init,
        .interfaces = interfaces,
    };

    type_register_static(&hellodev_info);
}
type_init(pci_hellodev_register_types)

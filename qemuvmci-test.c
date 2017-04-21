#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/acpi.h>
#include <linux/kexec.h>

MODULE_AUTHOR("Marc-André Lureau <marcandre.lureau@redhat.com>");
MODULE_DESCRIPTION("QEMU vmcoreinfo driver");
MODULE_LICENSE("GPL");

static int qemuvmci_add(struct acpi_device *device);
static int qemuvmci_remove(struct acpi_device *device);

static u64 qemuvmci_addr;

static const struct acpi_device_id qemuvmci_device_ids[] = {
    { "QEMUVMCI", 0}, /* QEMU */
    { "", 0},
};
MODULE_DEVICE_TABLE(acpi, qemuvmci_device_ids);

static struct acpi_driver qemuvmci_driver = {
    .name  = "QEMU vmcoreinfo",
    .ids   = qemuvmci_device_ids,
    .ops   = {
        .add    = qemuvmci_add,
        .remove = qemuvmci_remove,
    },
    .owner = THIS_MODULE,
};

static int write_vmcoreinfo(void)
{
    u8 *dest;

    if (qemuvmci_addr == 0) {
        printk(KERN_ERR "ADDR not initialized\n");
        return -EINVAL;
    }

    dest = ioremap_nocache(qemuvmci_addr, 16);
    if (!dest) {
        printk(KERN_ERR "Unable to map memory for access\n");
        return -EINVAL;
    }

    writel(0, dest); /* version */
    writeq(paddr_vmcoreinfo_note(), dest + 4);
    writel(sizeof(vmcoreinfo_note), dest + 12);
    iounmap(dest);

    return 0;
}

static int qemuvmci_add(struct acpi_device *device)
{
    acpi_status result;
    struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
    union acpi_object *obj;

    if (acpi_has_method(device->handle, "ADDR")) {
        printk(KERN_INFO "QEMUVMCI ADDR method found\n");
        result = acpi_evaluate_object(device->handle, "ADDR", NULL, &buffer);
        if (ACPI_FAILURE(result))
            return -EINVAL;

        obj = buffer.pointer;
        if (!obj || obj->type != ACPI_TYPE_PACKAGE || obj->package.count != 2 ) {
            printk(KERN_INFO "Invalid ADDR data\n");
            return -EINVAL;
        }
        qemuvmci_addr = obj->package.elements[0].integer.value +
            (obj->package.elements[1].integer.value << 32);
        return write_vmcoreinfo();
    }

    return 0;
}

static int __init qemuvmci_init(void)
{
    if (acpi_bus_register_driver(&qemuvmci_driver) < 0) {
        ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
                          "Error registering QEMUVMCI\n"));
        printk(KERN_INFO "Unable to register QEMUVMCI\n");
        return -ENODEV;
    }
    printk(KERN_INFO "QEMUVMCI module registered\n");
    return 0;
}

static int qemuvmci_remove(struct acpi_device *device)
{
    return 0;
}

static void __exit qemuvmci_exit(void)
{
    acpi_bus_unregister_driver(&qemuvmci_driver);
}

module_init(qemuvmci_init);
module_exit(qemuvmci_exit);

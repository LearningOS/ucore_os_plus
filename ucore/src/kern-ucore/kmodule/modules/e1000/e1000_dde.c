#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include "e1000_dde.h"

#define IRQ_OFFSET 32

const char e1000_dev_name[] = "e1000_pci_dev";
struct pci_dev e1000_dev;
struct net_device *netdev;

struct pci_func pcif_handler;

struct pci_device_id ent;

void e1000_dev_init(struct pci_func *pcif) {
	memset(&e1000_dev, 0, sizeof(struct pci_dev));


	e1000_dev.vendor = PCI_VENDOR(pcif->dev_id);
    e1000_dev.device = PCI_PRODUCT(pcif->dev_id);

    e1000_dev.devfn = (pcif->dev << 3) + pcif->func;
    e1000_dev.dev.init_name = e1000_dev_name;
	
    pci_setup_device(&e1000_dev);

}

static inline unsigned char *__skb_push(struct sk_buff *skb, unsigned int len)
{
        skb->data -= len;
        skb->len  += len;
        return skb->data;
}

struct sk_buff *alloc_skb_from_buf(void *buf, int buf_size) {
    unsigned int size;
    struct kmem_cache *cache;
    struct skb_shared_info *shinfo;
    struct sk_buff *skb;
    u8 *data;
    int new_buf_size = max(buf_size, 60);

    /* Get the HEAD */
    skb = ucore_kmalloc(sizeof(struct sk_buff));
    if (!skb)
        goto out;

    /* We do our best to align skb_shared_info on a separate cache
     * line. It usually works because kmalloc(X > SMP_CACHE_BYTES) gives
     * aligned memory blocks, unless SLUB/SLAB debug is enabled.
     * Both skb->head and skb_shared_info are cache line aligned.
     */
    size = new_buf_size + sizeof(struct skb_shared_info);
    data = ucore_kmalloc(size);
    if (!data)
        goto nodata;

    /*
     * Only clear those fields we need to clear, not those that we will
     * actually initialise below. Hence, don't put any more fields after
     * the tail pointer in struct sk_buff!
     */
    memset(skb, 0, offsetof(struct sk_buff, tail));
    /* Account for allocated memory : skb + skb->head */
    skb->truesize = size;
    skb->pfmemalloc = 0; // FIXME: what value?
    skb->len = new_buf_size;
    skb->head = skb->data = data;
    skb->tail = skb->end = new_buf_size;
    skb->mac_header = ~0U;
    skb->transport_header = ~0U;

    /* make sure we initialize shinfo sequentially */
    shinfo = skb_shinfo(skb);
    memset(shinfo, 0, offsetof(struct skb_shared_info, dataref));

    // put buf into skb
    memset(skb->data, 0, new_buf_size);
    memcpy(skb->data, buf, buf_size);

out:
    return skb;
nodata:
    kfree(skb);
    skb = NULL;
    goto out;
}

void
transmit_packet(void *buf, size_t size) {
    struct sk_buff *skb;
    skb = alloc_skb_from_buf(buf, size);
    netdev->netdev_ops->ndo_start_xmit(skb, netdev);
}

void test_transmission() {
    kprintf("Testing transmission...\n");
	uint8_t data[50];
	int i, j;

	for (i = 0; i < 50; i++) {
		data[i] = i;
	}

	for (i = 0; i < 18; i++) {
        kprintf("buf %x size %x\n", data, 50);
        transmit_packet(data, 50);
        for (j = 0; j < 50; j++)
            data[j] += 50;
	}
}

void open_e1000() {
    netdev->netdev_ops->ndo_open(netdev);
}

void stop_e1000() {
	netdev->netdev_ops->ndo_stop(netdev);
}

void e1000_intr_trap(int irq) {
    //kprintf("processing e1000 intr\n");
    e1000_intr(irq, netdev);
    //kprintf("processing e1000 intr done\n");
}

int e1000_irq_handler(int irq, void* data) {
	e1000_intr_trap(irq);
	//e1000_intr(irq, data);
	lapiceoi();
	return 0;
}

int request_threaded_irq(unsigned int irq, irq_handler_t handler,
                         irq_handler_t thread_fn, unsigned long irqflags,
                         const char *devname, void *dev_id) {
    kprintf("irq %d devname %s dev_id %x\n", irq, devname, dev_id);
    register_irq(IRQ_OFFSET+irq, e1000_irq_handler, NULL);
	ioapicenable(irq, 0);
    return 0;
}

//void e1000_irq_init() {
//	register_irq(T_SYSCALL, e1000_irq_handler, NULL);
//}

//int e1000_dde_init(struct pci_func *pcif) {
//    e1000_dev_init(pcif);//setup pci device
//}

//mv lwip to another module in future
int flag=1;
int pci_register_e1000() {
	memset(&pcif_handler,0,sizeof(struct pci_func));
	e1000_pcif_get(&pcif_handler);//get
	e1000_dev_init(&pcif_handler);//init
	register_transmit_func(&transmit_packet);

    
	e1000_probe(&e1000_dev, &ent);//set relation between pci and e1000(set as an netdevice in it)
    netdev = e1000_dev.dev.p;
    kprintf("netdev %x\n", netdev);

	
	//tmp test, should call by ifconfig in future
	open_e1000();
	
	if (flag)
	{
		void init_lwip_dev();
		init_lwip_dev();
		flag = 0;
	}
	return 0;
}
int pci_unregister_e1000(){
	//tmp test, should call by ifconfig in future
	stop_e1000();
	
	e1000_remove(&e1000_dev);
	unregister_transmit_func();
	//kfree(&e1000_dev);
	printk("goodby");
	return 0;
}

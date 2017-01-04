#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/time.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin Lynch");
MODULE_DESCRIPTION("An interrupt-based timing measurement driver for the BBB");
MODULE_VERSION("1.0");

// Macros swiped from parrot driver
#define dbg(format, arg...) do { if (debug) pr_info(CLASS_NAME ": %s: \n" format , __FUNCTION__ , ## arg); } while (0)
#define err(format, arg...) pr_err(CLASS_NAME ": " format, ## arg)
#define info(format, arg...) pr_info(CLASS_NAME ": " format, ## arg)
#define warn(format, arg...) pr_warn(CLASS_NAME ": " format, ## arg)

#define DEVICE_NAME "muon_timer_device"
#define CLASS_NAME "muon_timer"
#define MUON_TIMER_FIFO_SIZE 1024

// module parameters appear in /sys/module/muon_timer/parameters
// add 'debug=1' on insmod to change .. 
///g
static bool debug = false;
module_param(debug, bool, S_IRUGO);
MODULE_PARM_DESC(debug, "enable debug info (default: false)");
///
static unsigned int gpio_input = 115;  // BBB GPIO P9_27
module_param(gpio_input, uint, S_IRUGO);
MODULE_PARM_DESC(gpio_input, "BBB GPIO line for discriminated input (default: 115)");
///
static unsigned int gpio_reset  = 48; // BBB GPIO ???
module_param(gpio_reset, uint, S_IRUGO);
MODULE_PARM_DESC(gpio_reset, "BBB GPIO line for latch reset output (default: 48)");
///
static unsigned int gpio_pulse = 49; // BBB GPIO P9_23
module_param(gpio_pulse, uint, S_IRUGO);
MODULE_PARM_DESC(gpio_pulse, "BBB GPIO line for user output (default: 49)");

static int muon_major;
static struct class *muon_class = 0;
static struct device *muon_device = 0;

// We'll allow only one process into the timer at once
static DEFINE_MUTEX(muon_timer_mutex);

// define fifo ... 
static DEFINE_KFIFO(muon_timer_fifo, struct timeval, MUON_TIMER_FIFO_SIZE);
// going to need a recovery timer, too
// make pulse
// interrupt handler

// muon_timer_open
static int muon_timer_open(struct inode *inode, struct file *filp){
  dbg("\n");

  //// allow only read access; filched from parrot
  if ( ((filp->f_flags & O_ACCMODE) == O_WRONLY)
       || ((filp->f_flags & O_ACCMODE) == O_RDWR) ) {
    warn("write access is prohibited\n");
    return -EACCES;
  }
  
  //// grab mutex
  if(!mutex_trylock(&muon_timer_mutex)){
    warn("another process is accessing the muon_timer\n");
    return -EBUSY;
  }
  
  //// setup sysfs controls for reset and pulse and fifo clear
  //// setup interrupts
  //// do a reset
  return 0;
}

// muon_timer_release
static int muon_timer_release(struct inode *inode, struct file *filp){
  dbg("\n");
  //// kill interrupts
  //// free sysfs controls
  //// release mutex
  mutex_unlock(&muon_timer_mutex);
  return 0;
}
// muon_timer_read
//// pull from fifo, "translate", and return

static struct file_operations fops = {
  //  .read = muon_timer_read,
    .open = muon_timer_open,
    .release = muon_timer_release
};

// device setup and teardown
//// ought to test pin numbers to make sure they're sane...
static int __init muon_timer_init(void){
  int retval = 0;
  int i=0;
  struct timeval tv;

  dbg("\n");
  // Register char device and get a muon_major number
  muon_major = register_chrdev(0, DEVICE_NAME, &fops);
  if( muon_major<0 ){
    err("failed to register char device: error %d\n", muon_major);
    retval = muon_major;
    goto failed_chrdev;
  }

  // Register the muon_timer virtual class ... no physical bus to
  // connect to
  muon_class = class_create(THIS_MODULE, CLASS_NAME);
  if(IS_ERR(muon_class)){
    err("failed to register device class '%s'\n", CLASS_NAME);
    retval = PTR_ERR(muon_class);
    goto failed_class;
  }

  // create device and nodes in /dev
  muon_device = device_create(muon_class, NULL, MKDEV(muon_major, 0),
			      NULL, CLASS_NAME);
  if(IS_ERR(muon_device)){
    err("failed to create device '%s'\n", CLASS_NAME);
    retval = PTR_ERR(muon_device);
    goto failed_create;
  }

  for(; i<10; ++i){
    do_gettimeofday(&tv);
    kfifo_put(&muon_timer_fifo, &tv);
  }
    
  return 0;

 failed_create:
  class_destroy(muon_class);
 failed_class:
  unregister_chrdev(muon_major, DEVICE_NAME);
 failed_chrdev:
  return retval;
}

static void __exit muon_timer_exit(void){
  struct timeval tv;

  dbg("\n");

  while( kfifo_get(&muon_timer_fifo, &tv))
    info("from muon_timer_fifo: %ld %ld\n", tv.tv_sec, tv.tv_usec);

  // at this point, no other thread should exist with access to
  // muon_timer_fifo, so reset it before we go away
  kfifo_reset(&muon_timer_fifo);

  device_destroy(muon_class, MKDEV(muon_major,0));
  class_destroy(muon_class);
  unregister_chrdev(muon_major, DEVICE_NAME);
}


module_init(muon_timer_init);
module_exit(muon_timer_exit);

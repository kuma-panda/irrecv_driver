#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
// #include <linux/sched.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/ktime.h>
// #include <linux/timekeeping.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/math64.h>

#define GPIO_PIN_IRR    17

#define NUM_EDGES       68

#define IRR_IOCTL_READ  0
#define IRR_IOCTL_RESET 1

// NECフォーマットのタイミング検証マクロ
#define IS_LEADER_ON(t)     (8500 < (t) && (t) < 9500)  // リーダコードのON時間 (9ms)
#define IS_LEADER_OFF(t)    (3500 < (t) && (t) < 5500)  // リーダコードのOFF時間 (4.5ms)
#define IS_REPEAT_OFF(t)    (1250 < (t) && (t) < 3250)  // リピートコードのOFF時間 (2.25ms)
#define IS_DATA_ON(t)       (360 < (t) && (t) < 760)    // データコードのON時間 (0.56ms)
#define IS_DATA_OFF_1(t)    (1190 < (t) && (t) < 2190)  // データコード'1'のOFF時間 (1.69ms)
#define IS_DATA_OFF_0(t)    (360 < (t) && (t) < 760)    // データコード'0'のOFF時間 (0.56ms)

MODULE_LICENSE("Dual BSD/GPL");
#define DRIVER_NAME "IRReceiver"    // /proc/devices等で表示されるデバイス名

// このデバイスドライバで使うマイナー番号の開始番号と個数(=デバイス数)
static const unsigned int MINOR_BASE = 0;
static const unsigned int MINOR_NUM  = 1;

static unsigned int  g_irrdev_major;        // このデバイスドライバのメジャー番号(動的に決める)
static struct cdev   g_irrdev_cdev;         // キャラクタデバイスのオブジェクト
static struct class *g_irrdev_class = NULL; // デバイスドライバのクラスオブジェクト

static long g_time[NUM_EDGES];  // 入力信号のエッジ検出時刻(usec単位)
static int  g_state;            // 現在検出待ちのエッジインデックス(0～NUM_EDGES-1)
static bool g_irr_repeat;       // リピートコードを検出した場合は true


static bool decode(u32 *presult)
{
    bool success = true;
    *presult = 0x00000000;

    if( g_state < NUM_EDGES )
    {
        return false;
    }
    if( g_irr_repeat )
    {
        *presult = 0xFFFFFFFF;
    }
    else
    {
        for( int i = 2 ; i < 34 ; i += 2 )
        {
            if( !IS_DATA_ON(g_time[i+1] - g_time[i]) )
            {
                success = false;
                break;
            }    
            if( IS_DATA_OFF_1(g_time[i+2] - g_time[i+1]) )
            {
                *presult |= (0x00010000 << ((i-2)/2));
            }
            else if( !IS_DATA_OFF_0(g_time[i+2] - g_time[i+1]) )
            {
                success = false;
                break;
            }
        }
        for( uint8_t i = 34 ; i < 50 ; i += 2 )
        {
            if( !IS_DATA_ON(g_time[i+1] - g_time[i]) )
            {
                success = false;
                break;
            }    
            if( IS_DATA_OFF_1(g_time[i+2] - g_time[i+1]) )
            {
                *presult |= (0x00000100 << ((i-34)/2));
            }
            else if( !IS_DATA_OFF_0(g_time[i+2] - g_time[i+1]) )
            {
                success = false;
                break;
            }
        }
        for( uint8_t i = 50 ; i < 66 ; i += 2 )
        {
            if( !IS_DATA_ON(g_time[i+1] - g_time[i]) )
            {
                success = false;
                break;
            }    
            if( IS_DATA_OFF_1(g_time[i+2] - g_time[i+1]) )
            {
                *presult |= (0x00000001 << ((i-50)/2));
            }
            else if( !IS_DATA_OFF_0(g_time[i+2] - g_time[i+1]) )
            {
                success = false;
                break;
            }
        }
        if( success )
        {
            if( (((*presult & 0xFF00) >> 8) + (*presult & 0x00FF)) != 0xFF )
            {
                success = false;
            }
        }
    }
    g_state = 0;
    g_irr_repeat = false;
    return success;
}


// 割り込みハンドラ
static irqreturn_t irrdev_gpio_intr(int irq, void *dev_id)
{
    if( g_state >= NUM_EDGES )
    {
        return IRQ_HANDLED;
    }
    int f = gpio_get_value(GPIO_PIN_IRR);
    if( ((g_state % 2) && (f != 0)) || (((g_state % 2) == 0) && (f == 0)) )
    {
        g_time[g_state] = (long)div_u64(ktime_get_ns(), NSEC_PER_USEC);   //   t.tv_sec*1000000L + t.tv_nsec/1000L;
        if( g_state > 0 )
        {
            if( ((g_time[g_state] - g_time[g_state-1]) > 100000L) || ((g_time[g_state-1] - g_time[g_state]) > 100000L) )
            {
                g_state = 0;
                return IRQ_HANDLED;
            }
        }
        g_state++;
        if( g_state == 2 )
        {
            if( !IS_LEADER_ON(g_time[1] - g_time[0]) )
            {
                g_state = 0;
                return IRQ_HANDLED;
            }
        }
        if( g_state == 3 )
        {
            if( IS_REPEAT_OFF(g_time[2] - g_time[1]) )
            {
                g_irr_repeat = true;
                g_state = 99;
                return IRQ_HANDLED;
            }
            if( !IS_LEADER_OFF(g_time[2] - g_time[1]) )
            {
                g_state = 0;
                return IRQ_HANDLED;
            }
        }
    }
    else
    {
        g_state = 0;
    }
    return IRQ_HANDLED;
}

static int irrdev_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int irrdev_close(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t irrdev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    return 0;
}

static ssize_t irrdev_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    return 0;
}

static long irrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    if( cmd == IRR_IOCTL_READ )
    {
        u32 value = 0;
        long result = decode(&value)? 1 : 0;
        copy_to_user((unsigned char __user *)arg, &value, sizeof(value));             
        return result;
    }
    if( cmd == IRR_IOCTL_RESET )
    {
        g_state = 0;
        g_irr_repeat = false;
        return 0;
    }
    return -EFAULT;
}

struct file_operations s_irrdev_fops = {
    .open           = irrdev_open,
    .release        = irrdev_close,
    .read           = irrdev_read,
    .write          = irrdev_write,
    .unlocked_ioctl = irrdev_ioctl,
    .compat_ioctl   = irrdev_ioctl, // for 32-bit App
};

static int irrdev_init(void)
{
    printk("irrdev_init\n");

    int alloc_ret = 0;
    int cdev_err = 0;
    dev_t dev;
    int irq;

    // 1. 空いているメジャー番号を確保
    alloc_ret = alloc_chrdev_region(&dev, MINOR_BASE, MINOR_NUM, DRIVER_NAME);
    if( alloc_ret != 0 ) 
    {
        printk(KERN_ERR  "alloc_chrdev_region = %d\n", alloc_ret);
        return -1;
    }

    // 2. 取得したdev( = メジャー番号 + マイナー番号)からメジャー番号を取得して保持しておく
    g_irrdev_major = MAJOR(dev);
    dev = MKDEV(g_irrdev_major, MINOR_BASE);    /* 不要? */

    // 3. cdev構造体の初期化とシステムコールハンドラテーブルの登録
    cdev_init(&g_irrdev_cdev, &s_irrdev_fops);
    g_irrdev_cdev.owner = THIS_MODULE;

    // 4. このデバイスドライバ(cdev)をカーネルに登録する
    cdev_err = cdev_add(&g_irrdev_cdev, dev, MINOR_NUM);
    if( cdev_err != 0 ) 
    {
        printk(KERN_ERR  "cdev_add = %d\n", cdev_err);
        unregister_chrdev_region(dev, MINOR_NUM);
        return -1;
    }

    // 5. このデバイスのクラス登録をする(/sys/class/irrecv/ を作る)
    g_irrdev_class = class_create(THIS_MODULE, "irrecv");
    if( IS_ERR(g_irrdev_class) ) 
    {
        printk(KERN_ERR  "class_create\n");
        cdev_del(&g_irrdev_cdev);
        unregister_chrdev_region(dev, MINOR_NUM);
        return -1;
    }

    // 6. /sys/class/irrecv/irrecv* を作る
    for( int minor = MINOR_BASE; minor < MINOR_BASE + MINOR_NUM ; minor++ ) 
    {
        device_create(g_irrdev_class, NULL, MKDEV(g_irrdev_major, minor), NULL, "irrecv%d", minor);
    }

    // 赤外線リモコン受信モジュール用のGPIO04を入力にする
    gpio_direction_input(GPIO_PIN_IRR);

    // 割り込み番号を取得する
    irq = gpio_to_irq(GPIO_PIN_IRR);
    printk("gpio_to_irq = %d\n", irq);

    // 割り込みハンドラを登録する
    if (request_irq(irq, (void *)irrdev_gpio_intr, IRQF_SHARED | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "irrdev_gpio_intr", (void *)irrdev_gpio_intr) < 0) 
    {
        printk(KERN_ERR "request_irq\n");
        return -1;
    }
    return 0;
}

static void irrdev_exit(void)
{
    int irq;
    dev_t dev;

    printk("irrdev_exit\n");

    irq = gpio_to_irq(GPIO_PIN_IRR);
    free_irq(irq, (void *)irrdev_gpio_intr);

    dev = MKDEV(g_irrdev_major, MINOR_BASE);

    // 7. /sys/class/irrecv/irrecv* を削除する
    for( int minor = MINOR_BASE ; minor < MINOR_BASE + MINOR_NUM ; minor++ ) 
    {
        device_destroy(g_irrdev_class, MKDEV(g_irrdev_major, minor));
    }

    // 8. このデバイスのクラス登録を取り除く(/sys/class/irrecv/を削除する)
    class_destroy(g_irrdev_class);

    // 9. このデバイスドライバ(cdev)をカーネルから取り除く
    cdev_del(&g_irrdev_cdev);

    // 10. このデバイスドライバで使用していたメジャー番号の登録を取り除く
    unregister_chrdev_region(dev, MINOR_NUM);
}

module_init(irrdev_init);
module_exit(irrdev_exit);

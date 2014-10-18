/*
 * MD218A voice coil motor driver
 *
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include "IMX091AF.h"
#include "../camera/kd_camera_hw.h"

#define LENS_I2C_BUSNUM 1
static struct i2c_board_info __initdata kd_lens_dev={ I2C_BOARD_INFO("IMX091AF", 0x1C)};


#define IMX091AF_DRVNAME "IMX091AF"
#define IMX091AF_VCM_WRITE_ID           0x18

#define IMX091AF_DEBUG
#ifdef IMX091AF_DEBUG
#define IMX091AFDB printk
#else
#define IMX091AFDB(x,...)
#endif

static spinlock_t g_IMX091AF_SpinLock;

static struct i2c_client * g_pstIMX091AF_I2Cclient = NULL;

static dev_t g_IMX091AF_devno;
static struct cdev * g_pIMX091AF_CharDrv = NULL;
static struct class *actuator_class = NULL;

static int  g_s4IMX091AF_Opened = 0;
static long g_i4MotorStatus = 0;
static long g_i4Dir = 0;
static unsigned long g_u4IMX091AF_INF = 0;
static unsigned long g_u4IMX091AF_MACRO = 1023;
static unsigned long g_u4TargetPosition = 0;
static unsigned long g_u4CurrPosition   = 0;

static int g_sr = 3;

extern int mt_set_gpio_mode(unsigned long u4Pin, unsigned long u4Mode);
extern int mt_set_gpio_out(unsigned long u4Pin, unsigned long u4PinOut);
extern int mt_set_gpio_dir(unsigned long u4Pin, unsigned long u4Dir);


static int s4IMX091AF_ReadReg(unsigned short * a_pu2Result)
{
    int  i4RetValue = 0;
    char pBuff[2];

    i4RetValue = i2c_master_recv(g_pstIMX091AF_I2Cclient, pBuff , 2);

    if (i4RetValue < 0) 
    {
        IMX091AFDB("[IMX091AF] I2C read failed!! \n");
        return -1;
    }

    *a_pu2Result = (((u16)pBuff[0]) << 4) + (pBuff[1] >> 4);
    //*a_pu2Result = (((u16)(pBuff[0] & 0x03)) << 8) + pBuff[1];
    return 0;
}

static int s4IMX091AF_WriteReg(u16 a_u2Data)
{
    int  i4RetValue = 0;

    char puSendCmd[2] = {(char)(a_u2Data >> 4) , (char)(((a_u2Data & 0xF) << 4)+g_sr)};
    //char puSendCmd[2] = {(char)(((a_u2Data >> 8) & 0x03) | 0xc0), (char)(a_u2Data & 0xff)};


    //IMX091AFDB("[IMX091AF] g_sr %d, write %d \n", g_sr, a_u2Data);
    g_pstIMX091AF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    i4RetValue = i2c_master_send(g_pstIMX091AF_I2Cclient, puSendCmd, 2);
	
    if (i4RetValue < 0) 
    {
        IMX091AFDB("[IMX091AF] I2C send failed!! \n");
        return -1;
    }

    return 0;
}

inline static int getIMX091AFInfo(__user stIMX091AF_MotorInfo * pstMotorInfo)
{
    stIMX091AF_MotorInfo stMotorInfo;
    stMotorInfo.u4MacroPosition   = g_u4IMX091AF_MACRO;
    stMotorInfo.u4InfPosition     = g_u4IMX091AF_INF;
    stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
    stMotorInfo.bIsSupportSR      = TRUE;

	if (g_i4MotorStatus == 1)	{stMotorInfo.bIsMotorMoving = 1;}
	else						{stMotorInfo.bIsMotorMoving = 0;}

	if (g_s4IMX091AF_Opened >= 1)	{stMotorInfo.bIsMotorOpen = 1;}
	else						{stMotorInfo.bIsMotorOpen = 0;}

    if(copy_to_user(pstMotorInfo , &stMotorInfo , sizeof(stIMX091AF_MotorInfo)))
    {
        IMX091AFDB("[IMX091AF] copy to user failed when getting motor information \n");
    }

    return 0;
}

inline static int moveIMX091AF(unsigned long a_u4Position)
{
    int ret = 0;
    
    if((a_u4Position > g_u4IMX091AF_MACRO) || (a_u4Position < g_u4IMX091AF_INF))
    {
        IMX091AFDB("[IMX091AF] out of range \n");
        return -EINVAL;
    }

    if (g_s4IMX091AF_Opened == 1)
    {
        unsigned short InitPos;
        ret = s4IMX091AF_ReadReg(&InitPos);
	    
        spin_lock(&g_IMX091AF_SpinLock);
        if(ret == 0)
        {
            IMX091AFDB("[IMX091AF] Init Pos %6d \n", InitPos);
            g_u4CurrPosition = (unsigned long)InitPos;
        }
        else
        {		
            g_u4CurrPosition = 0;
        }
        g_s4IMX091AF_Opened = 2;
        spin_unlock(&g_IMX091AF_SpinLock);
    }

    if (g_u4CurrPosition < a_u4Position)
    {
        spin_lock(&g_IMX091AF_SpinLock);	
        g_i4Dir = 1;
        spin_unlock(&g_IMX091AF_SpinLock);	
    }
    else if (g_u4CurrPosition > a_u4Position)
    {
        spin_lock(&g_IMX091AF_SpinLock);	
        g_i4Dir = -1;
        spin_unlock(&g_IMX091AF_SpinLock);			
    }
    else										{return 0;}

    spin_lock(&g_IMX091AF_SpinLock);    
    g_u4TargetPosition = a_u4Position;
    spin_unlock(&g_IMX091AF_SpinLock);	

    //IMX091AFDB("[IMX091AF] move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition);

            spin_lock(&g_IMX091AF_SpinLock);
            g_sr = 3;
            g_i4MotorStatus = 0;
            spin_unlock(&g_IMX091AF_SpinLock);	
		
            if(s4IMX091AF_WriteReg((unsigned short)g_u4TargetPosition) == 0)
            {
                spin_lock(&g_IMX091AF_SpinLock);		
                g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
                spin_unlock(&g_IMX091AF_SpinLock);				
            }
            else
            {
                IMX091AFDB("[IMX091AF] set I2C failed when moving the motor \n");			
                spin_lock(&g_IMX091AF_SpinLock);
                g_i4MotorStatus = -1;
                spin_unlock(&g_IMX091AF_SpinLock);				
            }

    return 0;
}

inline static int setIMX091AFInf(unsigned long a_u4Position)
{
    spin_lock(&g_IMX091AF_SpinLock);
    g_u4IMX091AF_INF = a_u4Position;
    spin_unlock(&g_IMX091AF_SpinLock);	
    return 0;
}

inline static int setIMX091AFMacro(unsigned long a_u4Position)
{
    spin_lock(&g_IMX091AF_SpinLock);
    g_u4IMX091AF_MACRO = a_u4Position;
    spin_unlock(&g_IMX091AF_SpinLock);	
    return 0;	
}

////////////////////////////////////////////////////////////////
static long IMX091AF_Ioctl(
struct file * a_pstFile,
unsigned int a_u4Command,
unsigned long a_u4Param)
{
    long i4RetValue = 0;

    switch(a_u4Command)
    {
        case IMX091AFIOC_G_MOTORINFO :
            i4RetValue = getIMX091AFInfo((__user stIMX091AF_MotorInfo *)(a_u4Param));
        break;

        case IMX091AFIOC_T_MOVETO :
            i4RetValue = moveIMX091AF(a_u4Param);
        break;
 
        case IMX091AFIOC_T_SETINFPOS :
            i4RetValue = setIMX091AFInf(a_u4Param);
        break;

        case IMX091AFIOC_T_SETMACROPOS :
            i4RetValue = setIMX091AFMacro(a_u4Param);
        break;
		
        default :
      	    IMX091AFDB("[IMX091AF] No CMD \n");
            i4RetValue = -EPERM;
        break;
    }

    return i4RetValue;
}

//Main jobs:
// 1.check for device-specified errors, device not ready.
// 2.Initialize the device if it is opened for the first time.
// 3.Update f_op pointer.
// 4.Fill data structures into private_data
//CAM_RESET
static int IMX091AF_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
    IMX091AFDB("[IMX091AF] IMX091AF_Open - Start\n");

    spin_lock(&g_IMX091AF_SpinLock);

    if(g_s4IMX091AF_Opened)
    {
        spin_unlock(&g_IMX091AF_SpinLock);
        IMX091AFDB("[IMX091AF] the device is opened \n");
        return -EBUSY;
    }

    g_s4IMX091AF_Opened = 1;
		
    spin_unlock(&g_IMX091AF_SpinLock);

    IMX091AFDB("[IMX091AF] IMX091AF_Open - End\n");

    return 0;
}

//Main jobs:
// 1.Deallocate anything that "open" allocated in private_data.
// 2.Shut down the device on last close.
// 3.Only called once on last time.
// Q1 : Try release multiple times.
static int IMX091AF_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
    IMX091AFDB("[IMX091AF] IMX091AF_Release - Start\n");

    if (g_s4IMX091AF_Opened)
    {
        IMX091AFDB("[IMX091AF] feee \n");
        g_sr = 5;
	    s4IMX091AF_WriteReg(200);
        msleep(10);
	    s4IMX091AF_WriteReg(100);
        msleep(10);
            	            	    	    
        spin_lock(&g_IMX091AF_SpinLock);
        g_s4IMX091AF_Opened = 0;
        spin_unlock(&g_IMX091AF_SpinLock);

    }

    IMX091AFDB("[IMX091AF] IMX091AF_Release - End\n");

    return 0;
}

static const struct file_operations g_stIMX091AF_fops = 
{
    .owner = THIS_MODULE,
    .open = IMX091AF_Open,
    .release = IMX091AF_Release,
    .unlocked_ioctl = IMX091AF_Ioctl
};

inline static int Register_IMX091AF_CharDrv(void)
{
    struct device* vcm_device = NULL;

    IMX091AFDB("[IMX091AF] Register_IMX091AF_CharDrv - Start\n");

    //Allocate char driver no.
    if( alloc_chrdev_region(&g_IMX091AF_devno, 0, 1,IMX091AF_DRVNAME) )
    {
        IMX091AFDB("[IMX091AF] Allocate device no failed\n");

        return -EAGAIN;
    }

    //Allocate driver
    g_pIMX091AF_CharDrv = cdev_alloc();

    if(NULL == g_pIMX091AF_CharDrv)
    {
        unregister_chrdev_region(g_IMX091AF_devno, 1);

        IMX091AFDB("[IMX091AF] Allocate mem for kobject failed\n");

        return -ENOMEM;
    }

    //Attatch file operation.
    cdev_init(g_pIMX091AF_CharDrv, &g_stIMX091AF_fops);

    g_pIMX091AF_CharDrv->owner = THIS_MODULE;

    //Add to system
    if(cdev_add(g_pIMX091AF_CharDrv, g_IMX091AF_devno, 1))
    {
        IMX091AFDB("[IMX091AF] Attatch file operation failed\n");

        unregister_chrdev_region(g_IMX091AF_devno, 1);

        return -EAGAIN;
    }

    actuator_class = class_create(THIS_MODULE, "actuatordrv5");
    if (IS_ERR(actuator_class)) {
        int ret = PTR_ERR(actuator_class);
        IMX091AFDB("Unable to create class, err = %d\n", ret);
        return ret;            
    }

    vcm_device = device_create(actuator_class, NULL, g_IMX091AF_devno, NULL, IMX091AF_DRVNAME);

    if(NULL == vcm_device)
    {
        return -EIO;
    }
    
    IMX091AFDB("[IMX091AF] Register_IMX091AF_CharDrv - End\n");    
    return 0;
}

inline static void Unregister_IMX091AF_CharDrv(void)
{
    IMX091AFDB("[IMX091AF] Unregister_IMX091AF_CharDrv - Start\n");

    //Release char driver
    cdev_del(g_pIMX091AF_CharDrv);

    unregister_chrdev_region(g_IMX091AF_devno, 1);
    
    device_destroy(actuator_class, g_IMX091AF_devno);

    class_destroy(actuator_class);

    IMX091AFDB("[IMX091AF] Unregister_IMX091AF_CharDrv - End\n");    
}

//////////////////////////////////////////////////////////////////////

static int IMX091AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int IMX091AF_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id IMX091AF_i2c_id[] = {{IMX091AF_DRVNAME,0},{}};   
struct i2c_driver IMX091AF_i2c_driver = {                       
    .probe = IMX091AF_i2c_probe,                                   
    .remove = IMX091AF_i2c_remove,                           
    .driver.name = IMX091AF_DRVNAME,                 
    .id_table = IMX091AF_i2c_id,                             
};  

#if 0 
static int IMX091AF_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {         
    strcpy(info->type, IMX091AF_DRVNAME);                                                         
    return 0;                                                                                       
}      
#endif 
static int IMX091AF_i2c_remove(struct i2c_client *client) {
    return 0;
}

/* Kirby: add new-style driver {*/
static int IMX091AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int i4RetValue = 0;

    IMX091AFDB("[IMX091AF] IMX091AF_i2c_probe\n");

    /* Kirby: add new-style driver { */
    g_pstIMX091AF_I2Cclient = client;
    
    //g_pstIMX091AF_I2Cclient->addr = g_pstIMX091AF_I2Cclient->addr >> 1;
    g_pstIMX091AF_I2Cclient->addr = IMX091AF_VCM_WRITE_ID >> 1;
    
    //Register char driver
    i4RetValue = Register_IMX091AF_CharDrv();

    if(i4RetValue){

        IMX091AFDB("[IMX091AF] register char device failed!\n");

        return i4RetValue;
    }

    spin_lock_init(&g_IMX091AF_SpinLock);

    IMX091AFDB("[IMX091AF] Attached!! \n");

    return 0;
}

static int IMX091AF_probe(struct platform_device *pdev)
{
    return i2c_add_driver(&IMX091AF_i2c_driver);
}

static int IMX091AF_remove(struct platform_device *pdev)
{
    i2c_del_driver(&IMX091AF_i2c_driver);
    return 0;
}

static int IMX091AF_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    return 0;
}

static int IMX091AF_resume(struct platform_device *pdev)
{
    return 0;
}

// platform structure
static struct platform_driver g_stIMX091AF_Driver = {
    .probe		= IMX091AF_probe,
    .remove	= IMX091AF_remove,
    .suspend	= IMX091AF_suspend,
    .resume	= IMX091AF_resume,
    .driver		= {
        .name	= "lens_actuator5",
        .owner	= THIS_MODULE,
    }
};

static struct platform_device actuator_dev5 = {
	.name		  = "lens_actuator5",
	.id		  = -1,
};

static int __init IMX091AF_i2C_init(void)
{
    i2c_register_board_info(LENS_I2C_BUSNUM, &kd_lens_dev, 1);
    platform_device_register(&actuator_dev5);	
    if(platform_driver_register(&g_stIMX091AF_Driver)){
        IMX091AFDB("failed to register IMX091AF driver\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit IMX091AF_i2C_exit(void)
{
	platform_driver_unregister(&g_stIMX091AF_Driver);
}

module_init(IMX091AF_i2C_init);
module_exit(IMX091AF_i2C_exit);

MODULE_DESCRIPTION("IMX091AF lens module driver");
MODULE_AUTHOR("KY Chen <ky.chen@Mediatek.com>");
MODULE_LICENSE("GPL");




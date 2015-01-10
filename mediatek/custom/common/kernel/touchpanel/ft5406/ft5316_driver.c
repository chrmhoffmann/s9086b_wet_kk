/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

 
#include "tpd.h"
#include <linux/interrupt.h>
#include <cust_eint.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>

#include "tpd_custom_ft5306t.h"
//#ifdef MT6575
//#include <mach/mt6575_pm_ldo.h>
//#include <mach/mt6575_typedefs.h>
//#include <mach/mt6575_boot.h>
//#endif

#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <mach/eint.h>
#include "cust_gpio_usage.h"

#include <mach/upmu_common.h>

//zhaoshaopeng add for ft driver firmware update 20120903 start
#define FT_FM_UPDATE
extern int fts_ctpm_fw_upgrade_with_i_file(struct i2c_client *client);
extern int fts_ctpm_auto_upgrade(struct i2c_client *client);

//zhaoshaopeng end

#define FTS_CTL_IIC

#ifdef FTS_CTL_IIC
#include "focaltech_ctl.h"
extern int ft_rw_iic_drv_init(struct i2c_client *client);
extern void  ft_rw_iic_drv_exit(void);
#endif


 //zhaoshaopeng add start
//#define  TPD_DEBUG printk
extern struct tpd_device *tpd;
 
static struct i2c_client *i2c_client = NULL;
static struct task_struct *thread = NULL;

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DEFINE_MUTEX(i2c_access);
 
 
static void tpd_eint_interrupt_handler(void);
 
 
 
static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int __devexit tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);
 

static int tpd_flag = 0;
static int point_num = 0;
static int p_point_num = 0;

//#define TPD_CLOSE_POWER_IN_SLEEP

#define TPD_OK 0
//register define

#define DEVICE_MODE 0x00
#define GEST_ID 0x01
#define TD_STATUS 0x02

#define TOUCH1_XH 0x03
#define TOUCH1_XL 0x04
#define TOUCH1_YH 0x05
#define TOUCH1_YL 0x06

#define TOUCH2_XH 0x09
#define TOUCH2_XL 0x0A
#define TOUCH2_YH 0x0B
#define TOUCH2_YL 0x0C

#define TOUCH3_XH 0x0F
#define TOUCH3_XL 0x10
#define TOUCH3_YH 0x11
#define TOUCH3_YL 0x12

#define POINT_NUM 10
//register define
struct touch_info {
    int y[POINT_NUM];
    int x[POINT_NUM];
    int p[POINT_NUM];
    s16 touchId[POINT_NUM];
 //   u16 pressure;
    int count;
};
#define TPD_RESET_ISSUE_WORKAROUND

#define TPD_MAX_RESET_COUNT 3

#ifdef TPD_HAVE_BUTTON 
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT]   = TPD_WARP_END;
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX;
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

#define VELOCITY_CUSTOM_FT5306t
#ifdef VELOCITY_CUSTOM_FT5306t
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

// for magnify velocity********************************************
#define TOUCH_IOC_MAGIC 'A'

#define TPD_GET_VELOCITY_CUSTOM_X _IO(TOUCH_IOC_MAGIC,0)
#define TPD_GET_VELOCITY_CUSTOM_Y _IO(TOUCH_IOC_MAGIC,1)

static int g_v_magnify_x =TPD_VELOCITY_CUSTOM_X;
static int g_v_magnify_y =TPD_VELOCITY_CUSTOM_Y;
static int tpd_misc_open(struct inode *inode, struct file *file)
{
/*
	file->private_data = adxl345_i2c_client;

	if(file->private_data == NULL)
	{
		printk("tpd: null pointer!!\n");
		return -EINVAL;
	}
	*/
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int tpd_misc_release(struct inode *inode, struct file *file)
{
	//file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int adxl345_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
//       unsigned long arg)
static long tpd_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	//struct i2c_client *client = (struct i2c_client*)file->private_data;
	//struct adxl345_i2c_data *obj = (struct adxl345_i2c_data*)i2c_get_clientdata(client);	
	char strbuf[256];
	void __user *data;
	
	long err = 0;
	
	if(_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	}
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if(err)
	{
		printk("tpd: access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch(cmd)
	{
		case TPD_GET_VELOCITY_CUSTOM_X:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}			
			
			if(copy_to_user(data, &g_v_magnify_x, sizeof(g_v_magnify_x)))
			{
				err = -EFAULT;
				break;
			}				 
			break;

	   case TPD_GET_VELOCITY_CUSTOM_Y:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}			
			
			if(copy_to_user(data, &g_v_magnify_y, sizeof(g_v_magnify_y)))
			{
				err = -EFAULT;
				break;
			}				 
			break;


		default:
			printk("tpd: unknown IOCTL: 0x%08x\n", cmd);
			err = -ENOIOCTLCMD;
			break;
			
	}

	return err;
}


static struct file_operations tpd_fops = {
//	.owner = THIS_MODULE,
	.open = tpd_misc_open,
	.release = tpd_misc_release,
	.unlocked_ioctl = tpd_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice tpd_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "touch",
	.fops = &tpd_fops,
};

//**********************************************
#endif

//zhaoshaopeng
/*
static struct touch_info {
    int y[3];
    int x[3];
    int p[3];
    int count;
};
*/

//zhaoshaopeng end
  //zhaoshaopeng add start
#define TPD_X_RES (1080)
#define TPD_Y_RES (1920)
#define TPD_WARP_Y(y) ( TPD_Y_RES - 1 - y )
#define TPD_WARP_X(x) (x)//( TPD_X_RES - 1 - x )//(x)


 static const struct i2c_device_id ft5306t_tpd_id[] = {{"ft5306t",0},{}};
 //unsigned short force[] = {0,0x70,I2C_CLIENT_END,I2C_CLIENT_END}; 
 //static const unsigned short * const forces[] = { force, NULL };
 //static struct i2c_client_address_data addr_data = { .forces = forces, };
 static struct i2c_board_info __initdata ft5306t_i2c_tpd={ I2C_BOARD_INFO("ft5306t", (0x70>>1))};
 
 
 static struct i2c_driver tpd_i2c_driver = {
  .driver = {
	 .name = "ft5306t",//.name = TPD_DEVICE,
//	 .owner = THIS_MODULE,
  },
  .probe = tpd_probe,
  .remove = __devexit_p(tpd_remove),
  .id_table = ft5306t_tpd_id,
  .detect = tpd_detect,
//  .address_data = &addr_data,
 };
 

static  void tpd_down(int x, int y, int p, int id) {
    if ((!p) && (!id))
    {
        input_report_abs(tpd->dev, ABS_MT_PRESSURE, 100);
        input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 100);
    }
    else
    {
        input_report_abs(tpd->dev, ABS_MT_PRESSURE, p);
        input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, p);
        /* track id Start 0 */
        //put_report_abs(tpd->dev, ABS_MT_TRACKING_ID, id);//zhaoshaopeng delete this
    }
	 TPD_EM_PRINT(x, y, x, y, p, 1);
    input_report_key(tpd->dev, BTN_TOUCH, 1);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(tpd->dev);

     if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
     {   
       tpd_button(x, y, 1);  
     }
	 if(y > TPD_RES_Y) //virtual key debounce to avoid android ANR issue
	 {
        // msleep(50);
		 printk("D virtual key \n");
	 }

 }
 
static  void tpd_up(int x, int y,int *count) {
	 //if(*count>0) {
		 //input_report_abs(tpd->dev, ABS_PRESSURE, 0);
		 input_report_key(tpd->dev, BTN_TOUCH, 0);
		 //input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
		 //input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
		 //input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
		 //printk("U[%4d %4d %4d] ", x, y, 0);
		 input_mt_sync(tpd->dev);
		 TPD_EM_PRINT(x, y, x, y, 0, 0);
	//	 (*count)--;
     if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
     {   
        tpd_button(x, y, 0); 
     }   		 

 }

 static int tpd_touchinfo(struct touch_info *cinfo, struct touch_info *pinfo)
 {

	int i = 0;
	
	//char data[30] = {0};
	char data[50] = {0};

        u16 high_byte,low_byte;
	u8 report_rate =0;

	p_point_num = point_num;

	i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 8, &(data[0]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0x08, 8, &(data[8]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0x10, 8, &(data[16]));
	//zhaoshaopeng start
	//i2c_smbus_read_i2c_block_data(i2c_client, 0xa6, 1, &(data[24]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0x18, 8, &(data[24]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0x20, 8, &(data[32]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0xa6, 1, &(data[40]));
	
	//TPD_DEBUG("FW version=%x]\n",data[24]);
	//TPD_DEBUG("FW version=%x]\n",data[40]);
	//zhaoshaopeng end
	//TPD_DEBUG("received raw data from touch panel as following:\n");
	//TPD_DEBUG("[data[0]=%x,data[1]= %x ,data[2]=%x ,data[3]=%x ,data[4]=%x ,data[5]=%x]\n",data[0],data[1],data[2],data[3],data[4],data[5]);
	//TPD_DEBUG("[data[9]=%x,data[10]= %x ,data[11]=%x ,data[12]=%x]\n",data[9],data[10],data[11],data[12]);
	//TPD_DEBUG("[data[15]=%x,data[16]= %x ,data[17]=%x ,data[18]=%x]\n",data[15],data[16],data[17],data[18]);

        /* Device Mode[2:0] == 0 :Normal operating Mode*/
        if(data[0] & 0x70 != 0) return false; 

    //    
	 //we have  to re update report rate
    // TPD_DMESG("report rate =%x\n",report_rate);
	 if(report_rate < 8)
	 {
	   report_rate = 0x8;
	   if((i2c_smbus_write_i2c_block_data(i2c_client, 0x88, 1, &report_rate))< 0)
	   {
		   TPD_DMESG("I2C read report rate error, line: %d\n", __LINE__);
	   }
	 }
	/*get the number of the touch points*/
	cinfo->count = data[2] & 0x07;	point_num = cinfo->count;
/*	
     if(cinfo->count == 0)
     {
         	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
         	input_sync(tpd->dev);
          return 0;
     }
*/
     for(i = 0;i<cinfo->count ;i++)
		{
			//cinfo->p[i] = data[3+6*i] >> 6; //event flag 

	       /*get the X coordinate, 2 bytes*/
			high_byte = data[3+6*i];
			high_byte <<= 8;
			high_byte &= 0x0f00;
			low_byte = data[3+6*i + 1];
			cinfo->x[i] = high_byte |low_byte;

				//cinfo->x[i] =  cinfo->x[i] * 480 >> 11; //calibra
		
			/*get the Y coordinate, 2 bytes*/
			
			high_byte = data[3+6*i+2];
			high_byte <<= 8;
			high_byte &= 0x0f00;
			low_byte = data[3+6*i+3];
			cinfo->y[i] = high_byte |low_byte;


			cinfo->p[i] = data[3+6*i+4];
			//cinfo->p[i] = data[3+6*i+5];
               /*get touch ID*/
			cinfo->touchId[i] = (data[3+6*i+2] & 0xf0)>>4;
           //printk("\r\n zhaoshaopeng cinfo->count =%d, i=%d , cinfo->touchId[%d]=%d \r\n", cinfo->count, i, cinfo->touchId[i]);			   
			     }
	//printk("\r\n\r\n");	
 
/*
     for(i = 0;i<cinfo->count; i++)
                          {
		input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, cinfo->touchId[i]);			
		input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 1);
		input_report_abs(tpd->dev, ABS_MT_POSITION_X, cinfo->x[i]);
		input_report_abs(tpd->dev, ABS_MT_POSITION_Y, cinfo->y[i]);
	//	input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, 1);
		input_mt_sync(tpd->dev);
			     }
      input_sync(tpd->dev);
			
*/	 return true;

 };

 static int touch_event_handler(void *unused)
 {
  
    struct touch_info cinfo, pinfo;
    int usb_state =0;
    u8 report_rate_1 =0x01; 
    u8 report_rate_0 =0x00; 
   
    //printk("\r\n zhaoshaopeng for k500 touch_event_handler \r\n");
	 struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	 sched_setscheduler(current, SCHED_RR, &param);

 
	 do
	 {
	  mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
		 set_current_state(TASK_INTERRUPTIBLE); 
		  wait_event_interruptible(waiter,tpd_flag!=0);
						 
			 tpd_flag = 0;
			 
		 set_current_state(TASK_RUNNING);
	 #if 1
 	usb_state=upmu_get_rgs_chrdet();
 	if(usb_state != 0)
 	{
 		i2c_smbus_write_i2c_block_data(i2c_client, 0x8b, 1, &report_rate_1);
 	}
 	else
 	{
 		i2c_smbus_write_i2c_block_data(i2c_client, 0x8b, 1, &report_rate_0);
 	}
       #endif		 

		  if (tpd_touchinfo(&cinfo, &pinfo)) 
		  {
		    //TPD_DEBUG("point_num = %d\n",point_num);
			TPD_DEBUG_SET_TIME;
		  
           if(point_num >0) {
                tpd_down(TPD_WARP_X(cinfo.x[0]), cinfo.y[0], cinfo.p[0],cinfo.touchId[0]);
                if(point_num>1)
             	{     
			if(cinfo.y[1] <= TPD_Y_RES)
				tpd_down(TPD_WARP_X(cinfo.x[1]), cinfo.y[1], cinfo.p[1],cinfo.touchId[1]);
				if(point_num >2)
             			{
					if(cinfo.y[2] <= TPD_Y_RES)
			   	    	tpd_down(TPD_WARP_X(cinfo.x[2]), cinfo.y[2], cinfo.p[2],cinfo.touchId[2]);
				    		if(point_num >3)
    			        		{
							if(cinfo.y[3] <= TPD_Y_RES)
    			   	        		tpd_down(TPD_WARP_X(cinfo.x[3]), cinfo.y[3], cinfo.p[3],cinfo.touchId[3]);
					 			if(point_num >4)
    			            				{
									if(cinfo.y[4] <= TPD_Y_RES)
    			   	            				tpd_down(TPD_WARP_X(cinfo.x[4]), cinfo.y[4], cinfo.p[4],cinfo.touchId[4]);
	    			   	            				if(point_num >5)
	    			            					{
											if(cinfo.y[5] <= TPD_Y_RES)
	    			   	            					tpd_down(TPD_WARP_X(cinfo.x[5]), cinfo.y[5], cinfo.p[5],cinfo.touchId[5]);
		    			   	            					if(point_num >6)
	    			            							{
													if(cinfo.y[6] <= TPD_Y_RES)
	    			   	            							tpd_down(TPD_WARP_X(cinfo.x[6]), cinfo.y[6], cinfo.p[6],cinfo.touchId[6]);
		    			   	            							if(point_num >7)
	    			            									{
															if(cinfo.y[7] <= TPD_Y_RES)
	    			   	            									tpd_down(TPD_WARP_X(cinfo.x[7]), cinfo.y[7], cinfo.p[7],cinfo.touchId[7]);
	    			   	            		    			   	            			if(point_num >8)
					    			            							{
																	if(cinfo.y[8] <= TPD_Y_RES)
					    			   	            							tpd_down(TPD_WARP_X(cinfo.x[8]), cinfo.y[8], cinfo.p[8],cinfo.touchId[8]);
					    					    			   	            			if(point_num >9)
							    			            							{
																			if(cinfo.y[9] <= TPD_Y_RES)
							    			   	            							tpd_down(TPD_WARP_X(cinfo.x[9]), cinfo.y[9], cinfo.p[9],cinfo.touchId[9]);
							    			   	            							
							    			              							}   	            							
					    			              							}							
	    			              									}	    			   	            							
	    			              							}
	    			              					}
    			              				}
    			           		}
				}
             	}
                input_sync(tpd->dev);
				//printk("press ---> cinfo.x[0]=%d ,cinfo.y[0]=%d \n",cinfo.x[0],cinfo.y[0]);
				
            }else{
			    tpd_up(cinfo.x[0], cinfo.y[0], 0);
                //TPD_DEBUG("release --->\n"); 
                //input_mt_sync(tpd->dev);
                input_sync(tpd->dev);
            }
        }

 }while(!kthread_should_stop());
 
	 return 0;
 }
 
 static int tpd_detect (struct i2c_client *client, int kind, struct i2c_board_info *info) 
 {
	 strcpy(info->type, TPD_DEVICE);	
	  return 0;
 }
 
 static void tpd_eint_interrupt_handler(void)
 {
         //printk("\r\n zhaoshaopeng for k500 tpd_eint_interrupt_handler FT5306t \r\n");
	 TPD_DEBUG("TPD interrupt has been triggered\n");
	 tpd_flag = 1;
	 wake_up_interruptible(&waiter);
	 
 }
 static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
 {	 
	int retval = TPD_OK;
	char data;
	u8 report_rate=0;
	int err=0;
	int reset_count = 0;

reset_proc:   
	i2c_client = client;

      // printk("\r\n zhaoshaopeng add to test ft_5306t tpd_i2c_probe\r\n ");
#ifdef __FT5X06_HASE_KEYS__
       int key;
       for(key=0;key<TOUCHKEYNONE;key++)
              set_bit(ft5x06_touch_keyarry[key],tpd->dev->keybit); 
#endif

   
//#ifdef MT6575
//    //power on, need confirm with SA
//    hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
//    //hwPowerOn(MT65XX_POWER_LDO_VGP, VOL_1800, "TP");//zhaoshaopeng delete      
//#endif	
//#ifdef MT6577
    //power on, need confirm with SA
//    hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
    //hwPowerOn(MT65XX_POWER_LDO_VGP, VOL_1800, "TP");//zhaoshaopeng delete 
	hwPowerOn(MT65XX_POWER_LDO_VGP4, VOL_2800, "TP");
	msleep(1);
	hwPowerOn(MT65XX_POWER_LDO_VGP5, VOL_1800, "TP");

//#endif	

#ifdef TPD_CLOSE_POWER_IN_SLEEP	 
	hwPowerDown(TPD_POWER_SOURCE,"TP");
	hwPowerOn(TPD_POWER_SOURCE,VOL_3300,"TP");
	msleep(100);
#else
//	#ifdef MT6573
//	mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
//  mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
//	mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
//	msleep(100);
//	#endif
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
    msleep(3);	
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
    msleep(10);  
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
    msleep(3);
	#endif

	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
 
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, tpd_eint_interrupt_handler, 1); 
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
 
	msleep(100);
 
	if((i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 1, &data))< 0)
	{
		TPD_DMESG("I2C transfer error, line: %d\n", __LINE__);
//	#ifdef MT6575
//       hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
//       #endif    
//       #ifdef MT6577
//       hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
		hwPowerDown(MT65XX_POWER_LDO_VGP4, "TP");
		msleep(1);
		hwPowerDown(MT65XX_POWER_LDO_VGP5, "TP");

//       #endif    
		msleep(10);
#ifdef TPD_RESET_ISSUE_WORKAROUND
        if ( reset_count < TPD_MAX_RESET_COUNT )
        {
            reset_count++;
            goto reset_proc;
        }
#endif
		   return -1; 
	}

	//set report rate 80Hz
	report_rate = 0x8; 
	if((i2c_smbus_write_i2c_block_data(i2c_client, 0x88, 1, &report_rate))< 0)
	{
	    if((i2c_smbus_write_i2c_block_data(i2c_client, 0x88, 1, &report_rate))< 0)
	    {
		   TPD_DMESG("I2C read report rate error, line: %d\n", __LINE__);
	    }
		   
	}

	tpd_load_status = 1;

	#ifdef VELOCITY_CUSTOM_FT5306t
	if((err = misc_register(&tpd_misc_device)))
	{
		printk("mtk_tpd: tpd_misc_device register failed\n");
		
	}
	#endif

//zhaoshaopeng add for ft firmware update
#ifdef  FT_FM_UPDATE
	//ft5x0x_read_reg(0xa6,&buf);//can be deleted
	msleep(50);  //must add or "Step 3:check READ-ID" will return error 
       if(fts_ctpm_auto_upgrade(i2c_client) == 0)
     	{
     	     printk("\r\n zhaoshaopeng tp firmware update success! \r\n");
     	}
#endif
//zhaoshaopeng end

	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	 if (IS_ERR(thread))
		 { 
		  retval = PTR_ERR(thread);
		  TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", retval);
		}
	#ifdef FTS_CTL_IIC
		if (ft_rw_iic_drv_init(client) < 0)
			dev_err(&client->dev, "%s:[FTS] create fts control iic driver failed\n",
					__func__);
	#endif


	TPD_DMESG("ft5306t Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");
   return 0;
   
 }

 static int __devexit tpd_remove(struct i2c_client *client)
 
 {
     	#ifdef FTS_CTL_IIC
	ft_rw_iic_drv_exit();
	#endif 
	 TPD_DEBUG("TPD removed\n");
 
   return 0;
 }
 
 
 static int tpd_local_init(void)
 {

 
  //TPD_DMESG("Focaltech FT5306t I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);
 
 
   if(i2c_add_driver(&tpd_i2c_driver)!=0)
   	{
  		TPD_DMESG("ft5306t unable to add i2c driver.\n");
      	return -1;
    }
   //zhaoshaopeng add
    if(tpd_load_status == 0) 
    {
    	TPD_DMESG("add error touch panel driver.======ft5306t\n");
    	i2c_del_driver(&tpd_i2c_driver);
    	return -1;
    }
//end
#ifdef TPD_HAVE_BUTTON     
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif   
  
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))    
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT*4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT*4);
#endif 

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_def_calmat_local, 8*4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local, 8*4);	
#endif  
		TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);  
		tpd_type_cap = 1;
    return 0; 
 }

 static int tpd_resume(struct i2c_client *client)
 {
  int retval = TPD_OK;
 
   TPD_DEBUG("TPD wake up\n");
//#ifdef MT6575
//    //power on, need confirm with SA
//    //hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
//    //hwPowerOn(MT65XX_POWER_LDO_VGP, VOL_1800, "TP");  
//#endif  	
//#ifdef MT6577
//    //power on, need confirm with SA
//    //hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
//    //hwPowerOn(MT65XX_POWER_LDO_VGP, VOL_1800, "TP");  
//#endif   
#ifdef TPD_CLOSE_POWER_IN_SLEEP	
	hwPowerOn(TPD_POWER_SOURCE,VOL_3300,"TP"); 
#else
//#ifdef MT6573
//	mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
//    mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
//	mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
//#endif	
	msleep(100);
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
    msleep(3);	
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
    msleep(10);  
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
    msleep(3);
#endif
   mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);  
	
	 return retval;
 }
 
 static int tpd_suspend(struct i2c_client *client, pm_message_t message)
 {
	 int retval = TPD_OK;
	 static char data = 0x3;
 
	 TPD_DEBUG("TPD enter sleep\n");
	 mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#ifdef TPD_CLOSE_POWER_IN_SLEEP	
	hwPowerDown(TPD_POWER_SOURCE,"TP");
#else
i2c_smbus_write_i2c_block_data(i2c_client, 0xA5, 1, &data);  //TP enter sleep mode
#ifdef MT6573
mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ZERO);
#endif

#endif



//#ifdef MT6575
//    //power on, need confirm with SA
//    //hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
//    //hwPowerOn(MT65XX_POWER_LDO_VGP, VOL_1800, "TP");  
//#endif    
//#ifdef MT6577
//    //power on, need confirm with SA
//    //hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
//    //hwPowerOn(MT65XX_POWER_LDO_VGP, VOL_1800, "TP");  
//#endif    	 
	 return retval;
 } 


 static struct tpd_driver_t tpd_device_driver = {
		 .tpd_device_name = "FT5306t",
		 .tpd_local_init = tpd_local_init,
		 .suspend = tpd_suspend,
		 .resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
		 .tpd_have_button = 1,
#else
		 .tpd_have_button = 0,
#endif		
 };
 /* called when loaded into kernel */
 static int __init tpd_driver_init(void) {
	 printk("MediaTek FT5306t touch panel driver init\n");
	   i2c_register_board_info(0, &ft5306t_i2c_tpd, 1);
		 if(tpd_driver_add(&tpd_device_driver) < 0)
			 TPD_DMESG("add FT5306t driver failed\n");
	 return 0;
 }
 
 /* should never be called */
 static void __exit tpd_driver_exit(void) {
	 TPD_DMESG("MediaTek FT5306t touch panel driver exit\n");
	 //input_unregister_device(tpd->dev);
	 tpd_driver_remove(&tpd_device_driver);
 }
 
 module_init(tpd_driver_init);
 module_exit(tpd_driver_exit);



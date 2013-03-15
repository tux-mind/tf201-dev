#ifndef _ELAN_I2C_ASUS_H
#define _ELAN_I2C_ASUS_H

#include "asusdec.h"
#include <linux/input.h>

#define ELAN_DEBUG			0
#if ELAN_DEBUG
#define ELAN_INFO(format, arg...)	\
	printk(KERN_INFO "elan_i2c_asus: [%s] " format , __FUNCTION__ , ## arg)
#else
#define ELAN_INFO(format, arg...)
#endif


#define ELAN_ERR(format, arg...)	\
	printk(KERN_ERR "elan_i2c_asus: [%s] " format , __FUNCTION__ , ## arg)

#define CONVERSION_TIME_MS		50

#define ELAN_RETRY_COUNT		3

#define PSMOUSE_CMD_SETSCALE11	0x00e6
#define PSMOUSE_CMD_SETSCALE21	0x00e7
#define PSMOUSE_CMD_SETRES	0x10e8
#define PSMOUSE_CMD_GETINFO	0x03e9
#define PSMOUSE_CMD_SETSTREAM	0x00ea
#define PSMOUSE_CMD_SETPOLL	0x00f0
#define PSMOUSE_CMD_POLL	0x00eb	/* caller sets number of bytes to receive */
#define PSMOUSE_CMD_GETID	0x02f2
#define PSMOUSE_CMD_SETRATE	0x10f3
#define PSMOUSE_CMD_ENABLE	0x00f4
#define PSMOUSE_CMD_DISABLE	0x00f5
#define PSMOUSE_CMD_RESET_DIS	0x00f6
#define PSMOUSE_CMD_RESET_BAT	0x02ff

#define PSMOUSE_RET_BAT		0xaa
#define PSMOUSE_RET_ID		0x00
#define PSMOUSE_RET_ACK		0xfa
#define PSMOUSE_RET_NAK		0xfe
#define ELANTOUCHPAD		727


/*
 * Command values for Synaptics style queries
 */
#define ETP_FW_VERSION_QUERY		0x01
#define ETP_CAPABILITIES_QUERY		0x02


/*
 * Command values for register reading or writing
 */
#define ETP_REGISTER_READ		0x10
#define ETP_REGISTER_WRITE		0x11
#define ETP_REGISTER_RW			0x00

/*
 * Hardware version 2 custom PS/2 command value
 */
#define ETP_PS2_CUSTOM_COMMAND		0x00f8

/*
 * Times to retry a ps2_command and millisecond delay between tries
 */
#define ETP_PS2_COMMAND_TRIES		3
#define ETP_PS2_COMMAND_DELAY		500

/*
 * Times to try to read back a register and millisecond delay between tries
 */
#define ETP_READ_BACK_TRIES		5
#define ETP_READ_BACK_DELAY		2000

/*
 * Register bitmasks for hardware version 1
 */
#define ETP_R10_ABSOLUTE_MODE		0x04
#define ETP_R11_4_BYTE_MODE		0x02

/*
 * Capability bitmasks
 */
#define ETP_CAP_HAS_ROCKER		0x04

/*
 * One hard to find application note states that X axis range is 0 to 576
 * and Y axis range is 0 to 384 for harware version 1.
 * Edge fuzz might be necessary because of bezel around the touchpad
 */
#define ETP_EDGE_FUZZ_V1		32

#define ETP_XMIN_V1			(  0 + ETP_EDGE_FUZZ_V1)
#define ETP_XMAX_V1			(576 - ETP_EDGE_FUZZ_V1)
#define ETP_YMIN_V1			(  0 + ETP_EDGE_FUZZ_V1)
#define ETP_YMAX_V1			(384 - ETP_EDGE_FUZZ_V1)

/*
 * It seems the resolution for hardware version 2 doubled.
 * Hence the X and Y ranges are doubled too.
 * The bezel around the pad also appears to be smaller
 */
#define ETP_EDGE_FUZZ_V2		8

#define ETP_XMIN_V2			(   0 + ETP_EDGE_FUZZ_V2)
#define ETP_XMAX_V2			(1152 - ETP_EDGE_FUZZ_V2)
#define ETP_YMIN_V2			(   0 + ETP_EDGE_FUZZ_V2)
#define ETP_YMAX_V2			( 768 - ETP_EDGE_FUZZ_V2)

#define ETP_WMIN_V2                     0
#define ETP_WMAX_V2                     15

#define ETP_PMIN_V2			0
#define ETP_PMAX_V2			255
#define ETP_WMIN_V2			0
#define ETP_WMAX_V2			15

/*
 * For two finger touches the coordinate of each finger gets reported
 * separately but with reduced resolution.
 */
#define ETP_2FT_FUZZ			4

#define ETP_2FT_XMIN			(  0 + ETP_2FT_FUZZ)
#define ETP_2FT_XMAX			(288 - ETP_2FT_FUZZ)
#define ETP_2FT_YMIN			(  0 + ETP_2FT_FUZZ)
#define ETP_2FT_YMAX			(192 - ETP_2FT_FUZZ)

struct elantech_data {
	unsigned int xmax;
	unsigned int ymax;
	unsigned int fw_version;

	int left_button;
	int right_button;

	int fingers;
	struct { int x, y; } pos[2];

	struct input_dev *abs_dev;
};

int elantech_detect(struct asusdec_chip *ec_chip);
int elantech_init(struct asusdec_chip *ec_chip);
void elantech_report_absolute_to_related(struct asusdec_chip *ec_chip, int *Null_data_times);
#endif





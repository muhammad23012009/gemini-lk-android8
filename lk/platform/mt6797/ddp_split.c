#define LOG_TAG "SPLIT"


#include "platform/ddp_log.h"
#include "platform/ddp_info.h"
#include "platform/ddp_reg.h"
//#include "platform/ddp_split.h"

//#include <mach/mt_clkmgr.h>
//#include <linux/delay.h>


char *split_state(unsigned int state)
{
	switch (state) {
		case 1:
			return "idle";
		case 2:
			return "wait";
		case 4:
			return "busy";
		default:
			return "unknown";
	}
	return "unknown";
}

static int split_clock_on(DISP_MODULE_ENUM module, void *handle)
{
	ddp_enable_module_clock(module);
	return 0;
}

static int split_clock_off(DISP_MODULE_ENUM module, void *handle)
{
	ddp_disable_module_clock(module);
	return 0;
}

static int split_init(DISP_MODULE_ENUM module, void *handle)
{
	ddp_enable_module_clock(module);
	return 0;
}

static int split_deinit(DISP_MODULE_ENUM module, void *handle)
{
	ddp_enable_module_clock(module);
	return 0;
}

static int split_start(DISP_MODULE_ENUM module, void *handle)
{
	DISP_REG_SET(handle, DISP_REG_SPLIT_ENABLE, 0x01);
	return 0;
}

static int split_stop(DISP_MODULE_ENUM module, void *handle)
{
	DISP_REG_SET(handle, DISP_REG_SPLIT_ENABLE, 0x0);
	return 0;
}

static int split_config(DISP_MODULE_ENUM module, disp_ddp_path_config *pConfig, void *handle)
{
	DISP_REG_SET(handle, DISP_REG_SPLIT_CFG, 0x00);/* 0: even pixel first, 1: odd pexel first */
	DISP_REG_SET(handle, DISP_REG_SPLIT_DEBUG, 0x80000000);/* 0: even pixel first, 1: odd pexel first */

	return 0;
}

static int split_busy(DISP_MODULE_ENUM module)
{
	unsigned int state = DISP_REG_GET_FIELD(DEBUG_FLD_SPLIT_FSM, DISP_REG_SPLIT_DEBUG);

	return (state & 0x4);
}

static int split_idle(DISP_MODULE_ENUM module)
{
	unsigned int state = DISP_REG_GET_FIELD(DEBUG_FLD_SPLIT_FSM, DISP_REG_SPLIT_DEBUG);

	return (state & 0x3);
}

int split_reset(DISP_MODULE_ENUM module,void *handle)
{
	unsigned int delay_cnt = 0;

	DISP_REG_SET(handle, DISP_REG_SPLIT_SW_RESET, 0x1);
	DISP_REG_SET(handle, DISP_REG_SPLIT_SW_RESET, 0x0);
	/*always use cpu do reset*/
	while ((DISP_REG_GET_FIELD(DEBUG_FLD_SPLIT_FSM, DISP_REG_SPLIT_DEBUG) & 0x3) == 0) {
		delay_cnt++;
		//udelay(10);
		if (delay_cnt > 2000) {
			DDPERR("split_reset() timeout!\n");
			break;
		}
	}
	return 0;
}

void split_dump_reg(DISP_MODULE_ENUM module)
{
	DDPMSG("=DISP SPLIT REGS=\n");
	DDPMSG("ENABLE=0x%x\n", DISP_REG_GET(DISP_REG_SPLIT_ENABLE));
	DDPMSG("SW_RST=0x%x\n", DISP_REG_GET(DISP_REG_SPLIT_SW_RESET));
	DDPMSG("DEBUG=0x%x\n", DISP_REG_GET(DISP_REG_SPLIT_DEBUG));
}

void split_dump_analysis(DISP_MODULE_ENUM module)
{
	unsigned int pixel = DISP_REG_GET_FIELD(DEBUG_FLD_IN_PIXEL_CNT, DISP_REG_SPLIT_DEBUG);
	unsigned int state = DISP_REG_GET_FIELD(DEBUG_FLD_SPLIT_FSM, DISP_REG_SPLIT_DEBUG);

	DDPMSG("=DISP SPLIT0 ANALYSIS=\n");
	DDPMSG("cur_pixel %u,state %s\n", pixel, split_state(state));
}

static int split_dump(DISP_MODULE_ENUM module, int level)
{
	split_dump_analysis(module);
	split_dump_reg(module);

	return 0;
}

static int split_debug(DISP_MODULE_ENUM module)
{
	unsigned int pixel = DISP_REG_GET_FIELD(DEBUG_FLD_IN_PIXEL_CNT, DISP_REG_SPLIT_DEBUG);
	unsigned int state = DISP_REG_GET_FIELD(DEBUG_FLD_SPLIT_FSM, DISP_REG_SPLIT_DEBUG);

	DDPMSG("=DISP_PLIT DEBUG INFO=\n");

	DDPMSG("cur_pixel %u, state %s\n", pixel, split_state(state));
	return 0;
}

DDP_MODULE_DRIVER ddp_driver_split = {
	.module          = DISP_MODULE_SPLIT0,
	.init            = split_init,
	.deinit          = split_deinit,
	.config          = split_config,
	.start           = split_start,
	.trigger         = NULL,
	.stop            = split_stop,
	.reset           = split_reset,
	.power_on        = split_clock_on,
	.power_off       = split_clock_off,
	.is_idle         = split_idle,
	.is_busy         = split_busy,
	.dump_info       = split_dump,
	.bypass          = NULL,
	.build_cmdq      = NULL,
	.set_lcm_utils   = NULL,
};

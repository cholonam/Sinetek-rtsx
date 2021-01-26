#include "3rdParty/linux/drivers/misc/cardreader/rts_pcr.h"
#include "3rdParty/linux/include/linux/rtsx_pci.h"

#include "3rdParty/openbsd/rtsxvar.h" /* struct rtsx_softc */

#define UTL_THIS_CLASS ""
#include "util.h"

// See: https://github.com/torvalds/linux/blob/master/drivers/misc/cardreader/rts5249.c
static u8 aspm_en = 0;
static u8 sd30_drive_sel_1v8 = 0;
// Add a prefix because the symbol is public
u8 Sinetek_rtsx_3rdParty_linux_card_drive_sel = 0x41;
static u8 sd30_drive_sel_3v3 = 0;
static bool reverse_socket = 0;

int rtsx_read_cfg(struct rtsx_softc *sc, u_int8_t func, u_int16_t addr, u_int32_t *val);
int rtsx_write(struct rtsx_softc *sc, u_int16_t addr, u_int8_t mask, u_int8_t val);
int rtsx_write_phy(struct rtsx_softc *, u_int8_t, u_int16_t);

#define rtsx_pci_write_register                    rtsx_write
#define rtsx_pci_write_phy_register                rtsx_write_phy
#undef rtsx_pci_init_cmd
#define rtsx_pci_init_cmd(pcr)                     ((void)0)
#define rtsx_pci_add_cmd(pcr, cmd, reg, mask, val) rtsx_write(pcr, reg, mask, val)
#define rtsx_pci_send_cmd(pcr, to)                 0

static void rts5249_fill_driving(struct rtsx_pcr *pcr, u8 voltage)
{
	u8 driving_3v3[4][3] = {
		{0x13, 0x13, 0x13},
		{0x96, 0x96, 0x96},
		{0x7F, 0x7F, 0x7F},
		{0x96, 0x96, 0x96},
	};
	u8 driving_1v8[4][3] = {
		{0x99, 0x99, 0x99},
		{0xAA, 0xAA, 0xAA},
		{0xFE, 0xFE, 0xFE},
		{0xB3, 0xB3, 0xB3},
	};
	u8 (*driving)[3], drive_sel;

	if (voltage == OUTPUT_3V3) {
		driving = driving_3v3;
		drive_sel = /*pcr->*/sd30_drive_sel_3v3;
	} else {
		driving = driving_1v8;
		drive_sel = /*pcr->*/sd30_drive_sel_1v8;
	}

	rtsx_pci_add_cmd(pcr, WRITE_REG_CMD, SD30_CLK_DRIVE_SEL,
			0xFF, driving[drive_sel][0]);
	rtsx_pci_add_cmd(pcr, WRITE_REG_CMD, SD30_CMD_DRIVE_SEL,
			0xFF, driving[drive_sel][1]);
	rtsx_pci_add_cmd(pcr, WRITE_REG_CMD, SD30_DAT_DRIVE_SEL,
			0xFF, driving[drive_sel][2]);
}

void rtsx_base_fetch_vendor_settings(struct rtsx_softc *pcr)
{
	uint32_t reg;

	rtsx_read_cfg(pcr, 0, 0x724 /* PCR_SETTING_REG1 */, &reg);
	UTL_LOG("Cfg 0x%x: 0x%x\n", 0x724, reg);

	if (reg & 0x1000000) {
		UTL_LOG("skip fetch vendor setting\n");
		return;
	}

	aspm_en = (reg >> 28) & 0x03;
	sd30_drive_sel_1v8 = (reg >> 26) & 0x03;
	Sinetek_rtsx_3rdParty_linux_card_drive_sel &= 0x3F;
	Sinetek_rtsx_3rdParty_linux_card_drive_sel |= (((reg >> 25) & 0x01) << 6);
	UTL_LOG("aspm_en: %d, sd30_drive_sel_1v8: %d, card_drive_sel: 0x%02x",
		aspm_en,
		sd30_drive_sel_1v8,
		Sinetek_rtsx_3rdParty_linux_card_drive_sel);
	rtsx_read_cfg(pcr, 0, 0x814 /* PCR_SETTING_REG2 */, &reg);
	UTL_LOG("Cfg 0x%x: 0x%x\n", 0x814, reg);
	sd30_drive_sel_3v3 = (reg >> 5) & 0x03;
	if (reg & 0x4000)
		reverse_socket = true;
	UTL_LOG("sd30_drive_sel_3v3: %d, reverse_socket: %d",
		sd30_drive_sel_3v3,
		reverse_socket);
}


static void rts5249_init_from_cfg(struct rtsx_pcr *pcr)
{
#if 0
	struct rtsx_cr_option *option = &(pcr->option);
	u32 lval;

	if (CHK_PCI_PID(pcr, PID_524A))
		rtsx_pci_read_config_dword(pcr,
			PCR_ASPM_SETTING_REG1, &lval);
	else
		rtsx_pci_read_config_dword(pcr,
			PCR_ASPM_SETTING_REG2, &lval);

	if (lval & ASPM_L1_1_EN_MASK)
		rtsx_set_dev_flag(pcr, ASPM_L1_1_EN);

	if (lval & ASPM_L1_2_EN_MASK)
		rtsx_set_dev_flag(pcr, ASPM_L1_2_EN);

	if (lval & PM_L1_1_EN_MASK)
		rtsx_set_dev_flag(pcr, PM_L1_1_EN);

	if (lval & PM_L1_2_EN_MASK)
		rtsx_set_dev_flag(pcr, PM_L1_2_EN);

	if (option->ltr_en) {
		u16 val;

		pcie_capability_read_word(pcr->pci, PCI_EXP_DEVCTL2, &val);
		if (val & PCI_EXP_DEVCTL2_LTR_EN) {
			option->ltr_enabled = true;
			option->ltr_active = true;
			rtsx_set_ltr_latency(pcr, option->ltr_active_latency);
		} else {
			option->ltr_enabled = false;
		}
	}
#endif
}

static int rts5249_init_from_hw(struct rtsx_pcr *pcr)
{
#if 0
	struct rtsx_cr_option *option = &(pcr->option);

	if (rtsx_check_dev_flag(pcr, ASPM_L1_1_EN | ASPM_L1_2_EN
				| PM_L1_1_EN | PM_L1_2_EN))
		option->force_clkreq_0 = false;
	else
		option->force_clkreq_0 = true;
#endif
	return 0;
}

static int rts5249_extra_init_hw(struct rtsx_pcr *pcr)
{
#if 0
	struct rtsx_cr_option *option = &(pcr->option);
#endif

	rts5249_init_from_cfg(pcr);
	rts5249_init_from_hw(pcr);

	rtsx_pci_init_cmd(pcr);

	/* Rest L1SUB Config */
	rtsx_pci_add_cmd(pcr, WRITE_REG_CMD, L1SUB_CONFIG3, 0xFF, 0x00);
	/* Configure GPIO as output */
	rtsx_pci_add_cmd(pcr, WRITE_REG_CMD, GPIO_CTL, 0x02, 0x02);
	/* Reset ASPM state to default value */
	rtsx_pci_add_cmd(pcr, WRITE_REG_CMD, ASPM_FORCE_CTL, 0x3F, 0);
	/* Switch LDO3318 source from DV33 to card_3v3 */
	rtsx_pci_add_cmd(pcr, WRITE_REG_CMD, LDO_PWR_SEL, 0x03, 0x00);
	rtsx_pci_add_cmd(pcr, WRITE_REG_CMD, LDO_PWR_SEL, 0x03, 0x01);
	/* LED shine disabled, set initial shine cycle period */
	rtsx_pci_add_cmd(pcr, WRITE_REG_CMD, OLT_LED_CTL, 0x0F, 0x02);
	/* Configure driving */
	rts5249_fill_driving(pcr, OUTPUT_3V3);
	if (reverse_socket)
		rtsx_pci_add_cmd(pcr, WRITE_REG_CMD, PETXCFG, 0xB0, 0xB0);
	else
		rtsx_pci_add_cmd(pcr, WRITE_REG_CMD, PETXCFG, 0xB0, 0x80);

#if 0
	/*
	 * If u_force_clkreq_0 is enabled, CLKREQ# PIN will be forced
	 * to drive low, and we forcibly request clock.
	 */
	if (option->force_clkreq_0)
		rtsx_pci_add_cmd(pcr, WRITE_REG_CMD, PETXCFG,
			FORCE_CLKREQ_DELINK_MASK, FORCE_CLKREQ_LOW);
	else
		rtsx_pci_add_cmd(pcr, WRITE_REG_CMD, PETXCFG,
			FORCE_CLKREQ_DELINK_MASK, FORCE_CLKREQ_HIGH);
#endif

	return rtsx_pci_send_cmd(pcr, CMD_TIMEOUT_DEF);
}

int rts525a_optimize_phy(struct rtsx_softc *pcr)
{
	int err;

	err = rtsx_pci_write_register(pcr, RTS524A_PM_CTRL3, D3_DELINK_MODE_EN, 0x00);
	if (err < 0)
		return err;

	rtsx_pci_write_phy_register(pcr, _PHY_FLD0,
		_PHY_FLD0_CLK_REQ_20C | _PHY_FLD0_RX_IDLE_EN |
		_PHY_FLD0_BIT_ERR_RSTN | _PHY_FLD0_BER_COUNT |
		_PHY_FLD0_BER_TIMER | _PHY_FLD0_CHECK_EN);

	rtsx_pci_write_phy_register(pcr, _PHY_ANA03,
		_PHY_ANA03_TIMER_MAX | _PHY_ANA03_OOBS_DEB_EN |
		_PHY_CMU_DEBUG_EN);

	if (pcr->flags & RTSX_F_525A_TYPE_A)
		rtsx_pci_write_phy_register(pcr, _PHY_REV0,
			_PHY_REV0_FILTER_OUT | _PHY_REV0_CDR_BYPASS_PFD |
			_PHY_REV0_CDR_RX_IDLE_BYPASS);
	
	return 0;
}

int rts525a_extra_init_hw(struct rtsx_pcr *pcr)
{
	rts5249_extra_init_hw(pcr);

	rtsx_pci_write_register(pcr, PCLK_CTL, PCLK_MODE_SEL, PCLK_MODE_SEL);
	if (pcr->flags & RTSX_F_525A_TYPE_A) {
		rtsx_pci_write_register(pcr, L1SUB_CONFIG2,
			L1SUB_AUTO_CFG, L1SUB_AUTO_CFG);
		rtsx_pci_write_register(pcr, RREF_CFG,
			RREF_VBGSEL_MASK, RREF_VBGSEL_1V25);
		rtsx_pci_write_register(pcr, LDO_VIO_CFG,
			LDO_VIO_TUNE_MASK, LDO_VIO_1V7);
		rtsx_pci_write_register(pcr, LDO_DV12S_CFG,
			LDO_D12_TUNE_MASK, LDO_D12_TUNE_DF);
		rtsx_pci_write_register(pcr, LDO_AV12S_CFG,
			LDO_AV12S_TUNE_MASK, LDO_AV12S_TUNE_DF);
		rtsx_pci_write_register(pcr, LDO_VCC_CFG0,
			LDO_VCC_LMTVTH_MASK, LDO_VCC_LMTVTH_2A);
		rtsx_pci_write_register(pcr, OOBS_CONFIG,
			OOBS_AUTOK_DIS | OOBS_VAL_MASK, 0x89);
	}

	return 0;
}

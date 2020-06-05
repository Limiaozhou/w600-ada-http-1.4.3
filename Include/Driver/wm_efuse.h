/**
 * @file    wm_efuse.h
 *
 * @brief   virtual efuse Driver Module
 *
 * @author  dave
 *
 * Copyright (c) 2014 Winner Microelectronics Co., Ltd.
 */
#ifndef WM_EFUSE_H
#define WM_EFUSE_H

#define TLS_EFUSE_STATUS_OK      (0)
#define TLS_EFUSE_STATUS_EINVALID      (1)
#define TLS_EFUSE_STATUS_EIO      (2)
#define TLS_EFUSE_STATUS_ERR      (-1)


enum {
	CMD_MAC = 0x01,
	CMD_TX_DC,
	CMD_RX_DC,
	CMD_TX_IQ_GAIN,
	CMD_RX_IQ_GAIN,
	CMD_TX_IQ_PHASE,
	CMD_RX_IQ_PHASE,
	CMD_TX_GAIN,
	CMD_ALL,
};

#define VCG_ADDR  (FT_MAGICNUM_ADDR + sizeof(FT_PARAM_ST)+4)
#define VCG_LEN    (4)
//#define TX_GAIN_NEW_ADDR (VCG_ADDR+VCG_LEN)
#define TX_GAIN_LEN   (28*3)

/**
* @brief 	This function is used to init ft param.
*
* @param[in]	None
*
* @retval	 	TRUE			init success
* @retval		FALSE			init failed
*/
int tls_ft_param_init(void);


/**
* @brief 	This function is used to write ft_param.
*
* @param[in]	opnum  ft cmd
* @param[in]	data   data pointer
* @param[in]	len  the length written into "data"
*
* @retval	 	0		set success
* @retval		-1		set failed
*/
int tls_ft_param_set(unsigned int opnum, void *data, unsigned int len);

/**
* @brief 	This function is used to read ft_param.
*
* @param[in]	opnum  ft cmd
* @param[in]	data   data pointer
* @param[in]	len  len to read data
*
* @retval	 	0		get success
* @retval		-1		get failed
*/
int tls_ft_param_get(unsigned int opnum, void *data, unsigned int rdlen);


/**
* @brief 	This function is used to get mac addr
*
* @param[in]	mac		mac addr,6 byte
*
* @retval	 	0		get success
* @retval		-1		get failed
*/
int tls_get_mac_addr(u8 *mac);

/**
* @brief 	This function is used to set mac addr
*
* @param[in]	mac		mac addr,6 byte
*
* @retval	 	0		set success
* @retval		-1		set failed
*/
int tls_set_mac_addr(u8 *mac);

/**
* @brief 	This function is used to get tx gain
*
* @param[in]	txgain		tx gain,12 byte
*
* @retval	 	0		get success
* @retval		-1		get failed
*/
int tls_get_tx_gain(u8 *txgain);

/**
* @brief 	This function is used to set tx gain
*
* @param[in]	txgain		tx gain,12 byte
*
* @retval	 	0		set success
* @retval		-1		set failed
*/
int tls_set_tx_gain(u8 *txgain);

/**
* @brief 	This function is used to get tx lod
*
* @param[in]	txlo		tx lod
*
* @retval	 	0		get success
* @retval		-1		get failed
*/
int tls_get_tx_lo(u8 *txlo);

/**
* @brief 	This function is used to set tx lod
*
* @param[in]	txlo		tx lod
*
* @retval	 	0		set success
* @retval		-1		set failed
*/
int tls_set_tx_lo(u8 *txlo);

/**
* @brief 	This function is used to get tx iq gain
*
* @param[in]	txGain		
*
* @retval	 	0		get success
* @retval		-1		get failed
*/
int tls_get_tx_iq_gain(u8 *txGain);

/**
* @brief 	This function is used to set tx iq gain
*
* @param[in]	txGain		
*
* @retval	 	0		set success
* @retval		-1		set failed
*/
int tls_set_tx_iq_gain(u8 *txGain);

/**
* @brief 	This function is used to get rx iq gain
*
* @param[in]	rxGain		
*
* @retval	 	0		get success
* @retval		-1		get failed
*/
int tls_get_rx_iq_gain(u8 *rxGain);

/**
* @brief 	This function is used to set rx iq gain
*
* @param[in]	rxGain		
*
* @retval	 	0		set success
* @retval		-1		set failed
*/
int tls_set_rx_iq_gain(u8 *rxGain);

/**
* @brief 	This function is used to get tx iq phase
*
* @param[in]	txPhase		
*
* @retval	 	0		get success
* @retval		-1		get failed
*/
int tls_get_tx_iq_phase(u8 *txPhase);

/**
* @brief 	This function is used to set tx iq phase
*
* @param[in]	txPhase		
*
* @retval	 	0		set success
* @retval		-1		set failed
*/
int tls_set_tx_iq_phase(u8 *txPhase);

/**
* @brief 	This function is used to get rx iq phase
*
* @param[in]	rxPhase		
*
* @retval	 	0		get success
* @retval		-1		get failed
*/
int tls_get_rx_iq_phase(u8 *rxPhase);

/**
* @brief 	This function is used to set rx iq phase
*
* @param[in]	rxPhase		
*
* @retval	 	0		set success
* @retval		-1		set failed
*/
int tls_set_rx_iq_phase(u8 *rxPhase);

/**
* @brief 	This function is used to set/get freq err
*
* @param[in]	freqerr	
* @param[in]    flag  1-set  0-get
*/
int tls_freq_err_op(u8 *freqerr, u8 flag);

/**
* @brief 	This function is used to set/get vcg ctrl
*
* @param[in]	vcg	
* @param[in]    flag  1-set  0-get
*
*/
int tls_rf_vcg_ctrl_op(u8 *vcg, u8 flag);

/**
* @brief 	This function is used to get chip ID
*
* @param[out]	chip_id
*
*/
int tls_get_chipid(u8 chip_id[16]);

/**
* @brief 		This function is used for a delay of several seconds 		
*
* @param[in]	seconds
*
* @retval	 	0
*/
unsigned int tls_sleep(unsigned int seconds);

/**
* @brief 		This function is used for a delay of several milliseconds
*
* @param[in]	msec
*
* @retval	 	0
*/
int tls_msleep(unsigned int msec);
/**
* @brief 		This function is used for a delay of several microseconds
*
* @param[in]	usec
*
* @retval	 	0
*/
int tls_usleep(unsigned int usec);

#endif /* WM_EFUSE_H */


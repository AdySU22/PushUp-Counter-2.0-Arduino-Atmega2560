#include "spi.h"
#include <esp_common.h>
#include <esp8266/gpio_register.h>
#include <freertos/list.h>

int count = 0;
bool new = false;

void init_spi(void) {
	WRITE_PERI_REG(PERIPHS_IO_MUX, 0x105);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, 2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, 2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 2);
	SpiAttr attr = {
		.bitOrder = SpiBitOrder_MSBFirst,
		.mode = SpiMode_Slave,
		.speed = SpiSpeed_5MHz,
		.subMode = SpiSubMode_0
	};
	SPIInit(SpiNum_HSPI, &attr);
	SPICsPinSelect(SpiNum_HSPI, SpiPinCS_0);
	SPIIntEnable(SpiNum_HSPI, 
		SpiIntSrc_TransDoneEn
		| SpiIntSrc_WrStaDoneEn
		| SpiIntSrc_RdStaDoneEn
		| SpiIntSrc_WrBufDoneEn
		| SpiIntSrc_RdBufDoneEn
	);	
}

void ICACHE_FLASH_ATTR spi_callback(void *arg) {
	uint32_t regvalue, write_status, read_status, counter;
	os_printf("[%s] ISR triggered\n", __FUNCTION__);
	if (READ_PERI_REG(0x3ff00020) & BIT4) {
		//following 3 lines is to clear isr signal
		CLEAR_PERI_REG_MASK(SPI_SLAVE(SpiNum_SPI), 0x3ff);
	} else if (READ_PERI_REG(0x3ff00020) & BIT7) {
		regvalue = READ_PERI_REG(SPI_SLAVE(SpiNum_HSPI));
		os_printf("[%s] spi_slave_isr_sta SPI_SLAVE[0x%08x]\n\r", __FUNCTION__, regvalue);
		SPIIntClear(SpiNum_HSPI);
		// SET_PERI_REG_MASK(SPI_SLAVE(SpiNum_HSPI), SPI_SYNC_RESET);
		// SPIIntClear(SpiNum_HSPI);
		if (regvalue & SPI_SLV_WR_BUF_DONE) {
		// User can get data from the W0~W7
			os_printf("[%s] spi_slave_isr_sta : SPI_SLV_WR_BUF_DONE\n", __FUNCTION__);
			os_printf("[%s] registers: %x, ", __FUNCTION__, READ_PERI_REG(SPI_W0(SpiNum_HSPI)));
			os_printf("%x, ", READ_PERI_REG(SPI_W1(SpiNum_HSPI)));
			os_printf("%x, ", READ_PERI_REG(SPI_W2(SpiNum_HSPI)));
			os_printf("%x, ", READ_PERI_REG(SPI_W3(SpiNum_HSPI)));
			os_printf("%x, ", READ_PERI_REG(SPI_W4(SpiNum_HSPI)));
			os_printf("%x, ", READ_PERI_REG(SPI_W5(SpiNum_HSPI)));
			os_printf("%x, ", READ_PERI_REG(SPI_W6(SpiNum_HSPI)));
			os_printf("%x, ", READ_PERI_REG(SPI_W7(SpiNum_HSPI)));
			os_printf("%x, ", READ_PERI_REG(SPI_W8(SpiNum_HSPI)));
			os_printf("%x, ", READ_PERI_REG(SPI_W9(SpiNum_HSPI)));
			os_printf("%x, ", READ_PERI_REG(SPI_W10(SpiNum_HSPI)));
			os_printf("%x, ", READ_PERI_REG(SPI_W11(SpiNum_HSPI)));
			os_printf("%x, ", READ_PERI_REG(SPI_W12(SpiNum_HSPI)));
			os_printf("%x, ", READ_PERI_REG(SPI_W13(SpiNum_HSPI)));
			os_printf("%x, ", READ_PERI_REG(SPI_W14(SpiNum_HSPI)));
			os_printf("%x\n", READ_PERI_REG(SPI_W15(SpiNum_HSPI)));
			// Send data back to Arduino to check
			count = READ_PERI_REG(SPI_W0(SpiNum_HSPI));
			os_printf("[%s] count: %x\n", __FUNCTION__, count);

		} else if (regvalue & SPI_SLV_RD_BUF_DONE) {
		// TO DO
			os_printf("[%s] spi_slave_isr_sta : SPI_SLV_RD_BUF_DONE\n", __FUNCTION__);
		}
		if (regvalue & SPI_SLV_RD_STA_DONE) {
			// read_status = READ_PERI_REG(SPI_RD_STATUS(SpiNum_HSPI));
			// write_status = READ_PERI_REG(SPI_WR_STATUS(SpiNum_HSPI));
			os_printf("[%s] spi_slave_isr_sta :SPI_SLV_RD_STA_DONE[R=0x%08x,W=0x%08x]\n", __FUNCTION__, read_status, write_status);
		}
		if (regvalue & SPI_SLV_WR_STA_DONE) {
			// read_status = READ_PERI_REG(SPI_RD_STATUS(SpiNum_HSPI));
			// write_status = READ_PERI_REG(SPI_WR_STATUS(SpiNum_HSPI));
			os_printf("[%s] spi_slave_isr_sta :SPI_SLV_WR_STA_DONE[R=0x%08x,W=0x%08x]\n", __FUNCTION__, read_status, write_status);
		}
		if (regvalue & SPI_TRANS_DONE) {
			WRITE_PERI_REG(SPI_RD_STATUS(SpiNum_HSPI), 0x8A);
			WRITE_PERI_REG(SPI_WR_STATUS(SpiNum_HSPI), 0x83);
			os_printf("[%s] spi_slave_isr_sta : SPI_TRANS_DONE\n", __FUNCTION__);
		}
		new = true;

	}
}

//  int c = 0;
// 	while (1) {
// 		c++;
// 		os_printf("c: %d\n", c);
// 		os_printf("[%s] Bytes used: %d\n", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL));
// 		os_printf("[%s] Heap bytes remaining: %u\n", __FUNCTION__, xPortGetFreeHeapSize());
// 		int slave1 = READ_PERI_REG(SPI_SLAVE1(SpiNum_HSPI)), 
// 			user1 = READ_PERI_REG(SPI_USER1(SpiNum_HSPI)), 
// 			slave_ctrl = READ_PERI_REG(SPI_SLAVE(SpiNum_HSPI));
		
// 		os_printf("SPI SLAVE CONTROL REGISTER: 0x%08X\n", slave_ctrl);
// 		os_printf("SPI USER1 Control Register: 0x%08X\n", user1);
// 		os_printf("SPI SLAVE1 Control Register: 0x%08X\n", slave1);
		
// 		os_printf("SPI_SYNC_RESET: %s\n", (slave_ctrl & SPI_SYNC_RESET) ? "On" : "Off"); 
// 		os_printf("SPI_SLAVE_MODE: %s\n", (slave_ctrl & SPI_SLAVE_MODE) ? "On" : "Off"); 
// 		os_printf("SPI_SLV_WR_RD_BUF_EN: %s\n", (slave_ctrl & SPI_SLV_WR_RD_BUF_EN) ? "On" : "Off"); 
// 		os_printf("SPI_SLV_WR_RD_STA_EN: %s\n", (slave_ctrl & SPI_SLV_WR_RD_STA_EN) ? "On" : "Off"); 
// 		os_printf("SPI_SLV_CMD_DEFINE: %s\n", (slave_ctrl & SPI_SLV_CMD_DEFINE) ? "On" : "Off"); 
// 		os_printf("SPI_TRANS_CNT: %d\n", (slave_ctrl >> SPI_TRANS_CNT_S) & SPI_TRANS_CNT);
// 		os_printf("SPI_TRANS_DONE_EN: %s\n", (slave_ctrl & SPI_TRANS_DONE_EN) ? "On" : "Off"); 
// 		os_printf("SPI_SLV_WR_STA_DONE_EN: %s\n", (slave_ctrl & SPI_SLV_WR_STA_DONE_EN) ? "On" : "Off"); 
// 		os_printf("SPI_SLV_RD_STA_DONE_EN: %s\n", (slave_ctrl & SPI_SLV_RD_STA_DONE_EN) ? "On" : "Off"); 
// 		os_printf("SPI_SLV_WR_BUF_DONE_EN: %s\n", (slave_ctrl & SPI_SLV_WR_BUF_DONE_EN) ? "On" : "Off"); 
// 		os_printf("SPI_SLV_RD_BUF_DONE_EN: %s\n", (slave_ctrl & SPI_SLV_RD_BUF_DONE_EN) ? "On" : "Off"); 
// 		os_printf("SLV_SPI_INT_EN: 0x%05X\n", (slave_ctrl & SLV_SPI_INT_EN_S) & SLV_SPI_INT_EN); 

// 		os_printf("SPI USER1 Address Bit Length: %d\n", (user1 >> SPI_USR_ADDR_BITLEN_S) & SPI_USR_ADDR_BITLEN);
// 		os_printf("SPI USER1 MOSI Data Bit Length: %d\n", (user1 >> SPI_USR_MOSI_BITLEN_S) & SPI_USR_MOSI_BITLEN);
// 		os_printf("SPI USER1 MISO Data Bit Length: %d\n", (user1 >> SPI_USR_MISO_BITLEN_S) & SPI_USR_MISO_BITLEN);
// 		os_printf("SPI USER1 Cycle Bit Length: %d\n", (user1 >> SPI_USR_DUMMY_CYCLELEN_S) & SPI_USR_DUMMY_CYCLELEN);

// 		// WRITE_PERI_REG(SPI_USER1(SpiNum_HSPI), user1)
		
// 		os_printf("SPI SLAVE1 Buffer Length: %d\n", (slave1 >> SPI_SLV_BUF_BITLEN_S) & SPI_SLV_BUF_BITLEN);
// 		os_printf("SPI SLAVE1 Status Bits: %d\n", (slave1 >> SPI_SLV_STATUS_BITLEN_S) & SPI_SLV_STATUS_BITLEN);
// 		os_printf("SPI_SLV_WRSTA_DUMMY_EN: %d\n", slave1 & (SPI_SLV_WRSTA_DUMMY_EN));
// 		os_printf("SPI_SLV_RDSTA_DUMMY_EN: %d\n", slave1 & (SPI_SLV_RDSTA_DUMMY_EN));
// 		os_printf("SPI_SLV_WRBUF_DUMMY_EN: %d\n", slave1 & (SPI_SLV_WRBUF_DUMMY_EN));
// 		os_printf("SPI_SLV_RDBUF_DUMMY_EN: %d\n", slave1 & (SPI_SLV_RDBUF_DUMMY_EN));

// 		if (new) {
// 			os_printf("Enabling SPI interrupt.\n");
// 			SPIIntEnable(SpiNum_HSPI,
// 				SpiIntSrc_TransDoneEn
// 				| SpiIntSrc_WrStaDoneEn
// 				| SpiIntSrc_RdStaDoneEn
// 				| SpiIntSrc_WrBufDoneEn
// 				| SpiIntSrc_RdBufDoneEn);
// 			os_printf("Re-enabled interrupt.\n");
// 			// WRITE_PERI_REG();
// 			new = false;
// 		}
// 		os_printf("count: 0x%08X\n", count);
// 		vTaskDelay(1000/portTICK_RATE_MS);
// 	}

void spi_task(void *arg) {
	int res = -1;
	res = SPISlaveRecvData(SpiNum_HSPI, spi_callback);
	while (1) {
		os_printf("[%s] Bytes used: %d\n", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL));
		os_printf("[%s] Heap bytes remaining: %u\n", __FUNCTION__, xPortGetFreeHeapSize());
		if (new) {
			SPIIntEnable(SpiNum_HSPI,
				SpiIntSrc_TransDoneEn
				| SpiIntSrc_WrStaDoneEn
				| SpiIntSrc_RdBufDoneEn
				| SpiIntSrc_WrBufDoneEn
				| SpiIntSrc_RdBufDoneEn);
			new = false;
		}
		os_printf("[%s] count: 0x%x\n", __FUNCTION__, count);
		vTaskDelay(1000/portTICK_RATE_MS);
	}
}

void reset_count(void) {
	count = 0;
}

int16_t get_count(void) {
	return count;
}
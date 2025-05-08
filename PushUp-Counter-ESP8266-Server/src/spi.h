#ifndef SPI_H__
#define SPI_H__

#include <gpio.h>
#include <spi_flash.h>
#include <spi_register.h>
#include <spi_interface.h>

#if 0
// From spi_register.h, assuming REG_SPI_BASE(1) for HSPI
#define HSPI_BASE_ADDR                            (0x60000100) // (0x60000200 - 1*0x100)

// === SPI Command Register ===
// Controls overall SPI operation, initiates user transactions.
#define HSPI_CMD                                  (HSPI_BASE_ADDR + 0x00) // SPI_CMD(1)
// Bits for SPI_CMD:
#define SPI_USR                                   (1 << 18) // User defined transaction start. Cleared by HW on completion. (Master mode primarily)
// Other bits like SPI_FLASH_READ, SPI_FLASH_WREN etc. are for flash operations.

// === SPI Address Register ===
// Holds address for memory-mapped SPI devices (mostly flash in master mode).
#define HSPI_ADDR                                 (HSPI_BASE_ADDR + 0x04) // SPI_ADDR(1)

// === SPI Control Register ===
// General control settings.
#define HSPI_CTRL                                 (HSPI_BASE_ADDR + 0x08) // SPI_CTRL(1)
// Bits for SPI_CTRL:
#define SPI_WR_BIT_ORDER                          (1 << 26) // 0:MSB first, 1:LSB first (for MOSI)
#define SPI_RD_BIT_ORDER                          (1 << 25) // 0:MSB first, 1:LSB first (for MISO)
#define SPI_QIO_MODE                              (1 << 24) // Quad I/O mode
#define SPI_DIO_MODE                              (1 << 23) // Dual I/O mode
#define SPI_QOUT_MODE                             (1 << 20) // Quad Output mode
#define SPI_DOUT_MODE                             (1 << 14) // Dual Output mode
#define SPI_FASTRD_MODE                           (1 << 13) // Fast Read mode

// === SPI Control Register 1 ===
// More control settings, often for timing.
#define HSPI_CTRL1                                (HSPI_BASE_ADDR + 0x0C) // SPI_CTRL1(1)
// Bits like SPI_CS_HOLD_DELAY_S etc. for timing.

// === SPI Read Status Register ===
// Used by master to read status from a SPI device (typically flash).
// Note: SPI_RD_STATUS(i) in your SDK refers to a software-defined status register, not this hardware one.
#define HSPI_HW_RD_STATUS                         (HSPI_BASE_ADDR + 0x10) // SPI_RD_STATUS(1) (hardware one)

// === SPI Control Register 2 ===
// Advanced timing controls, mainly for master mode.
#define HSPI_CTRL2                                (HSPI_BASE_ADDR + 0x14) // SPI_CTRL2(1)
// Bits for CS delay, MOSI/MISO delay modes etc.

// === SPI Clock Control Register ===
// Configures SPI clock dividers for master mode. Slave mode adapts to master's clock.
#define HSPI_CLOCK                                (HSPI_BASE_ADDR + 0x18) // SPI_CLOCK(1)
// Bits:
#define SPI_CLK_EQU_SYSCLK                        (1 << 31) // SPI clk = system clk (80MHz)
#define SPI_CLKDIV_PRE_S                          18        // Pre-divider shift
#define SPI_CLKDIV_PRE                            (0x1FFF << SPI_CLKDIV_PRE_S) // Pre-divider mask
#define SPI_CLKCNT_N_S                            12        // N divider shift
#define SPI_CLKCNT_N                              (0x3F << SPI_CLKCNT_N_S)   // N divider mask
#define SPI_CLKCNT_H_S                            6         // H divider shift (clock high duration)
#define SPI_CLKCNT_H                              (0x3F << SPI_CLKCNT_H_S)   // H divider mask
#define SPI_CLKCNT_L_S                            0         // L divider shift (clock low duration)
#define SPI_CLKCNT_L                              (0x3F << SPI_CLKCNT_L_S)   // L divider mask

// === SPI User Control Register ===
// Defines phases of a user transaction (command, address, dummy, data). VERY IMPORTANT.
#define HSPI_USER                                 (HSPI_BASE_ADDR + 0x1C) // SPI_USER(1)
// Bits for SPI_USER:
#define SPI_USR_COMMAND                           (1 << 31) // Enable Command phase
#define SPI_USR_ADDR                              (1 << 30) // Enable Address phase
#define SPI_USR_DUMMY                             (1 << 29) // Enable Dummy phase
#define SPI_USR_MISO                              (1 << 28) // Enable MISO (DIN) phase
#define SPI_USR_MOSI                              (1 << 27) // Enable MOSI (DOUT) phase
#define SPI_USR_MOSI_HIGHPART                     (1 << 25) // MOSI data from W8-W15
#define SPI_USR_MISO_HIGHPART                     (1 << 24) // MISO data to W8-W15
#define SPI_CK_OUT_EDGE                           (1 << 7)  // Master Clock Polarity (CPHA for master, interacts with CPOL)
#define SPI_CK_I_EDGE                             (1 << 6)  // ** Slave Clock Input Edge (CPHA for slave) **
                                                          // 0: Slave samples on SCLK falling edge
                                                          // 1: Slave samples on SCLK rising edge
#define SPI_CS_SETUP                              (1 << 5)  // Enable CS setup time
#define SPI_CS_HOLD                               (1 << 4)  // Enable CS hold time
#define SPI_FLASH_MODE                            (1 << 2)  // Use for flash access mode (usually for SPI0)
// Note: SPI_DOUTDIN (Bit 0 for full-duplex) is also defined in esp8266_peri.h as SPIUDUPLEX

// === SPI User Control Register 1 ===
// Defines bit lengths for different phases of a user transaction.
#define HSPI_USER1                                (HSPI_BASE_ADDR + 0x20) // SPI_USER1(1)
// Bits for SPI_USER1:
#define SPI_USR_ADDR_BITLEN_S                     26        // Address length shift
#define SPI_USR_ADDR_BITLEN                       (0x3F << SPI_USR_ADDR_BITLEN_S) // Address length mask (0-63 bits)
#define SPI_USR_MOSI_BITLEN_S                     17        // MOSI data length shift
#define SPI_USR_MOSI_BITLEN                       (0x1FF << SPI_USR_MOSI_BITLEN_S) // MOSI data length mask (0-511 bits)
#define SPI_USR_MISO_BITLEN_S                     8         // MISO data length shift
#define SPI_USR_MISO_BITLEN                       (0x1FF << SPI_USR_MISO_BITLEN_S) // MISO data length mask (0-511 bits)
#define SPI_USR_DUMMY_CYCLELEN_S                  0         // Dummy cycle length shift
#define SPI_USR_DUMMY_CYCLELEN                    (0xFF << SPI_USR_DUMMY_CYCLELEN_S) // Dummy cycle length mask (0-255 cycles)

// === SPI User Control Register 2 ===
// Defines command value and length for user transaction.
#define HSPI_USER2                                (HSPI_BASE_ADDR + 0x24) // SPI_USER2(1)
// Bits for SPI_USER2:
#define SPI_USR_COMMAND_BITLEN_S                  28        // Command length shift
#define SPI_USR_COMMAND_BITLEN                    (0xF << SPI_USR_COMMAND_BITLEN_S) // Command length mask (0-15 bits)
#define SPI_USR_COMMAND_VALUE_S                   0         // Command value shift
#define SPI_USR_COMMAND_VALUE                     (0xFFFF << SPI_USR_COMMAND_VALUE_S) // Command value mask

// === SPI Write Status Register ===
// In master mode, data written here is sent during a "write status" command.
// In slave mode, master can write to this register if SPI_SLV_WR_RD_STA_EN is set.
// Your SDK uses SPI_WR_STATUS(i) to refer to this.
#define HSPI_WR_STATUS                            (HSPI_BASE_ADDR + 0x28) // SPI_WR_STATUS(1)

// === SPI Pin Control Register ===
// Controls CS pins, and often CPOL (Clock Polarity).
#define HSPI_PIN                                  (HSPI_BASE_ADDR + 0x2C) // SPI_PIN(1)
// Bits for SPI_PIN:
#define SPI_IDLE_EDGE                             (1 << 29) // Clock edge for idle state (CPOL)
                                                          // 0: SCLK idle low (CPOL=0)
                                                          // 1: SCLK idle high (CPOL=1)
#define SPI_CS2_DIS                               (1 << 2)  // Disable hardware CS2
#define SPI_CS1_DIS                               (1 << 1)  // Disable hardware CS1
#define SPI_CS0_DIS                               (1 << 0)  // Disable hardware CS0

// === SPI SLAVE CONTROL REGISTER (CRITICAL FOR SLAVE MODE) ===
// Configures slave mode operation and holds interrupt flags/enables.
#define HSPI_SLAVE                                (HSPI_BASE_ADDR + 0x30) // SPI_SLAVE(1)
// Bits for SPI_SLAVE:
// --- Control Bits ---
#define SPI_SYNC_RESET                            (1 << 31) // Write 1 to reset slave logic (auto-clears)
#define SPI_SLAVE_MODE                            (1 << 30) // Write 1 to enable SLAVE mode
#define SPI_SLV_WR_RD_BUF_EN                      (1 << 29) // Enable Slave Write/Read Buffer Mode
#define SPI_SLV_WR_RD_STA_EN                      (1 << 28) // Enable Slave Write/Read Status Mode
#define SPI_SLV_CMD_DEFINE                        (1 << 27) // Use SPI_SLAVE3 for command definitions
// --- Status Read Bits ---
#define SPI_TRANS_CNT_S                           23        // Transaction counter shift
#define SPI_TRANS_CNT                             (0xF << SPI_TRANS_CNT_S) // Transaction counter mask
// Other status like SPI_SLV_LAST_STATE, SPI_SLV_LAST_COMMAND also exist.
// --- Interrupt Enable Bits (Write 1 to enable) ---
#define SPI_TRANS_DONE_EN                         (1 << 9)  // SpiIntSrc_TransDoneEn
#define SPI_SLV_WR_STA_DONE_EN                    (1 << 8)  // SpiIntSrc_WrStaDoneEn
#define SPI_SLV_RD_STA_DONE_EN                    (1 << 7)  // SpiIntSrc_RdStaDoneEn
#define SPI_SLV_WR_BUF_DONE_EN                    (1 << 6)  // SpiIntSrc_WrBufDoneEn
#define SPI_SLV_RD_BUF_DONE_EN                    (1 << 5)  // SpiIntSrc_RdBufDoneEn
// --- Interrupt Status Bits (Read to check status, Write 1 to clear - W1TC) ---
#define SPI_TRANS_DONE                            (1 << 4)  // Transaction Done status
#define SPI_SLV_WR_STA_DONE                       (1 << 3)  // Write Status Done status
#define SPI_SLV_RD_STA_DONE                       (1 << 2)  // Read Status Done status
#define SPI_SLV_WR_BUF_DONE                       (1 << 1)  // Write Buffer Done status (Master wrote to slave)
#define SPI_SLV_RD_BUF_DONE                       (1 << 0)  // Read Buffer Done status (Master read from slave)

// === SPI Slave Control Register 1 ===
// Defines bit lengths for slave status and buffer, and dummy cycle enables.
#define HSPI_SLAVE1                               (HSPI_BASE_ADDR + 0x34) // SPI_SLAVE1(1)
// Bits for SPI_SLAVE1:
#define SPI_SLV_STATUS_BITLEN_S                   27
#define SPI_SLV_STATUS_BITLEN                     (0x1F << SPI_SLV_STATUS_BITLEN_S) // Status length (0-31 bits)
#define SPI_SLV_BUF_BITLEN_S                      16
#define SPI_SLV_BUF_BITLEN                        (0x1FF << SPI_SLV_BUF_BITLEN_S)  // Buffer length (0-511 bits)
#define SPI_SLV_WRSTA_DUMMY_EN                    (1 << 3)
#define SPI_SLV_RDSTA_DUMMY_EN                    (1 << 2)
#define SPI_SLV_WRBUF_DUMMY_EN                    (1 << 1)
#define SPI_SLV_RDBUF_DUMMY_EN                    (1 << 0)

// === SPI Slave Control Register 2 ===
// Defines dummy cycle lengths for slave operations.
#define HSPI_SLAVE2                               (HSPI_BASE_ADDR + 0x38) // SPI_SLAVE2(1)

// === SPI Slave Control Register 3 ===
// Defines command values for slave operations when SPI_SLV_CMD_DEFINE is set.
#define HSPI_SLAVE3                               (HSPI_BASE_ADDR + 0x3C) // SPI_SLAVE3(1)

// === SPI Data Buffer Registers (W0-W15) ===
// 16 x 32-bit registers (64 bytes total) for data transfer.
#define HSPI_W0                                   (HSPI_BASE_ADDR + 0x40) // SPI_W0(1)
#define HSPI_W1                                   (HSPI_BASE_ADDR + 0x44) // SPI_W1(1)
// ... up to ...
#define HSPI_W15                                  (HSPI_BASE_ADDR + 0x7C) // SPI_W15(1)

// === DPORT Register for CPU Interrupt Mapping/Status ===
// (Not an SPI peripheral register, but related to interrupt handling)
// From esp8266_peri.h: #define SPIIR ESP8266_DREG(0x20)
// Your code uses address 0x3FF00020 which is ESP8266_DREG(0x20)
#define DPORT_SPI_INT_REG                         (0x3FF00000 + 0x20)
// Bits in DPORT_SPI_INT_REG (DPORT_INT_STATUS_0 / DPORT_INT_ENABLE_0):
#define DPORT_SPI_INT_ST_HSPI_BIT                 (1 << 7)  // SPI1 (HSPI) interrupt status/enable at CPU level
#define DPORT_SPI_INT_ST_SPI_BIT                  (1 << 4)  // SPI0 (SPI) interrupt status/enable at CPU level

#endif // 0


void init_spi(void);
void spi_task(void *arg);
int16_t get_count(void);
void reset_count(void);

#endif // SPI_H__
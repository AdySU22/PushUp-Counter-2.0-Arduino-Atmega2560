#ifndef SPI_H__
#define SPI_H__

#include <gpio.h>
#include <spi_flash.h>
#include <spi_register.h>
#include <spi_interface.h>

void init_spi(void);
void spi_task(void *arg);

#endif // SPI_H__
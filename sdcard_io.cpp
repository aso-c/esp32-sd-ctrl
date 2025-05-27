/*
 * SD-card control classes
 * Implementation file
 * 	@file: sdcard_ctrl.cpp
 *	@author	Solomatov A.A. (aso)
 *	@date	Created 14.07.2022
 *		Updated 27.05.2025
 *	@version: 0.9.6.4
 *
 */

//#define __PURE_C__


#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG	// 4 - set 'DEBUG' logging level

#include <limits>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <cstdarg>

#include <cstring>
#include <cctype>
#include <sys/unistd.h>
#include <cerrno>
#include <esp_log.h>
#include <esp_console.h>
#include <esp_system.h>
#include <argtable3/argtable3.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <regex>
#include <fcntl.h>
#include <dirent.h>

#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>

#include <hal/spi_types.h>
#include <driver/sdspi_host.h>

#include "sdcard_io"
#include "sdspi_io"

#include "extrstream"
#include "astring.h"

//using namespace idf;
using namespace std;


//#define SD_MOUNT_POINT "/sdcard"


namespace SD //--------------------------------------------------------------------------------------------------------
{

    [[maybe_unused]]
    static const char * const TAG = "SD/MMC service";

//--[ strust SD::MMC::Slot ]-------------------------------------------------------------------------------------------

    /// @brief  Devault SD::MMC::Slot configuration
    const MMC::Slot MMC::Slot::def;

//--[ strust SD::MMC::Host ]-------------------------------------------------------------------------------------------


    MMC::Host::operator sdmmc_host_t&() {
	return instance;
    };

    MMC::Host::operator sdmmc_host_t*() {
	return &instance;
    };


    MMC::Host& MMC::Host::operator =(const Host& host)
    {
	*this = host.instance;
	return *this;
    }; /* SD::MMC::Host::operator =(const Host&) */

    MMC::Host& MMC::Host::operator =(const sdmmc_host_t& host)
    {
	// uint32_t flags;             /*!< flags defining host properties */
	instance.flags = host.flags;	/*!< flags defining host properties */
	//        int slot;                   /*!< slot number, to be passed to host functions */
	instance.slot = host.slot;	/*!< slot number, to be passed to host functions */
	//        int max_freq_khz;           /*!< max frequency supported by the host */
	instance.max_freq_khz = host.max_freq_khz;	/*!< max frequency supported by the host */
	//        float io_voltage;           /*!< I/O voltage used by the controller (voltage switching is not supported) */
	instance.io_voltage = host.io_voltage;	/*!< I/O voltage used by the controller (voltage switching is not supported) */
	//        esp_err_t (*init)(void);    /*!< Host function to initialize the driver */
	instance.init = host.init;	/*!< Host function to initialize the driver */
	//        esp_err_t (*set_bus_width)(int slot, size_t width);    /*!< host function to set bus width */
	instance.set_bus_width = host.set_bus_width;	/*!< host function to set bus width */
	//        size_t (*get_bus_width)(int slot); /*!< host function to get bus width */
	instance.get_bus_width = host.get_bus_width;	/*!< host function to get bus width */
	//        esp_err_t (*set_bus_ddr_mode)(int slot, bool ddr_enable); /*!< host function to set DDR mode */
	instance.set_bus_ddr_mode = host.set_bus_ddr_mode;	/*!< host function to set DDR mode */
	//        esp_err_t (*set_card_clk)(int slot, uint32_t freq_khz); /*!< host function to set card clock frequency */
	instance.set_card_clk = host.set_card_clk;	/*!< host function to set card clock frequency */
	//        esp_err_t (*do_transaction)(int slot, sdmmc_command_t* cmdinfo);    /*!< host function to do a transaction */
	instance.do_transaction = host.do_transaction;	/*!< host function to do a transaction */
	//        union {
	//            esp_err_t (*deinit)(void);  /*!< host function to deinitialize the driver */
	//            esp_err_t (*deinit_p)(int slot);  /*!< host function to deinitialize the driver, called with the `slot` */
	//        };
	instance.deinit = host.deinit;	/*!< host function to deinitialize the driver */
	//        esp_err_t (*io_int_enable)(int slot); /*!< Host function to enable SDIO interrupt line */
	instance.io_int_enable = host.io_int_enable;	/*!< Host function to enable SDIO interrupt line */
	//        esp_err_t (*io_int_wait)(int slot, TickType_t timeout_ticks); /*!< Host function to wait for SDIO interrupt line to be active */
	instance.io_int_wait = host.io_int_wait;	/*!< Host function to wait for SDIO interrupt line to be active */
	//        int command_timeout_ms;     /*!< timeout, in milliseconds, of a single command. Set to 0 to use the default value. */
	instance.command_timeout_ms = host.command_timeout_ms;	/*!< timeout, in milliseconds, of a single command. Set to 0 to use the default value. */
	return *this;
    }; /* SD::MMC::Host::operator =(const sdmmc_host_t&) */


    //--[ class SD::MMC::Host::mounter ]-------------------------------------------------------------------------------

    //
    // @brief Convenience function to get FAT filesystem on SD card registered in VFS
    //
    esp_err_t MMC::Host::mounter::operator()(std::string_view path, const FAT::mount::config& mount_config, Card& card) const
    {
  	return (phost->err = esp_vfs_fat_sdmmc_mount(path.data(), &phost->instance, &slot.cfg, &mount_config, &card.self));
    }; /* MMC::Host::mounter::operator() */


//--[ strust MMC::Slot ]-----------------------------------------------------------------------------------------------


//=================================================================================================
//
// ESP32’s SDMMC host peripheral has two slots. Each slot can be used independently to connect to an SD card, SDIO device or eMMC chip.
//
//    Slot 0 (SDMMC_HOST_SLOT_0) is an 8-bit slot. It uses HS1_* signals in the PIN MUX.
//
//    Slot 1 (SDMMC_HOST_SLOT_1) is a 4-bit slot. It uses HS2_* signals in the PIN MUX.
//
// The slots are connected to ESP32 GPIOs using IO MUX. Pin mappings of these slots are given in the table below.
//
// Signal    Slot 0	Slot 1
//
// CMD	    GPIO11	GPIO15
// CLK	    GPIO6	GPIO14
// D0	    GPIO7	GPIO2
// D1	    GPIO8	GPIO4
// D2	    GPIO9	GPIO12
// D3	    GPIO10	GPIO13
// D4	    GPIO16
// D5	    GPIO17
// D6	    GPIO5
// D7	    GPIO18
// CD	    any input via GPIO matrix
// WP	    any input via GPIO matrix
//
//=================================================================================================

    /// Assignment operators
    MMC::Slot& MMC::Slot::operator =(const Slot& slot)
    {
	*this = slot.cfg;
	return *this;
    }; /* SD::MMC::Slot::operator =(const Slot&) */

    MMC::Slot& MMC::Slot::operator =(const sdmmc_slot_config_t& config)
    {
#ifdef SOC_SDMMC_USE_GPIO_MATRIX
    //        gpio_num_t clk;         ///< GPIO number of CLK signal.
	cfg.clk = config.clk;	///< GPIO number of CLK signal.
    //        gpio_num_t cmd;         ///< GPIO number of CMD signal.
	cfg.cmd = config.cmd;	///< GPIO number of CMD signal.
    //        gpio_num_t d0;          ///< GPIO number of D0 signal.
	cfg.d0 = config.d0;	///< GPIO number of D0 signal.
    //        gpio_num_t d1;          ///< GPIO number of D1 signal.
	cfg.d1 = config.d1;	///< GPIO number of D1 signal.
    //        gpio_num_t d2;          ///< GPIO number of D2 signal.
	cfg.d2 = config.d2;	///< GPIO number of D2 signal.
    //        gpio_num_t d3;          ///< GPIO number of D3 signal.
	cfg.d3 = config.d3;	///< GPIO number of D3 signal.
    //        gpio_num_t d4;          ///< GPIO number of D4 signal. Ignored in 1- or 4- line mode.
	cfg.d4 = config.d4;	///< GPIO number of D4 signal. Ignored in 1- or 4- line mode.
    //        gpio_num_t d5;          ///< GPIO number of D5 signal. Ignored in 1- or 4- line mode.
	cfg.d5 = config.d5;	///< GPIO number of D5 signal. Ignored in 1- or 4- line mode.
    //        gpio_num_t d6;          ///< GPIO number of D6 signal. Ignored in 1- or 4- line mode.
	cfg.d6 = config.d6;	///< GPIO number of D6 signal. Ignored in 1- or 4- line mode.
    //        gpio_num_t d7;          ///< GPIO number of D7 signal. Ignored in 1- or 4- line mode.
	cfg.d7 = config.d7;	///< GPIO number of D7 signal. Ignored in 1- or 4- line mode.
#endif // SOC_SDMMC_USE_GPIO_MATRIX
	cfg.cd = config.cd;	///< card detect signal
	cfg.wp = config.wp;	///< write protect signal
	cfg.width = config.width;	///< Bus width used by the slot (might be less than the max width supported)
	cfg.flags = config.flags;         ///< Features used by this slot
	return *this;
    }; /* SD::MMC::Slot::operator =(const sdmmc_slot_config_t&) */


    MMC::Slot& MMC::Slot::operator =(sdmmc_slot_config_t&& config) noexcept
    {
	*this = config;
	return *this;
    }; /* SD::MMC::Slot::operator =(sdmmc_slot_config_t&&) */



//--[ struct SD::MMC::Device ]-----------------------------------------------------------------------------------------


    // Mount default SD-card slot onto path "mountpoint"
    esp_err_t MMC::Device::mount(Card& excard, std::string mountpoint)
    {
	ESP_LOGI(__PRETTY_FUNCTION__, "Mounting SD-Card to a mountpoint \"%s\"", mountpoint.c_str());
	// if card already mounted - exit with error
	if (mounted())
	{
	    ESP_LOGE(TAG, "%s: card already mounted at the %s, refuse to mount again", __func__, mountpath_c());
	    return ESP_ERR_NOT_SUPPORTED;
	}; /* if mounted() */

	mountpath(std::move(mountpoint));
	card  = &excard;

	ESP_LOGI(__PRETTY_FUNCTION__, "SD-Card mountpoint is set to a \"%s\"", mountpath_c());

	host.mount(mountpath(), mnt, *card);
	if (host.state() != ESP_OK)
	{
	    if (host.state() == ESP_FAIL)
		ESP_LOGI(TAG, "Failed to mount filesystem. %s",
			"If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
	    else
		ESP_LOGI(TAG, "Failed to initialize the card (error %d, %s). %s", ret,  esp_err_to_name(ret),
			"Make sure SD card lines have pull-up resistors in place.");
	    return host.state();
	}; /* if _host.state() != ESP_OK */

	ESP_LOGI(TAG, "Filesystem mounted at the %s", mountpath_c());

	return host.state();
    }; /* SD::MMC::Device::mount(Card&, const std::string&) */



    //esp_err_t unmount(const char mountpath[] = NULL);	// Unmount SD-card, that mounted onto "mountpath", default - call w/o mountpath
    //------------------------------------------------------------------------------------------
    //    // All done, unmount partition and disable SDMMC peripheral
    //    esp_vfs_fat_sdcard_unmount(mount_point, card);
    //    ESP_LOGI(TAG, "Card unmounted");
    //------------------------------------------------------------------------------------------

    // Unmount SD-card, that mounted onto "mountpath"
    esp_err_t MMC::Device::unmount()
    {
	// if card already mounted - exit with error
	if (!mounted())
	{
	    ESP_LOGE(TAG, "%s: mounted card is absent, nothing to unmount", __func__);
	    return ESP_ERR_NOT_FOUND;
	}; /* if card */

	host.unmount(mountpath(), *card);
	if (host.state() != ESP_OK)
	{
	    ESP_LOGE(TAG, "Error: %d, %s", host.state(), esp_err_to_name(ret));
	    return host.state();
	}; /* if _host.state() != ESP_OK */

	ESP_LOGI(TAG, "Card at %s unmounted", mountpath_c());
	card = nullptr;	// card is unmounted - clear this field as unmounted sign
	clean_mountpath();
	return host.state();
    }; /* SD::MMC::Device::unmount */

    //    esp_err_t unmount(sdmmc_card_t *card);	// Unmount SD-card "card", mounted onto default mountpath
    //    esp_err_t unmount(const char *base_path, sdmmc_card_t *card);	// Unmount mounted SD-card "card", mounted onto mountpath


//--[ class SD::Card ]-------------------------------------------------------------------------------------------------

#define CMD_TAG_PRFX "SD/MMC Card::"


    /// Print the card info to outfile
    /// print the SD-card info to a file (default - to stdout)
    esp_err_t  Card::info(FILE* outfile)
    {
	sdmmc_card_print_info(outfile, self);
	fprintf(outfile, "Sector: %d Bytes\n\n", self->csd.sector_size);
	return ESP_OK;
    }; /* SD::MMC::Card::print_info */


    const char* Card::TAG = "SD/MMC Card";

    /// Print CIS info to outfile -
    /// High level procedure
    esp_err_t Card::print_cis(FILE* outfile)
    {
	    size_t cisize = 0;
	    size_t bsize = 16;
	    std::vector<uint8_t> outbuf(bsize);
	    esp_err_t err;

	err = io.cis.data(outbuf, cisize);
	if (err != ESP_OK)
	{
	    ESP_LOGE("sdcard info command", "Error %i in get get CIS data first time: %s", err, esp_err_to_name(err));
	    switch (err)
	    {
	    case ESP_ERR_INVALID_RESPONSE:
		return err;
	    case ESP_ERR_INVALID_SIZE:	// CIS_CODE_END found, but buffer_size is less than required size, which is stored in the inout_cis_size then.
	    case ESP_ERR_NOT_FOUND:		// if the CIS_CODE_END not found. Increase input value of inout_cis_size or set it to 0,
		bsize = cisize;
		ESP_LOGD("sdcard info command", "Error %i in get get CIS data first time: %s", err, esp_err_to_name(err));
		ESP_LOGD("sdcard cis info command", "The new size of the CIS data buffer is: %i", bsize);
		cisize = 0;
		outbuf.resize(bsize);
		err = io.cis.data(outbuf, cisize);
	    }; /* switch err */
	}; /* if err != ESP_ERR_INVALID_SIZE */

	if (err != ESP_OK)
	{
	    //ESP_RETURN_ON_ERROR(err, "sdcard info command", "Error in the get CIS data");
	    ESP_LOGD("sdcard info command", "Error %i in the get CIS data: %s", err, esp_err_to_name(err));
	    return err;
	}; /* if err != ESP_OK */

	err = io.cis.info(outbuf, stdout);
	if (err != ESP_OK)
	    ESP_LOGD("sdcard info command", "Error %i in the print of the CIS info: %s", err, esp_err_to_name(err));

	return err;
    }; /* SD::Card::cis_info */


//--[ class IO ]--------------------------------------------------------------------------------------------------


}; //--[ namespace SD ]---------------------------------------------------------------------------------------------


//--[ sdcard_ctrl.cpp ]------------------------------------------------------------------------------------------------

/*
 * SD-card control classes
 * Implementation file
 * 	File: sdcard_ctrl.cpp
 *	Author:  aso (Solomatov A.A.)
 *	Created: 14.07.2022
 *	Version: 0.6
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
#ifdef __PURE_C__
#include <dirent.h>
#else
#if __cplusplus < 201703L
#include <fcntl.h>
#include <dirent.h>
#else	//  __cplusplus < 201703L
#include <fcntl.h>
#include <dirent.h>
#endif // __cplusplus < 201703L
#endif // ifdef __PURE_C__

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


/*
 * Предлагаемые команды:
 *    -	sd - main command for manipulation with SD-caed
 *	+ m, mount	- mount sdcard, options: [<card>] [<mountpoint>];
 *	+ u, umount	- unmount sdcard, options: [<card>|<mountpiont>];
 *	+ mkdir		- create new directory
 *	+ ls, dir	- list of files in sdcard, options: [<file pattern>];
 *	+ cd <dir>	- change dir;
 *	+ cat <file>	- print file to console
 *	+ type [<file>]	- type text to cinsile and store it in the file optionally
 *	+ cp, copy	- copy file, options: [<src file>|<dest file>];
 *	+ mv, move	- move or rename file, options: [<src file>|<dest file>];
 */

namespace SD //--------------------------------------------------------------------------------------------------------
{

    [[maybe_unused]]
    static const char *TAG = "SD/MMC service";


//--[ strust MMC::Host ]-----------------------------------------------------------------------------------------------


    /// Default constructor
    MMC::Host::Host(bus::width width, Pullup pullupst) // @suppress("Member declaration not found") // @suppress("Type cannot be resolved")
    {
	//slot.default_num(cfg.slot);
	ESP_LOGI(TAG, "Using SDMMC peripheral - default constructor");
	// Define my delay for SD/MMC command execution
	// cfg.command_timeout_ms = SDMMC_COMMAND_TIMEOUT;
	bus_width(width);
	set_pullup(pullupst);
    }; /* SD::MMC::Host::Host */

    /// Constructor with default slot configuration by number of the slot
    MMC::Host::Host(Slot::number number, bus::width width, Host::Pullup pullupst): // @suppress("Member declaration not found") // @suppress("Type cannot be resolved")
		_slot(number) // @suppress("Symbol is not resolved")
    {
	cfg.slot = number; // @suppress("Field cannot be resolved")
//	((sdmmc_slot_config_t*)_slot)->width = width;
	static_cast<sdmmc_slot_config_t*>(_slot)->width = width;
	bus_width(width);
	set_pullup(pullupst);
    }; /* SD::MMC::Host::Host(Slot::number, bus::width, Host::Pullup) */

    /// Custom slot configuration in temporary obj
    /// for desired slot number
    /// in lvalue object
    MMC::Host::Host(Slot::number num, const Slot& slot, bus::width width, Pullup pullupst):
		_slot(slot)
    {
	cfg.slot = num;
	((sdmmc_slot_config_t*)_slot)->width = width;
	bus_width(width);
	set_pullup(pullupst);
    }; /* SD::MMC::Host::Host(Slot::number, const Slot&) */

    /// in temporary object
    MMC::Host::Host(Slot::number num, Slot&& slot, bus::width width, Pullup pullupst):
		Host(num, slot, width, pullupst)
    {};

    /// Copy constructors
    /// for lvalue object (defined variable)
    MMC::Host::Host(const Host& host, bus::width width, Pullup pullupst):
		Host(host.cfg, width, pullupst)
    {
	_slot = host._slot;
    }; /* SD::MMC::Host::Host(const Host&) */

    /// for lvalue object (defined variable)
    ///Host(const sdmmc_host_t&, bus::width = bus::width_def, Pullup = nopullup);
    MMC::Host::Host(const sdmmc_host_t& host, bus::width width, Pullup pullupst)
    {
	cfg = host;
	if (host.slot == 0)
	    _slot = Slot(Slot::_0);
	bus_width(width);
	set_pullup(pullupst);
    }; /* SD::MMC::Host::Host(const sdmmc_host_t&) */

    /// for rvalue oblect (e.g. temporary object)
    MMC::Host::Host(sdmmc_host_t&& host, bus::width width, Pullup pullupst) noexcept:
		Host(host, width, pullupst)
    { };


    MMC::Host& MMC::Host::operator =(const Host& host)
    {
	*this = host.cfg;
	_slot = host._slot;
	return *this;
    }; /* SD::MMC::Host::operator =(const Host& */

    MMC::Host& MMC::Host::operator =(const sdmmc_host_t& host)
    {
	// uint32_t flags;             /*!< flags defining host properties */
	cfg.flags = host.flags;	/*!< flags defining host properties */
	//        int slot;                   /*!< slot number, to be passed to host functions */
	cfg.slot = host.slot;	/*!< slot number, to be passed to host functions */
	//        int max_freq_khz;           /*!< max frequency supported by the host */
	cfg.max_freq_khz = host.max_freq_khz;	/*!< max frequency supported by the host */
	//        float io_voltage;           /*!< I/O voltage used by the controller (voltage switching is not supported) */
	cfg.io_voltage = host.io_voltage;	/*!< I/O voltage used by the controller (voltage switching is not supported) */
	//        esp_err_t (*init)(void);    /*!< Host function to initialize the driver */
	cfg.init = host.init;	/*!< Host function to initialize the driver */
	//        esp_err_t (*set_bus_width)(int slot, size_t width);    /*!< host function to set bus width */
	cfg.set_bus_width = host.set_bus_width;	/*!< host function to set bus width */
	//        size_t (*get_bus_width)(int slot); /*!< host function to get bus width */
	cfg.get_bus_width = host.get_bus_width;	/*!< host function to get bus width */
	//        esp_err_t (*set_bus_ddr_mode)(int slot, bool ddr_enable); /*!< host function to set DDR mode */
	cfg.set_bus_ddr_mode = host.set_bus_ddr_mode;	/*!< host function to set DDR mode */
	//        esp_err_t (*set_card_clk)(int slot, uint32_t freq_khz); /*!< host function to set card clock frequency */
	cfg.set_card_clk = host.set_card_clk;	/*!< host function to set card clock frequency */
	//        esp_err_t (*do_transaction)(int slot, sdmmc_command_t* cmdinfo);    /*!< host function to do a transaction */
	cfg.do_transaction = host.do_transaction;	/*!< host function to do a transaction */
	//        union {
	//            esp_err_t (*deinit)(void);  /*!< host function to deinitialize the driver */
	//            esp_err_t (*deinit_p)(int slot);  /*!< host function to deinitialize the driver, called with the `slot` */
	//        };
	cfg.deinit = host.deinit;	/*!< host function to deinitialize the driver */
	//        esp_err_t (*io_int_enable)(int slot); /*!< Host function to enable SDIO interrupt line */
	cfg.io_int_enable = host.io_int_enable;	/*!< Host function to enable SDIO interrupt line */
	//        esp_err_t (*io_int_wait)(int slot, TickType_t timeout_ticks); /*!< Host function to wait for SDIO interrupt line to be active */
	cfg.io_int_wait = host.io_int_wait;	/*!< Host function to wait for SDIO interrupt line to be active */
	//        int command_timeout_ms;     /*!< timeout, in milliseconds, of a single command. Set to 0 to use the default value. */
	cfg.command_timeout_ms = host.command_timeout_ms;	/*!< timeout, in milliseconds, of a single command. Set to 0 to use the default value. */
	if (host.slot == 0)
	    _slot = Slot(Slot::_0);
	return *this;
    }; /* SD::MMC::Host::operator =(const sdmmc_host_t&) */


    ///esp_err_t Host::init(int slotno, const sdmmc_slot_config_t *slot_config)
    esp_err_t MMC::Host::init(Slot::number slotno, const Slot& extern_slot)
    {
	cfg.slot = slotno;
	_slot = extern_slot;
//	return sdmmc_host_init_slot(slot, slot_config);
	return init();
    }; /* SD::MMC::Host::init(Slot::number, const Slot&) */

    esp_err_t MMC::Host::init(Slot::number slotno, Slot&& extern_slot)
    {
	//cfg.slot = slotno;
	//slot = slot_config;
//	return sdmmc_host_init_slot(slot, &slot_config);
//	return init();
	return init(slotno, extern_slot);
    }; /* SD::MMC::Host::init(Slot::number, Slot&&) */


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


    MMC::Slot::Slot()
    {
	// To use 1-line SD mode, change this to 1:
    //	cfg.width = SLOT_WIDTH;

	// On chips where the GPIOs used for SD card can be configured, set them in

#if 0
	// the slot_config structure:
#ifdef SOC_SDMMC_USE_GPIO_MATRIX
	cfg.clk = GPIO_NUM_14;
	cfg.cmd = GPIO_NUM_15;
	cfg.d0 = GPIO_NUM_2;
	cfg.d1 = GPIO_NUM_4;
	cfg.d2 = GPIO_NUM_12;
	cfg.d3 = GPIO_NUM_13;
#endif
#endif

	// Enable internal pullups on enabled pins. The internal pullups
	// are insufficient however, please make sure 10k external pullups are
	// connected on the bus. This is for debug / example purpose only.
//	cfg.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    }; /* SD::MMC::Slot::Slot */

    //Slot();
    /// really copy constructor
    MMC::Slot::Slot(const Slot& slot):
	    Slot(slot.cfg)
    {};

    /// from struct sdmmc_slot_config_t copy constructor
    MMC::Slot::Slot(const sdmmc_slot_config_t& config)
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
//        union {
//            gpio_num_t gpio_cd;     ///< GPIO number of card detect signal
//            gpio_num_t cd;          ///< GPIO number of card detect signal; shorter name.
//        };
	cfg.cd = config.cd;	///< card detect signal
//        union {
//            gpio_num_t gpio_wp;     ///< GPIO number of write protect signal
//            gpio_num_t wp;          ///< GPIO number of write protect signal; shorter name.
//        };
	cfg.wp = config.wp;	///< write protect signal
//        uint8_t width;          ///< Bus width used by the slot (might be less than the max width supported)
	cfg.width = config.width;	///< Bus width used by the slot (might be less than the max width supported)
//        uint32_t flags;         ///< Features used by this slot
	cfg.flags = config.flags;         ///< Features used by this slot
//    #define SDMMC_SLOT_FLAG_INTERNAL_PULLUP  BIT(0)
            /**< Enable internal pullups on enabled pins. The internal pullups
             are insufficient however, please make sure external pullups are
             connected on the bus. This is for debug / example purpose only.
             */
    }; /* SD::MMC::Slot::Slot(const sdmmc_slot_config_t&) */

    /// temporary object copy constructor
    MMC::Slot::Slot(sdmmc_slot_config_t&& config):
	Slot(config)
    {};

    /// initializing Slot cfg by its number
    //enum number {_0 = SDMMC_HOST_SLOT_0, null=_0, _1 = SDMMC_HOST_SLOT_1, one=_1};
    MMC::Slot::Slot(number num)
    {
	switch (num)
	{
	// configurate slot to Slot0
	case null:

#ifdef SOC_SDMMC_USE_GPIO_MATRIX
	    //Signal Slot0	Slot 1
	    // CMD	GPIO11	GPIO15
	    cfg.cmd = GPIO_NUM_11;
	    // CLK	GPIO6	GPIO14
	    cfg.clk = GPIO_NUM_6;
	    // D0	GPIO7	GPIO2
	    cfg.d0 = GPIO_NUM_7;
	    // D1	GPIO8	GPIO4
	    cfg.d1 = GPIO_NUM_8;
	    // D2	GPIO9	GPIO12
	    cfg.d2 = GPIO_NUM_9;
	    // D3	GPIO10	GPIO13
	    cfg.d3 = GPIO_NUM_10;
	    // D4	GPIO16
	    cfg.d4 = GPIO_NUM_16;
	    // D5	GPIO17
	    cfg.d5 = GPIO_NUM_17;
	    // D6	GPIO5
	    cfg.d6 = GPIO_NUM_5;
	    // D7	GPIO18
	    cfg.d7 = GPIO_NUM_18;
	    // CD	any input via GPIO matrix
	    // WP	any input via GPIO matrix
#endif
	    // Other params - initialized by default
//	    cfg.cd = SDMMC_SLOT_NO_CD;
//	    cfg.wp = SDMMC_SLOT_NO_WP;
	//    cfg.width   = SDMMC_SLOT_WIDTH_DEFAULT;
	    //cfg.width   = bus::width_8;
//	    cfg.flags = 0;
	    break;

	// for Slot 1 - initialized by default values
	default:
	    ;
	}; /* switch num */
    }; /* SD::MMC::Slot::Slot(number) */

    /// Set or clear SDD::MMC internal pullup bit
    void MMC::Slot::internal_pullup(bool pullup)
    {
	if (pullup)
	    cfg.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;	///> set internal pullap bit
	else
	    cfg.flags &= ~SDMMC_SLOT_FLAG_INTERNAL_PULLUP;	///> clear internal pullup bit
    }; /* SD::MMC::Slot::internal_pullup */



    /// Assignment operators
    //    sdmmc_slot_config_t& operator =(const Slot&);
    MMC::Slot& MMC::Slot::operator =(const Slot& slot)
    {
	*this = slot.cfg;
	return *this;
    }; /* SD::MMC::Slot::operator =(const Slot&) */

    //    sdmmc_slot_config_t& operator =(const sdmmc_slot_config_t&);
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


//    sdmmc_slot_config_t& operator =(sdmmc_slot_config_t&&) noexcept;
    MMC::Slot& MMC::Slot::operator =(sdmmc_slot_config_t&& config) noexcept
    {
	*this = config;
	return *this;
    }; /* SD::MMC::Slot::operator =(sdmmc_slot_config_t&&) */



//--[ struct SD::MMC::Device ]-----------------------------------------------------------------------------------------


    MMC::Device::Device(bus::width width, Host::Pullup pullst, esp_vfs_fat_sdmmc_mount_config_t&& mnt_cfg):
		_host(width, pullst)
    {
	/*selective_log_level_set("Device::valid_path", ESP_LOG_DEBUG);*/	/* for debug purposes */
	mnt.format_if_mount_failed = mnt_cfg.format_if_mount_failed;
	mnt.max_files = mnt_cfg.max_files;
	mnt.allocation_unit_size = mnt_cfg.allocation_unit_size;
	//ESP_LOGI(TAG, "Initializing SD card");
    }; /* SD::MMC::Device::Device(bus::width, Host::Pullup, esp_vfs_fat_sdmmc_mount_config_t&) */

    MMC::Device::Device(Card::format::mntfail autofmt, int max_files, size_t size, bool disk_st_chk,
		    bus::width width, Host::Pullup pull):
		_host(width, pull)
    {
	/*selective_log_level_set("Device::valid_path", ESP_LOG_DEBUG);*/	/* for debug purposes */
	//ESP_LOGI(TAG, "Initializing SD card");
	mnt.format_if_mount_failed = (autofmt == Card::format::yes)? true: false;
        mnt.max_files = max_files;
        mnt.allocation_unit_size = size;
        mnt.disk_status_check_enable = disk_st_chk;
    }; /* SD::MMC::Device::Device(Card::format::mntfail, int, size_t, bus::width, Host::Pullup) */




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
//	ESP_LOGW(__PRETTY_FUNCTION__, "SD-Card is not mounted");

	mountpath(std::move(mountpoint));
	card  = &excard;

	ESP_LOGI(__PRETTY_FUNCTION__, "SD-Card mountpoint is set to a \"%s\"", mountpath_c());

	if ((ret = esp_vfs_fat_sdmmc_mount(mountpath_c(), _host, _host.slot(), &mnt, &card->self)) != ESP_OK)
	{
	    if (ret == ESP_FAIL)
		ESP_LOGI(TAG, "Failed to mount filesystem. %s",
			"If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
	    else
		ESP_LOGI(TAG, "Failed to initialize the card (error %d, %s). %s", ret,  esp_err_to_name(ret),
			"Make sure SD card lines have pull-up resistors in place.");
	    return ret;
	}; /* if (ret = esp_vfs_fat_sdmmc_mount(mountpath_c(), _host, _host.slot(), &mnt, &card->self)) != ESP_OK */

	ESP_LOGI(TAG, "Filesystem mounted at the %s", mountpath_c());

	return ret;
    }; /* SD::MMC::Device::mount(Card&, const std::string&) */



    //esp_err_t mount(Card&, int slot_no);		// Mount SD-card slot "slot_no" onto default mount path
    //esp_err_t mount(Card&, int slot_no, const char mountpoint[]);  // Mount SD-card slot "slot_no" onto default mount path

    // unmount();
    //esp_err_t unmount(const char mountpath[] = NULL);	// Unmount SD-card, that mounted onto "mountpath", default - call w/o mountpath
    //------------------------------------------------------------------------------------------
    //    // All done, unmount partition and disable SDMMC peripheral
    //    esp_vfs_fat_sdcard_unmount(mount_point, card);
    //    ESP_LOGI(TAG, "Card unmounted");
    //------------------------------------------------------------------------------------------

    // Unmount SD-card, that mounted onto "mountpath"
    esp_err_t MMC::Device::unmount()
    {

//	    esp_err_t ret;

	// if card already mounted - exit with error
	if (!mounted())
	{
	    ESP_LOGE(TAG, "%s: mounted card is absent, nothing to unmount", __func__);
	    return ESP_ERR_NOT_FOUND;
	}; /* if card */

	if ((ret = esp_vfs_fat_sdcard_unmount(mountpath_c(), card->self)) != ESP_OK)
	{
	    ESP_LOGE(TAG, "Error: %d, %s", ret, esp_err_to_name(ret));
	    return ret;
	}; /* if esp_vfs_fat_sdcard_unmount(mountpath_c(), card->self) != ESP_OK */

	ESP_LOGI(TAG, "Card at %s unmounted", mountpath_c());
	card = nullptr;	// card is unmounted - clear this field as unmounted sign
	clean_mountpath();
	return ret;
    }; /* SD::MMC::Device::unmount */

    //    esp_err_t unmount(sdmmc_card_t *card);	// Unmount SD-card "card", mounted onto default mountpath
    //    esp_err_t unmount(const char *base_path, sdmmc_card_t *card);	// Unmount mounted SD-card "card", mounted onto mountpath


//--[ class SD::MMC::Card ]--------------------------------------------------------------------------------------------

#define CMD_TAG_PRFX "SD/MMC Card::"


    /// Print the card info
    void MMC::Card::print_info(FILE* outfile)
    {
	sdmmc_card_print_info(outfile, self);
	fprintf(outfile, "Sector: %d Bytes\n\n", self->csd.sector_size);
    }; /* SD::MMC::Card::print_info */


    /// print the SD-card info (wrapper for the external caller)
    esp_err_t MMC::Card::info()
    {
	print_info(stdout);
	return ESP_OK;
    }; /* SD::MMC::Card::info */

    const char* MMC::Card::TAG = "SD/MMC Card";

    /// Print CIS info to outfile -
    /// High level procedure
    esp_err_t MMC::Card::cis_info(FILE* outfile)
    {
	    size_t cisize = 0;
	    size_t bsize = 16;
	    uint8_t* outbuf = (uint8_t*)malloc(bsize);
	    esp_err_t err;

	err = io.get_cis_data(outbuf, bsize, &cisize); // @suppress("Method cannot be resolved") // @suppress("Field cannot be resolved")
	if (err != ESP_OK)
	{
	    ESP_LOGE("sdcard info command", "Error %i in get get CIS data first time: %s", err, esp_err_to_name(err));
	    free(outbuf);
	    switch (err)
	    {
	    case ESP_ERR_INVALID_RESPONSE:
		return err;
	    case ESP_ERR_INVALID_SIZE:	// CIS_CODE_END found, but buffer_size is less than required size, which is stored in the inout_cis_size then.
	    case ESP_ERR_NOT_FOUND:		// if the CIS_CODE_END not found. Increase input value of inout_cis_size or set it to 0,
		bsize = cisize;
//	 	cout << aso::format("The new size of the CIS data buffer is: %i") % bsize << endl;
		ESP_LOGD("sdcard info command", "Error %i in get get CIS data first time: %s", err, esp_err_to_name(err));
		ESP_LOGD("sdcard cis info command", "The new size of the CIS data buffer is: %i", bsize);
		cisize = 0;
		outbuf = (uint8_t*)malloc(bsize);
		err = io.get_cis_data(outbuf, bsize, &cisize); // @suppress("Field cannot be resolved") // @suppress("Method cannot be resolved")
	    }; /* switch err */
	}; /* if err != ESP_ERR_INVALID_SIZE */

	if (err != ESP_OK)
	{
	    //ESP_RETURN_ON_ERROR(err, "sdcard info command", "Error in the get CIS data");
	    ESP_LOGD("sdcard info command", "Error %i in the get CIS data: %s", err, esp_err_to_name(err));
	    return err;
	}; /* if err != ESP_OK */

	err = io.print_cis_info(outbuf, bsize, stdout); // @suppress("Method cannot be resolved") // @suppress("Field cannot be resolved")
	free(outbuf);
	if (err != ESP_OK)
	    ESP_LOGD("sdcard info command", "Error %i in the print of the CIS info: %s", err, esp_err_to_name(err));

	return err;
    }; /* SD::MMC::Card::cis_info */



//--[ class IO ]--------------------------------------------------------------------------------------------------



}; /* namespace SD */  //----------------------------------------------------------------------------------------------


//--[ sdcard_ctrl.cpp ]----------------------------------------------------------------------------

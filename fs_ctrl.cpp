/*!
 * @brief Filesystem on storege device (SD-card etc) control/navigation classes
 * Implementation file
 * 	@file: fs_ctrl.cpp
 *	@author: Solomatov A.A. aso
 *	@date 14.07.2022 - 27.04.2024
 *	@version: 0.7
 */

//#define __PURE_C__


#include <limits>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <cstdarg>

#include <string>
#include <cstring>
#include <cctype>
#include <sys/unistd.h>
#include <cerrno>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_console.h>
#include <esp_system.h>
#include <argtable3/argtable3.h>
#include <sys/stat.h>
#include <sys/types.h>
//#include <unistd.h>
#include <regex>
//#ifdef __PURE_C__
////#include <fcntl.h>
//#include <dirent.h>
//#else
  //#if __cplusplus < 201703L
#ifndef __PURE_C__
#include <fcntl.h>
#endif // ifndef __PURE_C__
#include <dirent.h>
  //#else
  //#endif // __cplusplus < 201703L
//#endif // ifdef __PURE_C__

#include <esp_vfs_fat.h>
#include "sdkconfig.h"



#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>

#include <cwd_emulate>
#include <sdcard_io>
#include "fs_ctrl"


#include <extrstream>
#include <astring.h>

//using namespace idf;
using namespace std;


#define SD_MOUNT_POINT CONFIG_UNIT_SD_CARD_MOUNT_POINT //"/sdcard"


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

namespace Exec	//-----------------------------------------------------------------------------------------------------
{

//--[ instance of the cwd_emulation ]----------------------------------------------------------------------------------
//	char cwd_path_buff[PATH_MAX] = "";	// simulation current dir at this device
	fs::CWD_emulating fake_cwd(/*cwd_path_buff, sizeof(cwd_path_buff)*/SD_MOUNT_POINT);


//    static const char *TAG = "SD/MMC service";

//--[ class Server ]------------------------------------------------------------------------------------------------



const char* const Server::MOUNT_POINT_Default = SD_MOUNT_POINT;


#undef CMD_TAG_PRFX
#define CMD_TAG_PRFX "SD/MMC CMD server:"


    /// Mount default SD-card slot onto path "mountpoint", default mountpoint is MOUNT_POINT_Default
    esp_err_t Server::mount(SDMMC::Device& device, SDMMC::Card& card, const std::string& mountpoint) // @suppress("Type cannot be resolved") // @suppress("Member declaration not found")
    {
	ESP_LOGI(TAG, "Mounting SD-Cart to a mountpoint %s", mountpoint.c_str());

	if (astr::is_digitex(mountpoint))
	    return mount(device, card, atoi(mountpoint.c_str())); // @suppress("Invalid arguments")

	ESP_LOGI(TAG, "Mounting SD-Cart to a directory!!!");
	ret = device.mount(card, mountpoint.c_str()); // @suppress("Method cannot be resolved")
	if (ret == ESP_OK)
	{
#ifdef CONFIG_AUTO_CHDIR_BEHIND_MOUNTING
	    fake_cwd.change(mountpoint.c_str());
	    ESP_LOGI(TAG, "Current directory autochanged to: %s", fake_cwd.current().c_str());
#else
//	    change_currdir("/");
	    fake_cwd.get(fake_cwd_path, sizeof(fake_cwd_path));	// set fake_cwd according system pwd (through get_cwd())
	    ESP_LOGI(TAG, "Current directory set to: %s,  according system pwd", fake_cwd.current());
#endif
	}; /* if ret == ESP_OK */
	return ret;
    }; /* Server::mount */


    /// Mount SD-card slot "slot_no" onto specified mount path, default mountpoint is MOUNT_POINT_Default
    esp_err_t Server::mount(SDMMC::Device& device, SDMMC::Card& card, int slot_no, const std::string& mountpoint) // @suppress("Member declaration not found") // @suppress("Type cannot be resolved")
    {
	device.slot_no(slot_no); // @suppress("Method cannot be resolved")
	return device.mount(card, mountpoint.c_str()); // @suppress("Method cannot be resolved")
    }; /* Server::mount */


    //------------------------------------------------------------------------------------------
    //    // All done, unmount partition and disable SDMMC peripheral
    //    esp_vfs_fat_sdcard_unmount(mount_point, card);
    //    ESP_LOGI(TAG, "Card unmounted");
    //------------------------------------------------------------------------------------------

    // Unmount SD-card, that mounted onto "mountpath"
    esp_err_t Server::unmount(SDMMC::Device& device/*const char mountpath[]*/) // @suppress("Type cannot be resolved") // @suppress("Member declaration not found")
    {
//	ret = device.unmount(); // @suppress("Method cannot be resolved")
	//ESP_LOGI(TAG, "Card unmounted");
//	if (ret != ESP_OK)
	if ((ret = device.unmount()) != ESP_OK) // @suppress("Method cannot be resolved")
	    cout << TAG << ": "  << "Unmounting Error: " << ret
		<< ", " << esp_err_to_name(ret) << endl;
	else
	{
	    cout << TAG << ": " << "Card unmounted" << endl;
	    fake_cwd.clear();	// set fake cwd path to: ""
	}; /* if device.unmount() != ESP_OK */

	return ret;
    }; /* Server::unmount */

    //------------------------------------------------------------------------------------------
    //    // All done, unmount partition and disable SDMMC peripheral
    //    esp_vfs_fat_sdcard_unmount(mount_point, card);
    //    ESP_LOGI(TAG, "Card unmounted");
    //------------------------------------------------------------------------------------------

//    // Unmount SD-card "card", mounted onto default mountpath
//    esp_err_t Server::unmount(sdmmc_card_t *card)
//    {
//	cout << TAG << ": " << "Procedure \"Unmount(<card>)\" is not yet released now" << endl;
//	cout << "Exit..." << endl;
//	return ESP_ERR_INVALID_VERSION;
//    }; /* Server::unmount */
//
//    // Unmount mounted SD-card "card", mounted onto mountpath
//    esp_err_t Server::unmount(const char *base_path, sdmmc_card_t *card)
//    {
//	cout << TAG << ": " << "Procedure \"Unmount(<mountpath, ><card>)\" is not yet released now" << endl;
//	cout << "Exit..." << endl;
//	return ESP_ERR_INVALID_VERSION;
//    }; /* Server::unmount */


    // print current directory name
    esp_err_t Server::pwd(SDMMC::Device& device)
    {
#if __cplusplus < 201703L

	    const std::string buf = fake_cwd.get();

	cout << endl
	    << "PWD is: \"" << buf << '"' << endl
	    << endl;

	return ESP_OK;
#else	// __cplusplus < 201703L

	    const std::string buf = fake_cwd.get();

	if (astr::is_space(buf))
	    return errno;
	cout << endl
	    << "PWD is: \"" << buf << '"' << endl
	    << endl;

	return ESP_OK;
#endif	// __cplusplus < 201703L
    }; /* Server::pwd */



#define CMD_NM "mkdir"
    // create a new directory
    esp_err_t Server::mkdir(SDMMC::Device& device, const std::string& dirname)
    {
	if (!fake_cwd.valid(dirname.c_str()))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: the new directory name \"%s\" is invalid", __func__, dirname.c_str());
	    return ESP_ERR_NOT_FOUND;
	}; /* !device.valid_path(pattern) */

	if (astr::is_space(dirname))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: invoke command \"%s\" without parameters.\n%s", __func__, CMD_NM,
		    "This command required the creating directory name.");
	    return ESP_ERR_INVALID_ARG;
	}; /* if dirname == NULL || strcmp(dirname, "") */

#if __cplusplus < 201703L

	    struct stat statbuf;
	    std::string path = fake_cwd.compose(dirname/*.c_str()*/);

	ESP_LOGI(CMD_TAG_PRFX, "%s: Create directory with name \"%s\", real path is %s", __func__, dirname.c_str(), path.c_str());

	if (stat(path.c_str(), &statbuf) == 0)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Invalid argument - requested path \"%s\" is exist; denied create duplication name\n", __func__, path.c_str());
	    return ESP_ERR_INVALID_ARG;
	}; /* if stat(tmpstr, &statbuf) == -1 */
	errno = 0;
	::mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
	if (errno)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Error creating directory \"%s\": %s", __func__, dirname.c_str(), strerror(errno));
	    return ESP_FAIL;
	}; /* if (errno) */
	return ESP_OK;

#else	// __cplusplus < 201703L

	    struct stat statbuf;
	    std::string path = fake_cwd.compose(dirname/*.c_str()*/);

	    ESP_LOGI(CMD_TAG_PRFX, "%s: Create directory with name \"%s\", real path is %s", __func__, dirname.c_str(), path.c_str());

	if (stat(path.c_str(), &statbuf) == 0)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Invalid argument - requested path \"%s\" is exist; denied create duplication name\n", __func__, path.c_str());
	    return ESP_ERR_INVALID_ARG;
	}; /* if stat(tmpstr, &statbuf) == -1 */
	errno = 0;
	::mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
	if (errno)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Error creating directory \"%s\": %s", __func__, dirname.c_str(), strerror(errno));
	    return ESP_FAIL;
	}; /* if (errno) */
	return ESP_OK;

#endif	// __cplusplus < 201703L
    }; /* Server::mkdir */

#undef CMD_NM
#define CMD_NM "rmdir"
// delete empty directory
    esp_err_t Server::rmdir(SDMMC::Device& device, const std::string& dirname)
    {
	if (!fake_cwd.valid(dirname.c_str()))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: the directory name \"%s\" is invalid", __func__, dirname.c_str());
	    return ESP_ERR_NOT_FOUND;
	}; /* !device.valid_path(pattern) */

	if (astr::is_space(dirname))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: invoke command \"%s\" without parameters.\n%s", __func__, CMD_NM,
		     "This command required the name of the deleting directory.");
	    return ESP_ERR_INVALID_ARG;
	}; /* if dirname == NULL || strcmp(dirname, "") */
#if __cplusplus < 201703L

	    struct stat st;
	    std::string path = fake_cwd.compose(dirname/*.c_str()*/);

	ESP_LOGI(CMD_TAG_PRFX, "%s: Delete directory <%s>, real path is %s", __func__, dirname.c_str(), path.c_str());

	// Check if destination directory or file exists before deleting
	if (stat(path.c_str(), &st) != 0)
	{
	    // deleting a non-exist directory is not possible
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Directory \"%s\" is not exist - deleting a non-existent catalogue is not possible.\n%s", __func__, dirname.c_str(), esp_err_to_name(ESP_ERR_NOT_FOUND));
	    return ESP_ERR_NOT_FOUND;
	}; /* if stat(file_foo, &st) != 0 */
	if (!S_ISDIR(st.st_mode))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: The %s command delete directories, not the files.\n%s", __func__, CMD_NM, esp_err_to_name(ESP_ERR_NOT_SUPPORTED));
	    return ESP_ERR_INVALID_ARG;
	}; /* if (S_ISDIR(st.st_mode)) */

	    DIR *dir = opendir(path.c_str());	// Directory descriptor

	errno = 0;	// clear any possible errors

	    struct dirent *entry = readdir(dir);

	closedir(dir);
	if (errno)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Fail when closing directory \"%s\": %s", __func__, dirname.c_str(), strerror(errno));
	    return ESP_FAIL;
	}; /* if errno */
	if (entry)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Directory \"%s\" is not empty, deletung non-emty directories "
		    "is not supported.", __func__, dirname.c_str());
	    return ESP_ERR_NOT_SUPPORTED;
	}; /* if (entry) */

	errno = 0;
	//cout << aso::format("[[[ unlink the path [%s] ]]]") % path << endl;
	unlink(path.c_str());
	if (errno)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Fail when deleting \"%s\": %s", __func__, dirname.c_str(), strerror(errno));
	    return ESP_FAIL;
	}; /* if errno */

	return ESP_OK;

#else	// __cplusplus < 201703L
	    struct stat st;
	    std::string path = fake_cwd.compose(dirname/*.c_str()*/);

	ESP_LOGI(CMD_TAG_PRFX, "%s: Delete directory <%s>, real path is %s", __func__, dirname.c_str(), path.c_str());

	// Check if destination directory or file exists before deleting
	if (stat(path.c_str(), &st) != 0)
	{
	    // deleting a non-exist directory is not possible
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Directory \"%s\" is not exist - deleting a non-existent catalogue is not possible.\n%s", __func__, dirname.c_str(), esp_err_to_name(ESP_ERR_NOT_FOUND));
	    return ESP_ERR_NOT_FOUND;
	}; /* if stat(file_foo, &st) != 0 */
	if (!S_ISDIR(st.st_mode))
	{
	ESP_LOGE(CMD_TAG_PRFX, "%s: The %s command delete directories, not the files.\n%s", __func__, CMD_NM, esp_err_to_name(ESP_ERR_NOT_SUPPORTED));
	return ESP_ERR_INVALID_ARG;
	}; /* if (S_ISDIR(st.st_mode)) */

	    DIR *dir = opendir(path.c_str());	// Directory descriptor

	errno = 0;	// clear any possible errors

	    struct dirent *entry = readdir(dir);

	closedir(dir);
	if (errno)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Fail when closing directory \"%s\": %s", __func__, dirname.c_str(), strerror(errno));
	    return ESP_FAIL;
	}; /* if errno */
	if (entry)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Directory \"%s\" is not empty, deletung non-emty directories "
		    "is not supported.", __func__, dirname.c_str());
	    return ESP_ERR_NOT_SUPPORTED;
	}; /* if (entry) */

	errno = 0;
	//cout << aso::format("[[[ unlink the path [%s] ]]]") % path << endl;
	unlink(path.c_str());
	if (errno)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Fail when deleting \"%s\": %s", __func__, dirname.c_str(), strerror(errno));
	    return ESP_FAIL;
	}; /* if errno */

	return ESP_OK;

#endif // __cplusplus < 201703L

    }; /* Server::rmdir */



#undef CMD_NM
#define CMD_NM "cd"

    // change a current directory
    esp_err_t Server::cd(SDMMC::Device& device, const std::string& dirname)
    {
	    esp_err_t err;

	if (!fake_cwd.valid(dirname.c_str()))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: the directory name \"%s\" is invalid", __func__, dirname.c_str());
	    return ESP_ERR_NOT_FOUND;
	}; /* !device.valid_path(pattern) */

#if __cplusplus < 201703L

	if (!astr::is_space(dirname))
	    ESP_LOGI(CMD_TAG_PRFX, "%s: Change current dir to %s", __func__, dirname.c_str());
	else if (device.card != nullptr)
	    ESP_LOGI(CMD_TAG_PRFX, "%s: Not specified directory for jump to, change current dir to %s, [mountpoint].", __func__, device.mountpath_c());
	else
	{
    	    ESP_LOGW(CMD_TAG_PRFX, "%s: Card is not mounted, mountpoint is not valid, nothing to do", __func__);
    	    return ESP_ERR_NOT_SUPPORTED;
	}; /* else if device.card != nullptr */
	// change cwd dir: chdir(dirname);
	err = device.mounted()? fake_cwd.change((astr::is_space(dirname))? device.mountpath_c(): dirname.c_str()): ESP_FAIL;

	if (err != 0)
	    ESP_LOGE(CMD_TAG_PRFX, "%s: fail change directory to %s\n%s", __func__, dirname.c_str(), esp_err_to_name(err));
	return err;

#else	// __cplusplus < 201703L

	if (!astr::is_space(dirname))
	    ESP_LOGI(CMD_TAG_PRFX, "%s: Change current dir to %s", __func__, dirname.c_str());
	else if (device.card != nullptr)
	    ESP_LOGI(CMD_TAG_PRFX, "%s: Not specified directory for jump to, change current dir to %s, [mountpoint].", __func__, device.mountpath_c());
	else
	{
    	    ESP_LOGW(CMD_TAG_PRFX, "%s: Card is not mounted, mountpoint is not valid, nothing to do", __func__);
    	    return ESP_ERR_NOT_SUPPORTED;
	}; /* else if device.card != nullptr */
	// change cwd dir: chdir(dirname);
	err = device.mounted()? fake_cwd.change((astr::is_space(dirname))? device.mountpath_c(): dirname.c_str()): ESP_FAIL;

	if (err != 0)
	    ESP_LOGE(CMD_TAG_PRFX, "%s: fail change directory to %s\n%s", __func__, dirname.c_str(), esp_err_to_name(err));
	return err;

#endif	// __cplusplus < 201703L
    }; /* Server::cd */



#undef CMD_NM
#define CMD_NM "ls"

    //
    // Listing the entries of the opened directory
    // Parameters:
    //	dir  - opened directory stream;
    //	path - full path of this directory
    // Return:
    //	>=0 - listed entries counter;
    //	<0  - error - -1*(ESP_ERR_xxx) or ESP_FAIL (-1) immediately
    // C++ edition
    static int listing_direntries_Cpp(DIR *dir, const std::string& path);


    // print a list of files in the specified directory
    esp_err_t Server::ls(SDMMC::Device& device, const std::string& pattern)
    {
	ESP_LOGD(CMD_TAG_PRFX, "%s: pattern is             : \"%s\"", __func__, pattern.c_str());
	ESP_LOGD(CMD_TAG_PRFX, "%s: processed inner pattern: \"%s\"", __func__, fake_cwd.compose(pattern/*.c_str()*/).c_str());
	if (!fake_cwd.valid(pattern.c_str()))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: pattern \"%s\" is invalid", __func__, pattern.c_str());
	    return ESP_ERR_NOT_FOUND;
	}; /* !device.valid_path(pattern) */

    	    int entry_cnt = 0;
    	    DIR *dir;	// Directory descriptor
    	    struct stat statbuf;	// buffer for stat
    	    std::string in_pattern = fake_cwd.compose(pattern/*.c_str()*/);

	if (stat(in_pattern.c_str(), &statbuf) == -1)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Listing dir is failed - pattern \"%s\" (%s) is not exist", __func__, pattern.c_str(), in_pattern.c_str());
	    return ESP_ERR_NOT_FOUND;
	}; /* if stat(tmpstr, &statbuf) == -1 */
	if (!S_ISDIR(statbuf.st_mode))
	{
	    if (pattern.back() == '/' || pattern.back() == '.')
	    {
		ESP_LOGE(CMD_TAG_PRFX, "%s: %s -\n\t\t\t\t%s; pattern \"%s\" is invalid", __func__,
			"Name of the file or other similar entity that is not a directory",
			"cannot end with a slash or a dot", pattern.c_str());
		return ESP_ERR_INVALID_ARG;
	    }; /* if pattern[strlen(pattern) - 1] == '/' */

	    ESP_LOGI(__func__, "\n%s %s, file size %ld bytes\n", pattern.c_str(),
			(S_ISLNK(statbuf.st_mode))? "[symlink]":
			(S_ISREG(statbuf.st_mode))? "(file)":
			(S_ISDIR(statbuf.st_mode))? "<DIR>":
			(S_ISCHR(statbuf.st_mode))? "[char dev]":
			(S_ISBLK(statbuf.st_mode))? "[blk dev]":
			(S_ISFIFO(statbuf.st_mode))? "[FIFO]":
			(S_ISSOCK(statbuf.st_mode))? "[socket]":
			"[unknown type]", statbuf.st_size);
	    return ESP_OK;
	}; /* if (!S_ISDIR(statbuf.st_mode)) */


	dir = opendir(in_pattern.c_str());
	if (!dir) {
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Error opening directory <%s>, %s", __func__, pattern.c_str(), strerror(errno));
	    return ESP_FAIL;
	}; /* if !dir */

	    esp_err_t ret = ESP_OK;

	ESP_LOGI(__func__, "Files in the directory <%s> (%s)",  pattern.c_str(), in_pattern.c_str());
	printf("----------------\n");

	entry_cnt = listing_direntries_Cpp(dir, in_pattern);
	if (entry_cnt)
	{
	    cout << "----------------" << endl;
	    cout << aso::format("Total found %d files", entry_cnt) << endl;
	} /* if entry_cnt */
	else
	{
	    ESP_LOGW(__func__, "Files or directory not found, directory is empty.");
	    cout << "----------------" << endl;
	}; /* else if entry_cnt */

	if (errno != 0)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Error occured during reading of the directory <%s>, %s", __func__, pattern.c_str(), strerror(errno));
	    ret = ESP_FAIL;
	}; /* if errno != 0 */
	closedir(dir);
	cout << endl;
	return ret;
    }; /* Server::ls */


    // Printout one entry of the dir, C++ edition
    static void ls_entry_printout_Cpp(const char fullpath[], const char name[]);


    // Listing the entries of the opened directory
    // C++ edition
    // Parameters:
    //	dir  - opened directory stream;
    //	path - full path of this directory
    // Return:
    //	>=0 - listed entries counter;
    //	<0  - error - -1*(ESP_ERR_xxx) or ESP_FAIL (-1) immediately
    int listing_direntries_Cpp(DIR *dir, const std::string& path)
    {
	    char pathbuf[PATH_MAX + 1]; // @suppress("Symbol is not resolved")
	    char * fnbuf;
	    int cnt = 0;

	if (realpath(path.c_str(), pathbuf) == NULL)
	{
	    ESP_LOGE(CMD_TAG_PRFX CMD_NM, "Error canonicalizing path \"<%s>\", %s", path.c_str(), strerror(errno));
	    return ESP_FAIL;
	}; /* if realpath(pattern, pathbuf) == NULL */

	errno = 0;	// clear any possible errors
	fnbuf = pathbuf + strlen(pathbuf);
	fnbuf[0] = '/';
	fnbuf++;

	for ( struct dirent *entry = readdir(dir); entry != NULL; entry = readdir(dir))
	{
	    cnt++;
	    strcpy(fnbuf, entry->d_name);
	    ls_entry_printout_Cpp(pathbuf, entry->d_name);
	}; /* for entry = readdir(dir); entry != NULL; entry = readdir(dir) */
	return cnt;
    }; /* listing_direntries_Cpp */


    // Printout one entry of the dir, C++ edition
    void ls_entry_printout_Cpp(const char fullpath[], const char name[])
    {
	    struct stat statbuf;

	stat(fullpath, &statbuf);
	cout << fullpath << endl
	    << aso::format("\t%s ") % name
	    << aso::format((S_ISDIR(statbuf.st_mode))? "<DIR>":
			(S_ISREG(statbuf.st_mode))? "(file)": "[%s]",
			(S_ISLNK(statbuf.st_mode))? "symlink":
			(S_ISCHR(statbuf.st_mode))? "char dev":
			(S_ISBLK(statbuf.st_mode))? "blk dev":
			(S_ISFIFO(statbuf.st_mode))? "FIFO":
			(S_ISSOCK(statbuf.st_mode))? "socket":
				"unknown/other type") << endl;
    }; /* ls_entry_printout_Cpp */


//#define __NOT_OVERWRITE__	// Deny overwrite cp & move destination files

#undef CMD_NM
#define CMD_NM "cp"
#define __CP_OVERWRITE_FILE__

    // copy files according a pattern
    esp_err_t Server::cp(SDMMC::Device& device, const std::string& src_raw, const std::string& dest_raw)
    {
	if (astr::is_space(src_raw))
	{

	    ESP_LOGE(CMD_TAG_PRFX, "%s: too few arguments: invoke command \"%s\" without parameters.\n%s", __func__, __func__,
		    "Don't know what to copy.");
	    return ESP_ERR_INVALID_ARG;
	}; /* if astr::is_space(src_raw) */

	if (astr::is_space(dest_raw))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: too few arguments: invoke command \"%s\" with one parameters.\n%s", __func__, __func__,
		    "Don't know where to copy.");
	    return ESP_ERR_INVALID_ARG;
	}; /* if astr::is_space(dest_raw) */

	if (!fake_cwd.valid(src_raw.c_str()))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: the source file name \"%s\" is invalid", __func__, src_raw.c_str());
	    return ESP_ERR_NOT_FOUND;
	}; /* if !fake_cwd.valid(src_raw.c_str()) */

	if (!fake_cwd.valid(dest_raw.c_str()))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: the destination file name \"%s\" is invalid", __func__, dest_raw.c_str());
	    return ESP_ERR_NOT_FOUND;
	}; /* if !fake_cwd.valid(dest_raw.c_str()) */

	    std::string src = fake_cwd.compose(src_raw/*.c_str()*/);

#if __cplusplus < 201703L


	    struct stat st;

	// Check if source file is not exist
	if (stat(src.c_str(), &st) != 0)
	{
	    // Source file must be exist
	    ESP_LOGE(CMD_TAG_PRFX, "%s: file \"%s\" (%s) is not exist - copyng a non-existent file is not possible.\n%s",
		    __func__, src_raw.c_str(), src.c_str(), esp_err_to_name(ESP_ERR_NOT_FOUND));
	    return ESP_ERR_NOT_FOUND;
	}; /* if stat(src.c_str(), &st) */
	if (S_ISDIR(st.st_mode))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: copyng directories is unsupported.\n%s",
		    __func__, esp_err_to_name(ESP_ERR_NOT_SUPPORTED));
	    return ESP_ERR_NOT_SUPPORTED;
	}; /* if (S_ISDIR(st.st_mode)) */

	    std::string srcbase = basename(src.c_str());
	    std::string dest = fake_cwd.compose(dest_raw/*.c_str()*/);

	    // Check if destination file is exist
	if (stat(dest.c_str(), &st) == 0)
	{
	    // Destination file is exist
	    ESP_LOGI(CMD_TAG_PRFX, "%s: path \"%s\" (%s) is exist - copy is write to an existent file or directory.",
		    __func__, dest_raw.c_str(), dest.c_str());
	    // if destination - exist path, not a directory
	    if (S_ISDIR(st.st_mode))
		dest = dest + ((dest.back() != '/')? "/": "") + srcbase;	// create destination file full name
	}; /* if stat(dest.c_str(), &st) == 0 */

	// Re-check the modified version of the
	// destination filename, that may be exist:
	if (stat(dest.c_str(), &st) == 0)
	{
	    // the final name of the target file
	    // must not be a existing directory name
	    if (S_ISDIR(st.st_mode))
	    {
		ESP_LOGE(CMD_TAG_PRFX, "%s: overwrite exist \"%s\" directory by the destination file is denied; aborting.",
			__func__, dest.c_str());
		return ESP_ERR_NOT_SUPPORTED;
	    } /* if S_ISDIR(st.st_mode) */

#if !defined(__NOT_OVERWRITE__) && defined(__CP_OVERWRITE_FILE__)
	    ESP_LOGW(CMD_TAG_PRFX, "%s: overwrite an existing file \"%s\".", __func__, dest.c_str());
#else
	    ESP_LOGE(CMD_TAG_PRFX, "%s: overwrite the existent file \"%s\" is denied; aborting.", __func__, dest.c_str());
	    return ESP_ERR_NOT_SUPPORTED;
#endif	// __CP_OVER_EXIST_FILE__
	}; /* if stat(dest, &st) == 0 */

	if (src == dest)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: source & destination file name are same: \"%s\";\n\t\t\t copying file to iself is unsupported",
		    __func__, dest.c_str());
	    return ESP_ERR_NOT_SUPPORTED;
	}; /* if strcmp(src, dest) == 0 */

	// destination file - OK, it's not exist or is may be overwrited
	ESP_LOGI(CMD_TAG_PRFX ":" CMD_NM, "copy file %s to %s", src.c_str(), dest.c_str());

	    FILE* srcfile = fopen(src.c_str(), "rb");
	    FILE* destfile = fopen(dest.c_str(), "wb");

	if (!destfile)
	{
	    ESP_LOGE(CMD_TAG_PRFX CMD_NM, "failed creating file %s, aborting ", dest.c_str());
	    fclose(srcfile);
	    return ESP_ERR_NOT_FOUND;
	}; /* if !destfile */

#else

	    struct stat st;

	// Check if source file is not exist
	if (stat(src.c_str(), &st) != 0)
	{
	    // Source file must be exist
	    ESP_LOGE(CMD_TAG_PRFX, "%s: file \"%s\" (%s) is not exist - copyng a non-existent file is not possible.\n%s",
		    __func__, src_raw.c_str(), src.c_str(), esp_err_to_name(ESP_ERR_NOT_FOUND));
	    return ESP_ERR_NOT_FOUND;
	}; /* if stat(src.c_str(), &st) */
	if (S_ISDIR(st.st_mode))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: copyng directories is unsupported.\n%s",
		    __func__, esp_err_to_name(ESP_ERR_NOT_SUPPORTED));
	    return ESP_ERR_NOT_SUPPORTED;
	}; /* if (S_ISDIR(st.st_mode)) */

	    std::string srcbase = basename(src.c_str());
	    std::string dest = fake_cwd.compose(dest_raw/*.c_str()*/);

	    // Check if destination file is exist
	if (stat(dest.c_str(), &st) == 0)
	{
	    // Destination file is exist
	    ESP_LOGI(CMD_TAG_PRFX, "%s: path \"%s\" (%s) is exist - copy is write to an existent file or directory.",
		    __func__, dest_raw.c_str(), dest.c_str());
	    // if destination - exist path, not a directory
	    if (S_ISDIR(st.st_mode))
		dest = dest + ((dest.back() != '/')? "/": "") + srcbase;	// create destination file full name
	}; /* if stat(dest.c_str(), &st) == 0 */

	// Re-check the modified version of the
	// destination filename, that may be exist:
	if (stat(dest.c_str(), &st) == 0)
	{
	    // the final name of the target file
	    // must not be a existing directory name
	    if (S_ISDIR(st.st_mode))
	    {
		ESP_LOGE(CMD_TAG_PRFX, "%s: overwrite exist \"%s\" directory by the destination file is denied; aborting.",
			__func__, dest.c_str());
		return ESP_ERR_NOT_SUPPORTED;
	    } /* if S_ISDIR(st.st_mode) */

#if !defined(__NOT_OVERWRITE__) && defined(__CP_OVERWRITE_FILE__)
	    ESP_LOGW(CMD_TAG_PRFX, "%s: overwrite an existing file \"%s\".", __func__, dest.c_str());
#else
	    ESP_LOGE(CMD_TAG_PRFX, "%s: overwrite the existent file \"%s\" is denied; aborting.", __func__, dest.c_str());
	    return ESP_ERR_NOT_SUPPORTED;
#endif	// __CP_OVER_EXIST_FILE__
	}; /* if stat(dest, &st) == 0 */

	if (src == dest)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: source & destination file name are same: \"%s\";\n\t\t\t copying file to iself is unsupported",
		    __func__, dest.c_str());
	    return ESP_ERR_NOT_SUPPORTED;
	}; /* if strcmp(src, dest) == 0 */

	// destination file - OK, it's not exist or is may be overwrited
	ESP_LOGI(CMD_TAG_PRFX ":" CMD_NM, "copy file %s to %s", src.c_str(), dest.c_str());

	    FILE* srcfile = fopen(src.c_str(), "rb");
	    FILE* destfile = fopen(dest.c_str(), "wb");

	if (!destfile)
	{
	    ESP_LOGE(CMD_TAG_PRFX CMD_NM, "failed creating file %s, aborting ", dest.c_str());
	    fclose(srcfile);
	    return ESP_ERR_NOT_FOUND;
	}; /* if !destfile */

#endif	// else __cplusplus < 201703L

#define CP_BUFSIZE 512
	    char buf[CP_BUFSIZE];
	    size_t readcnt;

	while (!feof(srcfile))
	{
	    readcnt = fread(buf, 1, CP_BUFSIZE, srcfile);
	    if (readcnt == 0)
		break;
	    fwrite(buf, 1, readcnt, destfile);
	}; /* while !feof(srcfile) */

	fflush(destfile);
	fsync(fileno(destfile));
	fclose(destfile);
	fclose(srcfile);

	return ESP_OK;

    }; /* Server::cp */



#define __MV_OVERWRITE_FILE__

#undef CMD_NM
#define CMD_NM "mv"

    // move files according a pattern
    esp_err_t Server::mv(SDMMC::Device& device, const std::string& src_raw, const std::string& dest_raw)
    {

	if (astr::is_space(src_raw))
	{
	    ESP_LOGE(CMD_TAG_PRFX CMD_NM, "too few arguments: invoke command \"%s\" with one parameters.\n%s", CMD_NM,
		    "Don't know what to move?");
	    return ESP_ERR_INVALID_ARG;
	}; /* if astr::is_space(src_raw) */

	if (astr::is_space(dest_raw))
	{
	    ESP_LOGE(CMD_TAG_PRFX CMD_NM, "too few arguments: invoke command \"%s\" without parameters.\n%s", CMD_NM,
		    "Don't know where to move?");
	    return ESP_ERR_INVALID_ARG;
	}; /* if astr::is_space(dest_raw) */

	if (!fake_cwd.valid(src_raw.c_str()))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: the souce file name \"%s\" is invalid", __func__, src_raw.c_str());
	    return ESP_ERR_NOT_FOUND;
	}; /* if !fake_cwd.valid(src_raw.c_str()) */
	if (!fake_cwd.valid(dest_raw.c_str()))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: the destination file name \"%s\" is invalid", __func__, dest_raw.c_str());
	    return ESP_ERR_NOT_FOUND;
	}; /* if !fake_cwd.valid(dest_raw.c_str()) */



#if __cplusplus < 201703L


	    std::string src = fake_cwd.compose(src_raw/*.c_str()*/);
	    struct stat st_src;

	// Check if source file is not exist
	if (stat(src.c_str(), &st_src) != 0)
	{
	    // Source file must be exist
	    ESP_LOGE(CMD_TAG_PRFX, "%s: file \"%s\" (%s) is not exist - renaming a non-existent file is not possible.\n%s",
		    __func__, src_raw.c_str(), src.c_str(), esp_err_to_name(ESP_ERR_NOT_FOUND));
	    return ESP_ERR_NOT_FOUND;
	}; /* if stat(src.c_str(), &st_src) != 0 */


	    std::string dest = fake_cwd.compose(dest_raw/*.c_str()*/);
	    struct stat st_dest;

	cout << aso::format("Move file \"%s\" (%s) to \"%s\" (%s)") %src_raw %src
			%dest_raw %dest << endl;

	if (stat(dest.c_str(), &st_dest) == 0)
	{
	    // Target file exist
	    ESP_LOGW(CMD_TAG_PRFX, "%s: target file name \"%s\" (%s) exist",
		    __func__, dest_raw.c_str(), dest.c_str());
	    // if destination is existing directory
	    if (S_ISDIR(st_dest.st_mode))
	    {
		    std::string basenm = basename(src.c_str());

		ESP_LOGD(CMD_TAG_PRFX, "%s: destination file is exist directory,\n\t\t\tbasename of src is: %s ", __func__,
			basenm.c_str());
		// Add trailing slash if it absent
		if (dest.back() != '/')
		    dest += '/';
		ESP_LOGD(CMD_TAG_PRFX, "%s: adding trailing slash to a destination file: %s", __func__, dest.c_str());
		dest += basenm;
		ESP_LOGD(CMD_TAG_PRFX, "%s: adding src basename to a destination file: %s", __func__, dest.c_str());
	    } /* if S_ISDIR(st.st_mode) */
	} /* if stat(dest, &st) != 0 */

	// Re-check the modified version of the
	// destination filename, that may be exist:
	if (stat(dest.c_str(), &st_dest) == 0)
	{
	    // the final name of the target file
	    // must not be a existing directory name
	    if (S_ISDIR(st_dest.st_mode))
	    {
		ESP_LOGE(CMD_TAG_PRFX, "%s: overwrite exist directory \"%s\" by the destination file from the %s is not allowed; aborting.",
			__func__, dest.c_str(), src.c_str());
		return ESP_ERR_NOT_SUPPORTED;
	    } /* if S_ISDIR(st.st_mode) */

	    // if source - is dir, but destination - ordinary file
	    if (S_ISDIR(st_src.st_mode))
	    {
		ESP_LOGE(CMD_TAG_PRFX, "%s: overwrite exist file \"%s\" by renaming the source directory %s to it - is not allowed; aborting.",
			__func__, dest.c_str(), src.c_str());
		return ESP_ERR_NOT_SUPPORTED;
	    }; /* if S_ISDIR(st.st_mode) */

#if !defined(__NOT_OVERWRITE__) && defined(__CP_OVERWRITE_FILE__)
	    ESP_LOGW(CMD_TAG_PRFX, "%s: overwrite an existing file \"%s\".", __func__, dest.c_str());
#else
	    ESP_LOGE(CMD_TAG_PRFX, "%s: overwrite the existent file \"%s\" is denied; aborting.",
		    __func__, dest.c_str());
	    return ESP_ERR_NOT_SUPPORTED;
#endif	// __CP_OVER_EXIST_FILE__
	}; /*     if stat(dest.c_str(), &st_dest) == 0 */


#else	// __cplusplus < 201703L

	    std::string src = fake_cwd.compose(src_raw/*.c_str()*/);
	    struct stat st_src;

	// Check if source file is not exist
	if (stat(src.c_str(), &st_src) != 0)
	{
	    // Source file must be exist
	    ESP_LOGE(CMD_TAG_PRFX, "%s: file \"%s\" (%s) is not exist - renaming a non-existent file is not possible.\n%s",
		    __func__, src_raw.c_str(), src.c_str(), esp_err_to_name(ESP_ERR_NOT_FOUND));
	    return ESP_ERR_NOT_FOUND;
	}; /* if stat(src.c_str(), &st_src) != 0 */


	    std::string dest = fake_cwd.compose(dest_raw/*.c_str()*/);
	    struct stat st_dest;

	cout << aso::format("Move file \"%s\" (%s) to \"%s\" (%s)") %src_raw %src
				%dest_raw %dest << endl;

	if (stat(dest.c_str(), &st_dest) == 0)
	{
	    // Target file exist
	    ESP_LOGW(CMD_TAG_PRFX, "%s: target file name \"%s\" (%s) exist",
		    __func__, dest_raw.c_str(), dest.c_str());
	    // if destination is existing directory
	    if (S_ISDIR(st_dest.st_mode))
	    {
		    std::string basenm = basename(src.c_str());

		ESP_LOGD(CMD_TAG_PRFX, "%s: destination file is exist directory,\n\t\t\tbasename of src is: %s ", __func__,
			basenm.c_str());
		// Add trailing slash if it absent
		if (dest.back() != '/')
		    dest += '/';
		ESP_LOGD(CMD_TAG_PRFX, "%s: adding trailing slash to a destination file: %s", __func__, dest.c_str());
		dest += basenm;
		ESP_LOGD(CMD_TAG_PRFX, "%s: adding src basename to a destination file: %s", __func__, dest.c_str());
	    } /* if S_ISDIR(st.st_mode) */
	} /* if stat(dest, &st) != 0 */

	// Re-check the modified version of the
	// destination filename, that may be exist:
	if (stat(dest.c_str(), &st_dest) == 0)
	{
	    // the final name of the target file
	    // must not be a existing directory name
	    if (S_ISDIR(st_dest.st_mode))
	    {
		ESP_LOGE(CMD_TAG_PRFX, "%s: overwrite exist directory \"%s\" by the destination file from the %s is not allowed; aborting.",
			__func__, dest.c_str(), src.c_str());
		return ESP_ERR_NOT_SUPPORTED;
	    } /* if S_ISDIR(st.st_mode) */

	    // if source - is dir, but destination - ordinary file
	    if (S_ISDIR(st_src.st_mode))
	    {
		ESP_LOGE(CMD_TAG_PRFX, "%s: overwrite exist file \"%s\" by renaming the source directory %s to it - is not allowed; aborting.",
			__func__, dest.c_str(), src.c_str());
		return ESP_ERR_NOT_SUPPORTED;
	    }; /* if S_ISDIR(st.st_mode) */

#if !defined(__NOT_OVERWRITE__) && defined(__CP_OVERWRITE_FILE__)
	    ESP_LOGW(CMD_TAG_PRFX, "%s: overwrite an existing file \"%s\".", __func__, dest.c_str());
#else
	    ESP_LOGE(CMD_TAG_PRFX, "%s: overwrite the existent file \"%s\" is denied; aborting.",
		    __func__, dest.c_str());
	    return ESP_ERR_NOT_SUPPORTED;
#endif	// __CP_OVER_EXIST_FILE__
	}; /*     if stat(dest.c_str(), &st_dest) == 0 */

#endif	// __cplusplus < 201703L

	// Names of the moving/renaming file
	ESP_LOGI(CMD_TAG_PRFX, "%s: Moving/renaming file %s (%s) to %s (%s)", __func__, src_raw.c_str(), src.c_str(), dest_raw.c_str(), dest.c_str());
	// check the source and destination file are same
	if (src == dest)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: source & destination file name are same: \"%s\";\n\t\t\t copying file to iself is unsupported",
		    __func__, dest.c_str());
	    return ESP_ERR_NOT_SUPPORTED;
	}; /* if src == dest */


	//    // Rename original file
	//    cout << aso::format("Move/rename file %s (%s) to %s (%s)") %src_raw %src
	//			%dest_raw %dest << endl;
	if (rename(src.c_str(), dest.c_str()) != 0)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Error %d: %s", __func__, errno, strerror(errno));
	    return ESP_FAIL;
	}; /* if rename(src.c_str(), dest.c_str()) != 0 */
//	ESP_LOGW(CMD_TAG_PRFX CMD_NM, "the command '%s' now is partyally implemented for C edition", __func__);
	return ESP_OK;

    }; /* Server::mv */



#undef CMD_NM
#define CMD_NM "rm"

    // remove files according a pattern
    esp_err_t Server::rm(SDMMC::Device& device, const std::string& pattern)
    {

	if (!fake_cwd.valid(pattern.c_str()))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: pattern \"%s\" is invalid", __func__, pattern.c_str());
	    return ESP_ERR_NOT_FOUND;
	}; /* !device.valid_path(pattern) */

	    struct stat st;
	    std::string path = fake_cwd.compose(pattern/*.c_str()*/);

	if (astr::is_space(pattern))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: invoke command \"%s\" without parameters.\n%s", __func__, __func__,
		    "Missing filename to remove.");
	    return ESP_ERR_INVALID_ARG;
	}; /* if astr::is_space(pattern) */

	ESP_LOGI(CMD_TAG_PRFX, "%s: delete file \"%s\" (%s)", __func__,  pattern.c_str(), path.c_str());
#if __cplusplus < 201703L

	// Check if destination file exists before deleting
	if (stat(path.c_str(), &st) != 0)
	{
	    // deleting a non-existent file is not possible
	    ESP_LOGE(CMD_TAG_PRFX, "%s: file \"%s\" is not exist - deleting a non-existent file is not possible.\n%s",
		    __func__, pattern.c_str(), esp_err_to_name(ESP_ERR_NOT_FOUND));
	    return ESP_ERR_NOT_FOUND;
	}; /* if stat(file_foo, &st) != 0 */
	if (S_ISDIR(st.st_mode))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: deleting directories unsupported.\n%s",
		    __func__, esp_err_to_name(ESP_ERR_NOT_SUPPORTED));
	    return ESP_ERR_NOT_SUPPORTED;
	}; /* if (S_ISDIR(st.st_mode)) */
	errno = 0;
	//cout << "Now exec: ===>> " << aso::format("unlink(%s)") % path << "<<===" << endl;
	unlink(path.c_str());
	if (errno)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Fail when deleting \"%s\": %s", __func__, pattern.c_str(), strerror(errno));

	    return ESP_FAIL;
	}; /* if errno */

	return ESP_OK;

#else
	// Check if destination file exists before deleting
	if (stat(path.c_str(), &st) != 0)
	{
	    // deleting a non-existent file is not possible
	    ESP_LOGE(CMD_TAG_PRFX, "%s: file \"%s\" is not exist - deleting a non-existent file is not possible.\n%s",
		    __func__, pattern.c_str(), esp_err_to_name(ESP_ERR_NOT_FOUND));
	    return ESP_ERR_NOT_FOUND;
	}; /* if stat(file_foo, &st) != 0 */
	if (S_ISDIR(st.st_mode))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: deleting directories unsupported.\n%s",
		    __func__, esp_err_to_name(ESP_ERR_NOT_SUPPORTED));
	    return ESP_ERR_NOT_SUPPORTED;
	}; /* if (S_ISDIR(st.st_mode)) */
	errno = 0;
	//cout << "Now exec: ===>> " << aso::format("unlink(%s)") % path << "<<===" << endl;
	unlink(path.c_str());
	if (errno)
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Fail when deleting \"%s\": %s", __func__, pattern.c_str(), strerror(errno));

	    return ESP_FAIL;
	}; /* if errno */

	return ESP_OK;

#endif // __cplusplus < 201703L

    }; /* Server::rm */



#undef CMD_NM
#define CMD_NM "cat"

    // type file contents
    esp_err_t Server::cat(SDMMC::Device& device, const std::string& fname)
    {

	if (!fake_cwd.valid(fname.c_str()))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: pattern \"%s\" is invalid", __func__, fname.c_str());
	    return ESP_ERR_NOT_FOUND;
	}; /* !device.valid_path(fname) */

	    struct stat st;
	    std::string fullname = fake_cwd.compose(fname/*.c_str()*/);
	    FILE *text = nullptr; // file for type to screen

	if (empty(fname))
	{
	    cout << endl
		<< "*** Printing contents of the file <XXXX fname>. ***" << endl
		<< endl;
	    ESP_LOGE(CMD_TAG_PRFX CMD_NM, "invoke command \"%s\" without parameters.\n%s", CMD_NM,
		    "Missing filename for print to output.");

	    cout << "*** End of printing file XXXX. ** ******************" << endl;
	    return ESP_ERR_INVALID_ARG;
	}; /* if empty(fname) */ /* if fname == NULL || strcmp(fname, "") */

#if 1
	cout << endl
	    << aso::format("*** Printing contents of the file <%s> (realname '%s'). ***") % fname % fullname  << endl
	    << endl;
#else
	cout << endl
	    << "*** Printing contents of the file <" << fname << "> (realname '" << fullname << "'). ***"  << endl
	    << endl;
#endif

#if __cplusplus < 201703L

	// Check if destination file exists before typing
	if (stat(fullname.c_str(), &st) != 0)
	{
	    // typing a non-exist file is not possible
	    ESP_LOGE(CMD_TAG_PRFX, "%s: \"%s\" file does not exist - printing of the missing file is not possible.\n%s",
		    __func__, fname.c_str(), esp_err_to_name(ESP_ERR_NOT_FOUND));
	    return ESP_ERR_NOT_FOUND;
	}; /* if stat(path, &st) != 0 */

	if (S_ISDIR(st.st_mode))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Typing directories unsupported, use the 'ls' command instead.\n%s",
		    __func__, esp_err_to_name(ESP_ERR_NOT_SUPPORTED));
	    return ESP_ERR_NOT_SUPPORTED;
	}; /* if (S_ISDIR(st.st_mode)) */

	errno = 0;	// clear possible errors
	text = fopen(fullname.c_str(), "r"); // open the file for type to screen
	if (!text)
	{
	    ESP_LOGE(CMD_TAG_PRFX CMD_NM, "Error opening file <%s> (%s), %s", fname.c_str(), fullname.c_str(), strerror(errno));
	    return ESP_FAIL;
	}; /* if !FILE */

	for (char c = getc(text); !feof(text); c = getc(text))
	    putchar(c);

	if (errno)
	{
	    ESP_LOGE(CMD_TAG_PRFX CMD_NM, "Error during type the file %s (%s) to output, %s", fname.c_str(), fullname.c_str(), strerror(errno));
	    fclose(text);
	    return ESP_FAIL;
	}; /* if errno */

#else

	// Check if destination file exists before typing
	if (stat(fullname.c_str(), &st) != 0)
	{
	    // typing a non-exist file is not possible
	    ESP_LOGE(CMD_TAG_PRFX, "%s: \"%s\" file does not exist - printing of the missing file is not possible.\n%s",
		    __func__, fname.c_str(), esp_err_to_name(ESP_ERR_NOT_FOUND));
	    return ESP_ERR_NOT_FOUND;
	}; /* if stat(path, &st) != 0 */

	if (S_ISDIR(st.st_mode))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: Typing directories unsupported, use the 'ls' command instead.\n%s",
		    __func__, esp_err_to_name(ESP_ERR_NOT_SUPPORTED));
	    return ESP_ERR_NOT_SUPPORTED;
	}; /* if (S_ISDIR(st.st_mode)) */

	errno = 0;	// clear possible errors
	text = fopen(fullname.c_str(), "r"); // open the file for type to screen
	if (!text)
	{
	    ESP_LOGE(CMD_TAG_PRFX CMD_NM, "Error opening file <%s> (%s), %s", fname.c_str(), fullname.c_str(), strerror(errno));
	    return ESP_FAIL;
	}; /* if !FILE */

	for (char c = getc(text); !feof(text); c = getc(text))
	    putchar(c);

	if (errno)
	{
	    ESP_LOGE(CMD_TAG_PRFX CMD_NM, "Error during type the file %s (%s) to output, %s", fname.c_str(), fullname.c_str(), strerror(errno));
	    fclose(text);
	    return ESP_FAIL;
	}; /* if errno */
#endif	// __cplusplus < 201703L

	cout << endl
	    << "*** End of printing file " << fname << ". **************" << endl
	    << endl;

	fclose(text);
	if (errno)
	{
	    ESP_LOGE(CMD_TAG_PRFX CMD_NM, "Error during closing the file %s (%s) to output, %s", fname.c_str(), fullname.c_str(), strerror(errno));
	    fclose(text);
	    return ESP_ERR_INVALID_STATE;
	}; /* if errno */

	return ESP_OK;
    }; /* cat */



#undef CMD_NM
#define CMD_NM "type"

    // type text from keyboard to screen
    esp_err_t Server::type()
    {
	cout << endl
	     << "**** Type the text on keyboard to screen *****" << endl
	     << "Press <Enter> twice for exit..." << endl
	     << endl;

	    char c = '\0', prevc;
	do {
	    prevc = c;
	    cin >> noskipws >> c;
	    cout << c;
//	    if (c == '\n')
//		cout << "<LF>" << endl;
//	    if (c == '\r')
//		cout << "<CR>" << endl;
	} while (c != prevc || c != '\n');

	cout << endl << endl
	     << "**** End of typing the text on keyboard. *****" << endl
	     << endl;
	return ESP_OK;
    }; /* type */

    // Generates an error message if the struct stat
    // refers to an object, other than a file.
    // fname - the name of the object referenced by the struct stat.
    //static esp_err_t err4existent(const char fname[], const struct stat* statbuf);
    static esp_err_t err4existent(const std::string& fname, const struct stat& statbuf);


    // type text from keyboard to file and to screen
    esp_err_t Server::type(SDMMC::Device& device, const std::string& fname, size_t sector_size)
    {
	if (!fake_cwd.valid(fname.c_str()))
	{
	    ESP_LOGE(CMD_TAG_PRFX, "%s: pattern \"%s\" is invalid", __func__, fname.c_str());
	    return ESP_ERR_NOT_FOUND;
	}; /* !device.valid_path(fname) */

	    struct stat st;
	    std::string fullname = fake_cwd.compose(fname/*.c_str()*/);
	    FILE *storage = NULL;

	// Test file 'fname' for existing
	errno = 0;	// clear all error state
	if (access(fullname.c_str(), F_OK) == -1)
	{
	    if (errno == ENOENT)	// error "file does not exist"
	    {

		if (!stat(fullname.c_str(), &st))	// but if the fname still exists here, then it is a directory // @suppress("Symbol is not resolved")
		    return err4existent(fname, st);
		ESP_LOGI(CMD_TAG_PRFX CMD_NM, "OK, file \"%s\" does not exist, opening this file.", fname.c_str());
		cout << aso::format("Open file %s for the write") % fullname << endl;
		errno = 0;	// clear error state
		storage = fopen(fullname.c_str(), "w");
	    } /* if errno == ENOENT */
	    else	// error other than "file does not exist"
	    {
		ESP_LOGE(CMD_TAG_PRFX CMD_NM, "Error test existing file %s: %s", fullname.c_str() , strerror(errno));
		return ESP_FAIL;
	    } /* else if errno == ENOENT */
	} /* if stat(fname, &statbuf) == -1 */
	else
	{	// Error - file fname is exist

		char c;

	    // fname exists, check that is a regular file
	    if (!stat(fullname.c_str(), &st) && !S_ISREG(st.st_mode)) // @suppress("Symbol is not resolved")
		return err4existent(fname, st);

	    cout << aso::format("File %s is exist.\nDo you want use this file? [yes(add)/over(write)/No]: ") % fname;
	    cin >> noskipws >> c;
	    cout << c;

	    if (c == '\n')
	    cout << "<LF>";
	    cout << endl;

	    switch (tolower(c))
	    {
	    case 'a':
	    case 'y':
		ESP_LOGI(CMD_TAG_PRFX CMD_NM, "OK, open the file %s to add.", fname.c_str());
		cout << aso::format("File %s is opened for add+write.") % fname << endl;
		storage = fopen(fullname.c_str(), "a");
		break;

	    case 'o':
	    case 'w':
		ESP_LOGW(CMD_TAG_PRFX CMD_NM, "OK, open the file %s to owerwrite.", fname.c_str());
		cout << aso::format("File %s is opened to truncate+write (overwrite).") % fname << endl;
		storage = fopen(fullname.c_str(), "w");
		break;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="
	    case '\n':
		cout << "Enter char '\\n'" << endl; // @suppress("No break at end of case")
	    case 'n':
		ESP_LOGW(CMD_TAG_PRFX ":" CMD_NM " <filename>", "User cancel opening file %s.", fname.c_str());
		return ESP_ERR_NOT_FOUND;
		break;
#pragma GCC diagnostic pop

	    default:
		ESP_LOGW(CMD_TAG_PRFX CMD_NM, "Error: H.z. cho in input, input char value is: [%d]", (int)c);
		return ESP_ERR_INVALID_ARG;
	    }; /* switch tolower(c) */
	}; /* else if stat(fname, &statbuf) == -1 */

	if (storage == NULL)
	{
	    ESP_LOGE(CMD_TAG_PRFX CMD_NM, "Any error occured when opening the file %s: %s.", fname.c_str(), strerror(errno));
	    return ESP_ERR_NOT_FOUND;
	}; /* if storage == NULL */

//#define TYPEBUFSIZE (8 * 4*BUFSIZ)
//#define TYPEBUFSIZE (4 * card->self->csd.sector_size)
#define TYPEBUFSIZE (4 * sector_size)
//	    char typebuf[TYPEBUFSIZE];
	    char *typebuf = (char*)malloc(TYPEBUFSIZE);
//	    char *typebuf = (char*)malloc( card->data->csd.sector_size);

	errno = 0;
	setvbuf(storage, typebuf, _IOFBF, TYPEBUFSIZE);
	if (errno)
	{
	    ESP_LOGE(CMD_TAG_PRFX CMD_NM, "Error when setting buffering mode for file %s: %s.", fname.c_str(), strerror(errno));
	    fclose(storage);
	    return ESP_FAIL;
	}; /* if (errno) */


	cout << endl
	     << aso::format("**** Type the text on keyboard to screen and file [%s]. ****") % fname  << endl
	     << "Press <Enter> twice for exit..." << endl
	     << endl;

	    char c = '\0', prevc;
	do {
	    prevc = c;
	    cin >> noskipws >> c;
	    cout << c;
	    fputc(c, storage);
	} while (c != prevc || c != '\n');

	cout << endl << endl
	     << "**** End of typing the text on keyboard. *****" << endl
	     << endl;


	cout << aso::format("Flush&write cache buffer of the file %s.") % fname << endl;
	fflush(storage);
	fsync(fileno(storage));
	cout << aso::format("Close the file %s.") % fname << endl;
	fclose(storage);
	free(typebuf);
	if (errno)
	{
	    ESP_LOGE(CMD_TAG_PRFX CMD_NM, "Any error occured when closing the file %s: %s.", fname.c_str(), strerror(errno));
	    return ESP_FAIL;
	}; /* if errno */


	cout << endl
	     << aso::format("**** End of typing the text on keyboard for the screen and the file %s. ****") % fname << endl
	     << endl;
	return ESP_OK;

    }; /* Server::type <file> */


    // Generates an error message if the struct stat
    // refers to an object, other than a file.
    // fname - the name of the object referenced by the struct stat.
    //esp_err_t err4existent(const char fname[], const struct stat* statbuf)
    esp_err_t err4existent(const std::string& fname, const struct stat& statbuf)
    {
#define EXIST_FN_TAG "console::type exist chechk"
	ESP_LOGE(EXIST_FN_TAG, "Error: path %s exist, and is not a file, but a %s.\nOperation is not permitted.",
		fname.c_str(), statmode2txt(statbuf));
	return ESP_ERR_NOT_SUPPORTED;
#undef EXIST_FN_TAG
    }; /* err4existent */


#undef CMD_TAG_PRFX

    const char* Server::TAG = "SD/MMC service";

}; //--[ namespace Exec ]----------------------------------------------------------------------------------------------


//--[ sdcard_ctrl.cpp ]----------------------------------------------------------------------------

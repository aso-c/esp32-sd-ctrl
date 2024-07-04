idf_component_register(SRCS "fs_ctrl.cpp"
                            "sdcard_io.cpp"
                    INCLUDE_DIRS .
                    PRIV_REQUIRES extrstream fake_cwd
                    REQUIRES console driver sdmmc spi_flash fatfs cxx utils
		    )


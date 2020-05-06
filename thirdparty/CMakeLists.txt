    add_subdirectory(armadillo)
    
    add_subdirectory(libhdf5)
    
	add_subdirectory(protobuf)

    # Loook for the tiledb package on the system
	find_package(TileDB)

	# If its not found, add an external project to build it
	if(NOT TILEDB_FOUND)
		MESSAGE(STATUS "TileDB not found. Building as dependency from source")

		SET(TILEDB_VERSION "2.0.0")

		ExternalProject_Add(tiledb
				URL "https://github.com/TileDB-Inc/TileDB/archive/${TILEDB_VERSION}.tar.gz"
				URL_HASH SHA1=26488afa4fc90bf1cbc4377f3ea1b5953abed8b4
				DOWNLOAD_NAME "tiledb.tar.gz"
				CMAKE_ARGS
				# Set the install prefix to be the same as cytolib so headers and library will follow similar to other thirdparty packages
				-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}/cytolib
				-DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}/cytolib
				-DTILEDB_S3=ON #Enable S3 Support
				-DTILEDB_STATIC=ON #Builds the static library
				-DTILEDB_SERIALIZATION=ON #Enables TileDB Cloud support
				-DTILEDB_FORCE_ALL_DEPS=ON #Build all dependencies from source, don't use any system deps. This is recommended if you are building a distributable package as this will end up with a fat shared library, or a static library along with all the needed static libs
				# Additional cmake flags can be added, such as enabling azure of GCS integration
				UPDATE_COMMAND ""
				INSTALL_COMMAND
				${CMAKE_COMMAND} --build . --target install-tiledb
				LOG_DOWNLOAD TRUE
				LOG_CONFIGURE TRUE
				LOG_BUILD TRUE
				LOG_INSTALL TRUE
				)
	endif()
	# Set parent scope if TileDB is build from source or found on the system
	set(TILEDB_FOUND ${TILEDB_FOUND} PARENT_SCOPE)

		#cp to parent
	set(THIRDPARTY_INCLUDE_DIR ${INCLUDE_DIR} PARENT_SCOPE) 	
#	set ( PB_LIBS ${PB_LIBS} PARENT_SCOPE)
	   
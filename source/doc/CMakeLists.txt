cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

find_package (Doxygen)
if (NOT DOXYGEN_FOUND)
	message (FATAL_ERROR "VA documentation requires Doxygen which could not be found. Did you install it?")
endif ()

configure_file (doxyfile.in doxyfile @ONLY)
configure_file (doxygen.footer.html.in doxygen.footer.html @ONLY)

add_custom_target (
	VADoc
	"${DOXYGEN_EXECUTABLE}" doxyfile
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
	COMMENT "Generating VA C++ API documentation with Doxygen"
	VERBATIM
)

set_property (TARGET VADoc PROPERTY FOLDER "Documentation")

# Not required to install documentation since it is put on our website install( DIRECTORY
# "${CMAKE_CURRENT_BINARY_DIR}/html" DESTINATION "share/doc" )

if (VAWEBSITE_FOUND)
	if (WIN32)
		configure_file ("copy_doc_to_va_website.bat.proto" "copy_doc_to_va_website.bat" @ONLY)
		add_custom_command (
			TARGET VADoc
			POST_BUILD
			COMMAND copy_doc_to_va_website.bat
			COMMENT "Copying files to VAWebsite"
			VERBATIM
		)
	endif ()
endif ()

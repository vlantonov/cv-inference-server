# ---------------------------------------------------------------------------
# Docs.cmake -- Doxygen (+ GraphViz/dot + PlantUML) documentation target.
#
# Adds an optional, NOT-part-of-ALL `docs` target that renders the Doxygen HTML
# (including the C4 component diagram) into the build tree. Degrades gracefully:
# when Doxygen is absent the target is simply skipped and the normal CPU build
# is unaffected (design docs/design/doxygen-c4.md §1).
# ---------------------------------------------------------------------------

option(CVIS_BUILD_DOCS "Build Doxygen API documentation (requires doxygen)" OFF)

if(NOT CVIS_BUILD_DOCS)
    return()
endif()

# COMPONENTS dot -> populates DOXYGEN_DOT_EXECUTABLE when GraphViz is present.
find_package(Doxygen OPTIONAL_COMPONENTS dot)

if(NOT DOXYGEN_FOUND)
    message(STATUS "CVIS_BUILD_DOCS=ON but doxygen was not found -- docs target skipped.")
    return()
endif()

# ---------------------------------------------------------------------------
# Locate the directory that contains plantuml.jar (Doxygen's PLANTUML_JAR_PATH
# wants the DIRECTORY, not the file). Overridable via the PLANTUML_JAR_DIR cache
# variable. Strategy: honour an explicit override, then search common install
# locations, then derive the path from the `plantuml` wrapper script.
# ---------------------------------------------------------------------------
set(PLANTUML_JAR_DIR "" CACHE PATH
    "Directory containing plantuml.jar (for Doxygen PLANTUML_JAR_PATH). Auto-detected if empty.")

if(PLANTUML_JAR_DIR AND EXISTS "${PLANTUML_JAR_DIR}/plantuml.jar")
    message(STATUS "PlantUML: using cached PLANTUML_JAR_DIR=${PLANTUML_JAR_DIR}")
else()
    set(_plantuml_jar "")

    # 1) Search common locations for plantuml.jar directly.
    find_file(_cvis_plantuml_jar
        NAMES plantuml.jar
        PATHS
            /usr/share/plantuml
            /usr/local/share/plantuml
            /usr/share/java
            /usr/local/share/java
            /opt/plantuml
            /opt/homebrew/share/plantuml
            /usr/local/Cellar/plantuml
        NO_CACHE)
    if(_cvis_plantuml_jar)
        set(_plantuml_jar "${_cvis_plantuml_jar}")
    endif()

    # 2) Fall back to deriving the jar path from the `plantuml` wrapper script.
    if(NOT _plantuml_jar)
        find_program(_cvis_plantuml_wrapper plantuml)
        if(_cvis_plantuml_wrapper)
            file(STRINGS "${_cvis_plantuml_wrapper}" _puml_jar_lines
                 REGEX "[^ \t]+plantuml\\.jar")
            foreach(_line IN LISTS _puml_jar_lines)
                if(_line MATCHES "(/[^ \t\"]+/plantuml\\.jar)")
                    if(EXISTS "${CMAKE_MATCH_1}")
                        set(_plantuml_jar "${CMAKE_MATCH_1}")
                        break()
                    endif()
                endif()
            endforeach()
        endif()
    endif()

    if(_plantuml_jar)
        get_filename_component(_plantuml_dir "${_plantuml_jar}" DIRECTORY)
        set(PLANTUML_JAR_DIR "${_plantuml_dir}" CACHE PATH
            "Directory containing plantuml.jar (for Doxygen PLANTUML_JAR_PATH)." FORCE)
        message(STATUS "PlantUML: detected plantuml.jar in ${PLANTUML_JAR_DIR}")
    else()
        message(STATUS "PlantUML: plantuml.jar not found -- the C4 diagram will not render. "
                       "Set -DPLANTUML_JAR_DIR=<dir> to enable it.")
    endif()
endif()

# ---------------------------------------------------------------------------
# Resolve the dot path and the output directory for the Doxyfile template.
# ---------------------------------------------------------------------------
if(DOXYGEN_DOT_EXECUTABLE)
    get_filename_component(DOXYGEN_DOT_PATH "${DOXYGEN_DOT_EXECUTABLE}" DIRECTORY)
    message(STATUS "GraphViz dot found: ${DOXYGEN_DOT_EXECUTABLE}")
else()
    set(DOXYGEN_DOT_PATH "")
    message(STATUS "GraphViz dot not found -- include/collaboration graphs disabled.")
endif()

set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")
file(MAKE_DIRECTORY "${DOXYGEN_OUTPUT_DIR}")

# Substitute @VARS@ from the template into the generated Doxyfile.
set(_cvis_doxyfile_in  "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in")
set(_cvis_doxyfile_out "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")
configure_file("${_cvis_doxyfile_in}" "${_cvis_doxyfile_out}" @ONLY)

# ---------------------------------------------------------------------------
# `docs` target -- NOT part of ALL. Output lands in the build tree only.
# ---------------------------------------------------------------------------
add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} "${_cvis_doxyfile_out}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating API documentation with Doxygen -> ${DOXYGEN_OUTPUT_DIR}/html"
    VERBATIM)

message(STATUS "Docs: `docs` target available (output: ${DOXYGEN_OUTPUT_DIR}/html).")

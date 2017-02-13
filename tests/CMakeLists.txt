
SET(TESTS_EXECS_PATH "${CMAKE_BINARY_DIR}/tests-execs")


FILE(MAKE_DIRECTORY "${TESTS_EXECS_PATH}")

EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} "-E" "copy_directory" "${CMAKE_CURRENT_SOURCE_DIR}/datasets" "${TESTS_EXECS_PATH}")


SET(OPENFLUID_RUN_OPTS "--simulators-paths=${CMAKE_BINARY_DIR}/src/simulators/import.spatial.bvservice:${CMAKE_BINARY_DIR}/src/simulators/land.indicators.bvservice:${CMAKE_BINARY_DIR}/src/simulators/water.surf-uz.runoff-infiltration.bvservice"
"--observers-paths=${CMAKE_BINARY_DIR}/src/observers/export.results.bvservice")

ADD_TEST(NAME openfluid-DardaillonSmall
         COMMAND "${OpenFLUID_CMD_PROGRAM}" run "${TESTS_EXECS_PATH}/DardaillonSmall/IN" "${TESTS_EXECS_PATH}/DardaillonSmall/OUT"
                                                 ${OPENFLUID_RUN_OPTS})

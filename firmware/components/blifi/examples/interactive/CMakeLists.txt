# Interactive serial-console example for the blifi component.
cmake_minimum_required(VERSION 3.16)

# The blifi component is resolved via main/idf_component.yml (local
# override_path inside the monorepo, ESP Component Registry otherwise).

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(blifi_interactive)

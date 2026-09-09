add_library(glim-gpu-metal STATIC src/Device.mm)
add_library(glim::gpu-metal ALIAS glim-gpu-metal)
set_source_files_properties(src/Device.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
target_link_libraries(glim-gpu-metal PUBLIC glim::gpu)
glim_target_defaults(glim-gpu-metal)

find_library(GLIM_METAL Metal REQUIRED)
find_library(GLIM_QUARTZ QuartzCore REQUIRED)
target_link_libraries(glim-gpu-metal PUBLIC ${GLIM_METAL} ${GLIM_QUARTZ})

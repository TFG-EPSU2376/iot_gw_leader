set(component_srcs "main.c")

idf_component_register(SRCS "${component_srcs}" 
                       INCLUDE_DIRS "."
                       REQUIRES  log esp_common thread_leader  vfs lora json)

list(APPEND EXTRA_COMPONENT_DIRS components)

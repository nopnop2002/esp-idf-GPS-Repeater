set(srcs "main.c" "uart_event.c" "tcp.c" "udp.c" "spp.c")
idf_component_register(SRCS ${srcs} INCLUDE_DIRS "." REQUIRED_IDF_TARGETS esp32)

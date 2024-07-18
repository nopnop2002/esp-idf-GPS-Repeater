set(srcs "main.c" "uart_event.c")

if (CONFIG_TCP_SOCKET)
	list(APPEND srcs "tcp.c")
elseif (CONFIG_UDP_BROADCAST)
	list(APPEND srcs "udp.c")
elseif (CONFIG_BLUETOOTH_SPP)
	list(APPEND srcs "spp.c")
endif()

idf_component_register(SRCS ${srcs} INCLUDE_DIRS ".")

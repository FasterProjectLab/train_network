

enter_idf:
	docker run -it --device=/dev/ttyUSB0 --rm -v $(PWD)/:/appli -w /appli espressif/idf:release-v5.1 /bin/bash 

cp_distrib:
	docker cp build/ota_system.bin ota_server:/usr/share/nginx/html/firmware.bin
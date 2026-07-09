
# idf.py -p /dev/ttyUSB0 -b 115200 build flash monitor

enter_idf_without_usb:
	docker run -it --rm -v $(PWD)/:/appli -w /appli espressif/idf:release-v6.0 /bin/bash 

enter_idf:
	docker run -it --device=/dev/ttyUSB0 --rm -v $(PWD)/:/appli -w /appli espressif/idf:release-v6.0 /bin/bash 

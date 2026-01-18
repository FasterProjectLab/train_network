
build:
	docker build -t access_point .

run: build
	docker run -it --rm -v /run/dbus:/run/dbus --name access_point --privileged --network host \
		-e WIFI_INTERFACE=wlp3s0 \
		-e WIFI_IP=192.168.10.1 \
		-e WIFI_SSID=train_network \
		-e WIFI_PASS=password123 \
		-e WIFI_CHANNEL=6 \
		-e DHCP_START=192.168.10.50 \
		-e DHCP_END=192.168.10.150 \
		access_point 
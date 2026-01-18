

setup:
	docker build -t ota_server .

run: 
	docker run -it --rm -p 192.168.10.1:443:443 --name ota_server ota_server

get-cert:
	docker cp ota_server:/etc/nginx/ssl/server.crt server_cert.pem

enter_server: 
	docker exec -it ota_server sh
